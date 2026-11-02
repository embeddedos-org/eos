// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file net.c
 * @brief EoS Networking ΓÇö Stub implementation
 */

#include <eos/net.h>
#include <string.h>

#include "mqtt_codec.h"

#if EOS_ENABLE_NET

static bool net_initialized = false;

/* The socket layer below is the freestanding fallback. On a host, ADR-014
 * point 3 maps this API onto POSIX sockets instead; CMake compiles
 * net_posix.c and defines EOS_NET_POSIX, and these stubs step aside so there
 * is exactly one definition of each symbol.
 *
 * Left returning -1 rather than deleted: on a target with no IP stack yet,
 * failing is the honest answer, and it is what ADR-014 records as the
 * Planned state for the Nano and Edge profiles until lwIP lands. */
#ifndef EOS_NET_POSIX

int eos_net_init(void)
{
    net_initialized = true;
    return 0;
}

void eos_net_deinit(void)
{
    net_initialized = false;
}

eos_socket_t eos_net_socket(eos_net_proto_t proto)
{
    if (!net_initialized) {
        return EOS_SOCKET_INVALID;
    }

    if (proto != EOS_NET_TCP && proto != EOS_NET_UDP) {
        return EOS_SOCKET_INVALID;
    }

    static int next_fd = 1;
    return (eos_socket_t)next_fd++;
}

int eos_net_connect(eos_socket_t sock, const eos_net_addr_t *addr)
{
    (void)sock; (void)addr;
    return -1;
}

int eos_net_bind(eos_socket_t sock, uint16_t port)
{
    (void)sock; (void)port;
    if (!net_initialized) return -1;
    return 0;
}

int eos_net_listen(eos_socket_t sock, int backlog)
{
    (void)sock; (void)backlog;
    if (!net_initialized) return -1;
    return 0;
}

eos_socket_t eos_net_accept(eos_socket_t sock, eos_net_addr_t *client_addr)
{
    (void)sock; (void)client_addr;
    return EOS_SOCKET_INVALID;
}

int eos_net_send(eos_socket_t sock, const void *data, size_t len)
{
    (void)sock; (void)data; (void)len;
    return -1;
}

int eos_net_recv(eos_socket_t sock, void *buf, size_t len, uint32_t timeout_ms)
{
    (void)sock; (void)buf; (void)len; (void)timeout_ms;
    return -1;
}

int eos_net_sendto(eos_socket_t sock, const void *data, size_t len,
                    const eos_net_addr_t *dest)
{
    (void)sock; (void)data; (void)len; (void)dest;
    return -1;
}

int eos_net_recvfrom(eos_socket_t sock, void *buf, size_t len,
                      eos_net_addr_t *src, uint32_t timeout_ms)
{
    (void)sock; (void)buf; (void)len; (void)src; (void)timeout_ms;
    return -1;
}

int eos_net_close(eos_socket_t sock)
{
    if (sock == EOS_SOCKET_INVALID) return -1;
    (void)sock;
    return 0;
}

int eos_net_resolve(const char *hostname, uint32_t *ip)
{
    (void)hostname; (void)ip;
    return -1;
}

/* ---- HTTP ---- */


#endif /* !EOS_NET_POSIX */

int eos_http_get(const char *url, eos_http_response_t *resp)
{
    (void)url;
    if (resp) memset(resp, 0, sizeof(*resp));
    return -1;
}

int eos_http_post(const char *url, const void *body, size_t body_len,
                   const char *content_type, eos_http_response_t *resp)
{
    (void)url; (void)body; (void)body_len; (void)content_type;
    if (resp) memset(resp, 0, sizeof(*resp));
    return -1;
}

void eos_http_response_free(eos_http_response_t *resp)
{
    if (resp) {
        resp->body = NULL;
        resp->body_len = 0;
    }
}

/* ---- MQTT ----
 *
 * #199 slice 2: the client state machine, written against the eos_net socket
 * API rather than POSIX directly, so the same code runs on the POSIX mapping
 * today and on lwIP when it lands (docs/eos-mqtt-client-design.md section
 * 3a). Every packet is built and parsed by mqtt_codec.c.
 *
 * Handles are indices into a static table, so there is no heap use. The
 * client is polled and promises no threads (design doc section 1), so the
 * table is not locked: connect/disconnect must not race from two threads. */

#ifndef EOS_MQTT_MAX_CLIENTS
#define EOS_MQTT_MAX_CLIENTS 2
#endif

/* MQTT 3.1.1 section 3.2 leaves "a reasonable amount of time" for CONNACK
 * to the client. */
#ifndef EOS_MQTT_CONNACK_TIMEOUT_MS
#define EOS_MQTT_CONNACK_TIMEOUT_MS 5000u
#endif

/* DISCONNECTED is 0 so the zero-initialised table starts with every slot
 * free and needs no init call. connect and disconnect are synchronous, so
 * CONNECTING and DISCONNECTING last only for the call; they keep the slot
 * claimed and stop a handle from being used mid-handshake. */
typedef enum {
    MQTT_DISCONNECTED = 0,
    MQTT_CONNECTING,
    MQTT_CONNECTED,
    MQTT_DISCONNECTING
} mqtt_state_t;

typedef struct {
    mqtt_state_t state;
    eos_socket_t sock;
} mqtt_client_t;

static mqtt_client_t mqtt_clients[EOS_MQTT_MAX_CLIENTS];

/* The config strings are fixed arrays the caller fills; one without a
 * terminator would send strlen() past the end of the struct. */
static bool mqtt_field_ok(const char *field, size_t size)
{
    return memchr(field, '\0', size) != NULL;
}

static int mqtt_send_all(eos_socket_t sock, const uint8_t *buf, size_t len)
{
    while (len > 0) {
        int n = eos_net_send(sock, buf, len);
        if (n <= 0) return -1;
        buf += n;
        len -= (size_t)n;
    }
    return 0;
}

/* eos_net_recv returns 0 for both a timeout and an orderly close. Either one
 * means the bytes are not coming, so both fail the read. Each call waits up
 * to timeout_ms, so a peer that trickles one byte at a time can stretch the
 * total wait to len * timeout_ms. That is still bounded. */
static int mqtt_recv_exact(eos_socket_t sock, uint8_t *buf, size_t len,
                           uint32_t timeout_ms)
{
    while (len > 0) {
        int n = eos_net_recv(sock, buf, len, timeout_ms);
        if (n <= 0) return -1;
        buf += n;
        len -= (size_t)n;
    }
    return 0;
}

static void mqtt_release(mqtt_client_t *c)
{
    if (c->sock != EOS_SOCKET_INVALID) eos_net_close(c->sock);
    c->sock = EOS_SOCKET_INVALID;
    c->state = MQTT_DISCONNECTED;
}

/* Open the TCP connection, send CONNECT, and wait for an accepting CONNACK.
 * On failure the caller releases the slot, which closes the socket. */
static int mqtt_handshake(mqtt_client_t *c, const eos_mqtt_config_t *cfg,
                          const uint8_t *connect_pkt, size_t connect_len)
{
    eos_net_addr_t addr;
    uint8_t ack_buf[4];
    eos_mqtt_connack_t ack;

    if (eos_net_resolve(cfg->broker_host, &addr.ip) != 0) return -1;
    addr.port = cfg->broker_port;

    c->sock = eos_net_socket(EOS_NET_TCP);
    if (c->sock == EOS_SOCKET_INVALID) return -1;
    if (eos_net_connect(c->sock, &addr) != 0) return -1;
    if (mqtt_send_all(c->sock, connect_pkt, connect_len) != 0) return -1;

    /* The first packet from the server MUST be CONNACK (MQTT-3.2.0-1), and a
     * CONNACK is always exactly 4 bytes. Read exactly that and let the codec
     * reject anything else. */
    if (mqtt_recv_exact(c->sock, ack_buf, sizeof(ack_buf),
                        EOS_MQTT_CONNACK_TIMEOUT_MS) != 0) {
        return -1;
    }
    if (eos_mqtt_decode_connack(ack_buf, sizeof(ack_buf), &ack) != EOS_MQTT_OK) {
        return -1;
    }
    /* A refusal (return codes 1-5) fails the connect, so the handle never
     * becomes valid (design doc section 4). */
    return ack.return_code == 0 ? 0 : -1;
}

eos_mqtt_handle_t eos_mqtt_connect(const eos_mqtt_config_t *cfg)
{
    if (!cfg || cfg->broker_port == 0) return EOS_MQTT_INVALID;
    if (!mqtt_field_ok(cfg->broker_host, sizeof(cfg->broker_host)) ||
        !mqtt_field_ok(cfg->client_id, sizeof(cfg->client_id)) ||
        !mqtt_field_ok(cfg->username, sizeof(cfg->username)) ||
        !mqtt_field_ok(cfg->password, sizeof(cfg->password))) {
        return EOS_MQTT_INVALID;
    }
    if (cfg->broker_host[0] == '\0') return EOS_MQTT_INVALID;

    /* Encode before claiming a slot or touching the network, so the codec
     * rejects a bad config (such as a password with no username,
     * MQTT-3.1.2-22) at no cost. The config struct has no other way to mark
     * a field unset, so an empty string means "not set". clean_session is
     * always on: the API cannot resume a session, so asking the broker to
     * keep one would only leave state behind on the broker.
     *
     * Largest CONNECT the config can produce:
     * 1 + 2 + 10 + 3 * (2 + 63) = 208 bytes. */
    const char *user = cfg->username[0] != '\0' ? cfg->username : NULL;
    const char *pass = cfg->password[0] != '\0' ? cfg->password : NULL;
    uint8_t pkt[256];
    int pkt_len = eos_mqtt_encode_connect(pkt, sizeof(pkt), cfg->client_id,
                                          user, pass, cfg->keepalive_sec,
                                          true);
    if (pkt_len < 0) return EOS_MQTT_INVALID;

    int idx = -1;
    for (int i = 0; i < EOS_MQTT_MAX_CLIENTS; i++) {
        if (mqtt_clients[i].state == MQTT_DISCONNECTED) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return EOS_MQTT_INVALID;

    mqtt_client_t *c = &mqtt_clients[idx];
    c->state = MQTT_CONNECTING;
    c->sock = EOS_SOCKET_INVALID;

    if (mqtt_handshake(c, cfg, pkt, (size_t)pkt_len) != 0) {
        mqtt_release(c);
        return EOS_MQTT_INVALID;
    }
    c->state = MQTT_CONNECTED;
    return (eos_mqtt_handle_t)idx;
}

int eos_mqtt_disconnect(eos_mqtt_handle_t handle)
{
    if (handle < 0 || handle >= EOS_MQTT_MAX_CLIENTS) return -1;
    mqtt_client_t *c = &mqtt_clients[handle];
    if (c->state != MQTT_CONNECTED) return -1;

    c->state = MQTT_DISCONNECTING;
    uint8_t pkt[2];
    int pkt_len = eos_mqtt_encode_disconnect(pkt, sizeof(pkt));
    int rc = (pkt_len > 0 &&
              mqtt_send_all(c->sock, pkt, (size_t)pkt_len) == 0) ? 0 : -1;

    /* The client MUST close the connection after DISCONNECT (MQTT-3.14.4-1).
     * Close and free the slot even when the send failed, because the
     * connection is unusable and keeping the slot would leak it. In that case
     * -1 tells the caller the broker may not have seen a clean disconnect. */
    mqtt_release(c);
    return rc;
}

/* TODO(#199 slice 3): publish, subscribe, unsubscribe and loop. Until then
 * they fail on every handle, connected or not. */

int eos_mqtt_publish(eos_mqtt_handle_t handle, const char *topic,
                      const void *payload, size_t len, uint8_t qos)
{
    (void)handle; (void)topic; (void)payload; (void)len; (void)qos;
    return -1;
}

int eos_mqtt_subscribe(eos_mqtt_handle_t handle, const char *topic,
                         uint8_t qos, eos_mqtt_msg_callback_t cb, void *ctx)
{
    (void)handle; (void)topic; (void)qos; (void)cb; (void)ctx;
    return -1;
}

int eos_mqtt_unsubscribe(eos_mqtt_handle_t handle, const char *topic)
{
    (void)handle; (void)topic;
    return -1;
}

int eos_mqtt_loop(eos_mqtt_handle_t handle, uint32_t timeout_ms)
{
    (void)handle; (void)timeout_ms;
    return -1;
}

/* ---- mDNS ---- */

int eos_mdns_init(void) { return -1; }
void eos_mdns_deinit(void) {}

int eos_mdns_register(const char *service_name, const char *service_type,
                       uint16_t port)
{
    (void)service_name; (void)service_type; (void)port;
    return -1;
}

int eos_mdns_resolve(const char *service_type, eos_net_addr_t *addr,
                      uint32_t timeout_ms)
{
    (void)service_type; (void)addr; (void)timeout_ms;
    return -1;
}

#endif /* EOS_ENABLE_NET */