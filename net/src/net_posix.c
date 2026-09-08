// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file net_posix.c
 * @brief eNet socket layer on POSIX sockets — the Compute profile.
 *
 * ADR-014 point 3: "On the Compute profile, where a host OS is present, the
 * same API maps to POSIX sockets." This is that mapping.
 *
 * Before this file, every function here was a stub — eos_net_connect() was
 * `return -1` and eos_net_socket() returned an incrementing integer that named
 * nothing. Anything built on eNet could not be exercised on any target,
 * including the host, which is where EoSim runs.
 *
 * Scope is deliberately the socket layer only. ADR-014 point 5 keeps MQTT,
 * CoAP and HTTP out: they are separate adoption decisions on top of a working
 * IP stack, and they stay in net.c until those decisions are made.
 *
 * This does not make eNet Implemented. lwIP is still the decision for the Nano
 * and Edge profiles (ADR-014 point 1), where there is no host OS to borrow
 * sockets from.
 */

#include "eos/net.h"

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

/* net.h wraps its declarations in #if EOS_ENABLE_NET, but CMake adds this
 * file to eos_net on every non-cross build regardless of that flag. With no
 * product profile and no test build nothing sets it, so the header expanded to
 * nothing while this file still defined the API against it -- `unknown type
 * name 'eos_net_addr_t'`, and the default configuration stopped compiling.
 * net.c already carries this guard; the POSIX mapping needs the same one so
 * both translation units appear and disappear together. */
#if EOS_ENABLE_NET

/* eos_socket_t is an int and EOS_SOCKET_INVALID is -1, which is exactly what
 * socket(2) returns on failure, so the fd is the handle. No table, no
 * translation, and no way for the two to drift. */

static int g_initialised;

static int to_sockaddr(const eos_net_addr_t *addr, struct sockaddr_in *out)
{
    if (!addr) return -1;
    memset(out, 0, sizeof(*out));
    out->sin_family = AF_INET;
    out->sin_port = htons(addr->port);
    /* eos_net_addr_t documents ip as network byte order, so it is copied
     * rather than converted. Calling htonl here would swap it twice. */
    out->sin_addr.s_addr = addr->ip;
    return 0;
}

static void from_sockaddr(const struct sockaddr_in *in, eos_net_addr_t *out)
{
    if (!out) return;
    out->ip = in->sin_addr.s_addr;
    out->port = ntohs(in->sin_port);
}

int eos_net_init(void)
{
    g_initialised = 1;
    return 0;
}

void eos_net_deinit(void)
{
    g_initialised = 0;
}

eos_socket_t eos_net_socket(eos_net_proto_t proto)
{
    if (!g_initialised) return EOS_SOCKET_INVALID;
    if (proto != EOS_NET_TCP && proto != EOS_NET_UDP) return EOS_SOCKET_INVALID;

    int type = (proto == EOS_NET_TCP) ? SOCK_STREAM : SOCK_DGRAM;
    int fd = socket(AF_INET, type, 0);
    if (fd < 0) return EOS_SOCKET_INVALID;

    /* Without SO_REUSEADDR a listener cannot rebind while the previous
     * socket sits in TIME_WAIT, which makes a restart fail for a minute for
     * no reason the caller can see. */
    int one = 1;
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    return fd;
}

int eos_net_connect(eos_socket_t sock, const eos_net_addr_t *addr)
{
    struct sockaddr_in sa;
    if (sock < 0 || to_sockaddr(addr, &sa) != 0) return -1;
    return connect(sock, (const struct sockaddr *)&sa, sizeof(sa)) == 0 ? 0 : -1;
}

int eos_net_bind(eos_socket_t sock, uint16_t port)
{
    if (sock < 0) return -1;
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(port);
    return bind(sock, (const struct sockaddr *)&sa, sizeof(sa)) == 0 ? 0 : -1;
}

int eos_net_listen(eos_socket_t sock, int backlog)
{
    if (sock < 0 || backlog < 0) return -1;
    return listen(sock, backlog) == 0 ? 0 : -1;
}

eos_socket_t eos_net_accept(eos_socket_t sock, eos_net_addr_t *client_addr)
{
    if (sock < 0) return EOS_SOCKET_INVALID;
    struct sockaddr_in sa;
    socklen_t len = sizeof(sa);
    int fd = accept(sock, (struct sockaddr *)&sa, &len);
    if (fd < 0) return EOS_SOCKET_INVALID;
    if (client_addr) from_sockaddr(&sa, client_addr);
    return fd;
}

int eos_net_send(eos_socket_t sock, const void *data, size_t len)
{
    if (sock < 0 || (!data && len > 0)) return -1;
    /* MSG_NOSIGNAL: writing to a peer that has gone away raises SIGPIPE by
     * default, which kills the process instead of returning an error the
     * caller can handle. */
    ssize_t n = send(sock, data, len, MSG_NOSIGNAL);
    return n < 0 ? -1 : (int)n;
}

int eos_net_recv(eos_socket_t sock, void *buf, size_t len, uint32_t timeout_ms)
{
    if (sock < 0 || (!buf && len > 0)) return -1;

    struct timeval tv;
    tv.tv_sec = (time_t)(timeout_ms / 1000u);
    tv.tv_usec = (suseconds_t)((timeout_ms % 1000u) * 1000u);
    /* A zero timeout means "block forever" to SO_RCVTIMEO, which is the
     * opposite of what a caller passing 0 expects. Only set it when non-zero. */
    if (timeout_ms > 0) {
        (void)setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    ssize_t n = recv(sock, buf, len, 0);
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;  /* timed out */
    return n < 0 ? -1 : (int)n;
}

int eos_net_sendto(eos_socket_t sock, const void *data, size_t len,
                   const eos_net_addr_t *addr)
{
    struct sockaddr_in sa;
    if (sock < 0 || (!data && len > 0) || to_sockaddr(addr, &sa) != 0) return -1;
    ssize_t n = sendto(sock, data, len, MSG_NOSIGNAL,
                       (const struct sockaddr *)&sa, sizeof(sa));
    return n < 0 ? -1 : (int)n;
}

int eos_net_recvfrom(eos_socket_t sock, void *buf, size_t len,
                     eos_net_addr_t *addr, uint32_t timeout_ms)
{
    if (sock < 0 || (!buf && len > 0)) return -1;

    if (timeout_ms > 0) {
        struct timeval tv;
        tv.tv_sec = (time_t)(timeout_ms / 1000u);
        tv.tv_usec = (suseconds_t)((timeout_ms % 1000u) * 1000u);
        (void)setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    struct sockaddr_in sa;
    socklen_t sl = sizeof(sa);
    ssize_t n = recvfrom(sock, buf, len, 0, (struct sockaddr *)&sa, &sl);
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
    if (n < 0) return -1;
    if (addr) from_sockaddr(&sa, addr);
    return (int)n;
}

int eos_net_close(eos_socket_t sock)
{
    if (sock < 0) return -1;
    return close(sock) == 0 ? 0 : -1;
}

int eos_net_resolve(const char *hostname, uint32_t *ip)
{
    if (!hostname || !ip) return -1;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;          /* eos_net_addr_t.ip is 32 bits */
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = NULL;
    if (getaddrinfo(hostname, NULL, &hints, &res) != 0 || !res) return -1;

    *ip = ((const struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr;
    freeaddrinfo(res);
    return 0;
}

#endif /* EOS_ENABLE_NET */
