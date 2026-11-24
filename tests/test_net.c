// SPDX-License-Identifier: MIT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define EOS_ENABLE_NET 1
#include "eos/eos_config.h"
#include "eos/net.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

/* Not assert(): CI builds Release, and NDEBUG deletes assert() together with
 * the expression inside it. Half these checks wrap the call under test --
 * assert(eos_net_connect(...) == 0) stopped connecting at all, the echo
 * thread blocked in accept() forever, and pthread_join() hung the suite. A
 * check macro must always evaluate its expression. */
#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while (0)

static void test_net_init(void) {
    CHECK(eos_net_init() == 0);
    eos_net_deinit();
    PASS("net init/deinit");
}
static void test_net_socket_tcp(void) {
    eos_net_init();
    eos_socket_t s = eos_net_socket(EOS_NET_TCP);
    CHECK(s != EOS_SOCKET_INVALID);
    eos_net_close(s);
    eos_net_deinit();
    PASS("net socket TCP");
}
static void test_net_socket_udp(void) {
    eos_net_init();
    eos_socket_t s = eos_net_socket(EOS_NET_UDP);
    CHECK(s != EOS_SOCKET_INVALID);
    eos_net_close(s);
    eos_net_deinit();
    PASS("net socket UDP");
}
static void test_net_socket_invalid_protocol(void) {
    eos_net_init();

    eos_socket_t s = eos_net_socket((eos_net_proto_t)99);

    CHECK(s == EOS_SOCKET_INVALID);

    eos_net_deinit();
    PASS("net socket invalid protocol");
}
static void test_net_bind(void) {
    eos_net_init();
    eos_socket_t s = eos_net_socket(EOS_NET_TCP);
    CHECK(eos_net_bind(s, 8080) == 0);
    eos_net_close(s);
    eos_net_deinit();
    PASS("net bind");
}
static void test_net_listen(void) {
    eos_net_init();
    eos_socket_t s = eos_net_socket(EOS_NET_TCP);
    eos_net_bind(s, 9090);
    CHECK(eos_net_listen(s, 5) == 0);
    eos_net_close(s);
    eos_net_deinit();
    PASS("net listen");
}
static void test_net_close(void) {
    eos_net_init();
    eos_socket_t s = eos_net_socket(EOS_NET_TCP);
    CHECK(eos_net_close(s) == 0);
    eos_net_deinit();
    PASS("net close");
}
static void test_net_close_invalid(void) {
    eos_net_init();
    CHECK(eos_net_close(EOS_SOCKET_INVALID) != 0);
    eos_net_deinit();
    PASS("net close invalid");
}
static void test_net_resolve(void) {
    eos_net_init();
    uint32_t ip = 0;
    int r = eos_net_resolve("localhost", &ip);
    (void)r;
    eos_net_deinit();
    PASS("net resolve (no crash)");
}
static void test_net_resolve_null(void) {
    eos_net_init();
    int r = eos_net_resolve(NULL, NULL);
    CHECK(r != 0);
    eos_net_deinit();
    PASS("net resolve null");
}
static void test_net_send(void) {
    eos_net_init();
    eos_socket_t s = eos_net_socket(EOS_NET_TCP);
    const char *msg = "hello";
    int r = eos_net_send(s, msg, 5);
    (void)r;
    eos_net_close(s);
    eos_net_deinit();
    PASS("net send (no crash)");
}
static void test_net_recv(void) {
    eos_net_init();
    eos_socket_t s = eos_net_socket(EOS_NET_TCP);
    char buf[32];
    int r = eos_net_recv(s, buf, sizeof(buf), 100);
    (void)r;
    eos_net_close(s);
    eos_net_deinit();
    PASS("net recv (no crash)");
}
static void test_http_get(void) {
    eos_net_init();
    eos_http_response_t resp;
    memset(&resp, 0, sizeof(resp));
    int r = eos_http_get("http://example.com", &resp);
    (void)r;
    eos_http_response_free(&resp);
    eos_net_deinit();
    PASS("http get (no crash)");
}
static void test_http_post(void) {
    eos_net_init();
    eos_http_response_t resp;
    memset(&resp, 0, sizeof(resp));
    int r = eos_http_post("http://example.com/api", "{}", 2, "application/json", &resp);
    (void)r;
    eos_http_response_free(&resp);
    eos_net_deinit();
    PASS("http post (no crash)");
}
static void test_mqtt_connect(void) {
    eos_net_init();
    eos_mqtt_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.broker_host, "localhost", sizeof(cfg.broker_host)-1);
    cfg.broker_port = 1883;
    strncpy(cfg.client_id, "test", sizeof(cfg.client_id)-1);
    cfg.keepalive_sec = 60;
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    if (h != EOS_MQTT_INVALID) {
        eos_mqtt_disconnect(h);
    }
    eos_net_deinit();
    PASS("mqtt connect (no crash)");
}
static void test_mdns_init(void) {
    eos_net_init();
    int r = eos_mdns_init();
    (void)r;
    eos_mdns_deinit();
    eos_net_deinit();
    PASS("mdns init/deinit");
}

/* ── POSIX socket layer (ADR-014 point 3) ─────────────────────────────────
 *
 * Everything above tests the facade's argument validation, which passed just
 * as well when eos_net_connect() was `return -1`. These exercise a real
 * connection, so they compile only where a host OS supplies the sockets.
 */
#ifdef EOS_NET_POSIX
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define EOS_TEST_NET_DESTINATION_ENV "EOS_TEST_NET_DESTINATION"
/* Distinct from CHECK()'s exit(1), so a rejected destination is never
 * mistaken for a failed check. tests/CMakeLists.txt matches the message. */
#define EOS_TEST_NET_DESTINATION_REJECTED 2

static uint16_t g_test_port = 45771;
static uint32_t g_test_destination_ip;

/* ip in network byte order. True if one of this host's interfaces has it. */
static int ipv4_on_local_interface(uint32_t ip) {
    struct ifaddrs *list = NULL;
    int found = 0;

    if (getifaddrs(&list) != 0) return 0;
    for (const struct ifaddrs *it = list; it && !found; it = it->ifa_next) {
        if (it->ifa_addr && it->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in sin;
            memcpy(&sin, it->ifa_addr, sizeof(sin));
            found = sin.sin_addr.s_addr == ip;
        }
    }
    freeifaddrs(list);
    return found;
}

static _Noreturn void reject_test_destination(const char *value, const char *reason) {
    fprintf(stderr, "[CONFIG] %s rejected: '%s' %s\n",
            EOS_TEST_NET_DESTINATION_ENV, value, reason);
    exit(EOS_TEST_NET_DESTINATION_REJECTED);
}

/* The override exists for hosts where 127.0.0.1 does not reach the local
 * listener (some WSL setups). It must stay on this machine: only
 * 127.0.0.0/8 or an address assigned to a local interface is accepted, so a
 * misconfigured runner cannot send test traffic to another host. Runs
 * before any other check, so a rejected value exits before any socket work. */
static void configure_test_destination(void) {
    const char *override = getenv(EOS_TEST_NET_DESTINATION_ENV);
    struct in_addr parsed;

    if (!override) {
        g_test_destination_ip = htonl(INADDR_LOOPBACK);
        return;
    }
    if (override[0] == '\0' || inet_pton(AF_INET, override, &parsed) != 1)
        reject_test_destination(override, "is not a numeric IPv4 address");
    if ((ntohl(parsed.s_addr) >> 24) != 127 && !ipv4_on_local_interface(parsed.s_addr))
        reject_test_destination(override,
                                "is not in 127.0.0.0/8 or assigned to a local interface");
    g_test_destination_ip = parsed.s_addr;
}

static void *echo_server(void *arg) {
    int *ready = (int *)arg;
    eos_socket_t s = eos_net_socket(EOS_NET_TCP);
    if (s == EOS_SOCKET_INVALID || eos_net_bind(s, g_test_port) != 0) {
        *ready = -1;
        return NULL;
    }
    eos_net_listen(s, 1);
    *ready = 1;
    eos_net_addr_t peer;
    eos_socket_t c = eos_net_accept(s, &peer);
    if (c != EOS_SOCKET_INVALID) {
        char buf[32];
        memset(buf, 0, sizeof(buf));
        int n = eos_net_recv(c, buf, sizeof(buf) - 1, 2000);
        if (n > 0) eos_net_send(c, buf, (size_t)n);
        eos_net_close(c);
    }
    eos_net_close(s);
    return NULL;
}

static void test_net_tcp_round_trip(void) {
    eos_net_init();
    int ready = 0;
    pthread_t t;
    pthread_create(&t, NULL, echo_server, &ready);
    /* Wait for the listener rather than sleeping a fixed interval, so this
     * does not go flaky on a loaded machine. */
    for (int i = 0; i < 400 && ready == 0; i++) {
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 5 * 1000 * 1000;
        nanosleep(&ts, NULL);
    }
    CHECK(ready == 1);
    eos_socket_t c = eos_net_socket(EOS_NET_TCP);
    eos_net_addr_t addr;
    addr.ip = g_test_destination_ip;
    addr.port = g_test_port;
    CHECK(eos_net_connect(c, &addr) == 0);
    CHECK(eos_net_send(c, "ping", 4) == 4);
    char buf[32];
    memset(buf, 0, sizeof(buf));
    CHECK(eos_net_recv(c, buf, sizeof(buf) - 1, 2000) == 4);
    CHECK(strcmp(buf, "ping") == 0);
    eos_net_close(c);
    pthread_join(t, NULL);
    eos_net_deinit();
    PASS("net tcp round trip");
}

static void test_net_resolve_localhost(void) {
    eos_net_init();
    uint32_t ip = 0;
    CHECK(eos_net_resolve("localhost", &ip) == 0);
    CHECK(ip == inet_addr("127.0.0.1"));
    /* A name that cannot resolve must fail rather than reporting success and
     * leaving the caller with whatever was in the output already. */
    uint32_t before = ip;
    CHECK(eos_net_resolve("no.such.host.invalid", &ip) == -1);
    CHECK(ip == before);
    eos_net_deinit();
    PASS("net resolve localhost");
}

static void test_net_connect_refused(void) {
    eos_net_init();
    eos_socket_t c = eos_net_socket(EOS_NET_TCP);
    eos_net_addr_t addr;
    addr.ip = inet_addr("127.0.0.1");
    addr.port = 1;              /* nothing listens on port 1 */
    CHECK(eos_net_connect(c, &addr) != 0);
    eos_net_close(c);
    eos_net_deinit();
    PASS("net connect to a closed port fails");
}

static double elapsed_ms(const struct timespec *a, const struct timespec *b) {
    return (double)(b->tv_sec - a->tv_sec) * 1000.0 +
           (double)(b->tv_nsec - a->tv_nsec) / 1e6;
}

/* timeout_ms == 0 is a poll: with nothing to read it returns 0 at once. It
 * used to block forever on a fresh socket, and on a socket an earlier call
 * gave a timeout it waited out that stale timeout. An alarm turns a
 * regression into a failure instead of a hung test. */
static void test_net_recv_zero_timeout_polls(void) {
    int sv[2];
    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    char buf[8];
    struct timespec t0, t1;

    alarm(5);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    CHECK(eos_net_recv(sv[0], buf, sizeof(buf), 0) == 0);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    CHECK(elapsed_ms(&t0, &t1) < 100.0);

    CHECK(eos_net_recv(sv[0], buf, sizeof(buf), 300) == 0);   /* times out */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    CHECK(eos_net_recv(sv[0], buf, sizeof(buf), 0) == 0);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    CHECK(elapsed_ms(&t0, &t1) < 100.0);

    CHECK(write(sv[1], "x", 1) == 1);
    CHECK(eos_net_recv(sv[0], buf, sizeof(buf), 0) == 1 && buf[0] == 'x');
    close(sv[1]);
    CHECK(eos_net_recv(sv[0], buf, sizeof(buf), 100) == EOS_NET_EOF);
    alarm(0);

    close(sv[0]);
    PASS("net recv with zero timeout polls");
}

static uint16_t g_broker_port = 45780;

static void test_net_udp_zero_length_datagram(void) {
    eos_net_init();
    eos_socket_t rx = eos_net_socket(EOS_NET_UDP);
    CHECK(rx != EOS_SOCKET_INVALID);
    uint16_t port = g_broker_port++;
    CHECK(eos_net_bind(rx, port) == 0);

    eos_socket_t tx = eos_net_socket(EOS_NET_UDP);
    CHECK(tx != EOS_SOCKET_INVALID);

    eos_net_addr_t dest;
    memset(&dest, 0, sizeof(dest));
    eos_net_resolve("127.0.0.1", &dest.ip);
    dest.port = port;

    /* Send 0-length datagram */
    char dummy = 0;
    CHECK(eos_net_sendto(tx, &dummy, 0, &dest) == 0);

    /* Receive 0-length datagram */
    char buf[16];
    eos_net_addr_t sender;
    memset(&sender, 0, sizeof(sender));
    int rc = eos_net_recvfrom(rx, buf, sizeof(buf), &sender, 500);
    CHECK(rc == 0); /* 0-length datagram, NOT EOS_NET_EOF */
    CHECK(sender.port != 0);
    CHECK(sender.ip == dest.ip);

    eos_net_close(rx);
    eos_net_close(tx);
    eos_net_deinit();
    PASS("net UDP zero length datagram preserves sender address");
}

/* ── MQTT client over a fake broker (#199 slice 2) ────────────────────────
 *
 * No broker exists in CI (design doc section 6), so a thread plays one: it
 * accepts a single client, records the CONNECT, answers with a scripted
 * CONNACK (or hangs up), then records everything the client sends until the
 * client closes. Raw recv() is used for that last part because eos_net_recv
 * returns 0 for both a timeout and a close, and the tests need to tell those
 * apart to prove the client closed its socket.
 */
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    int             ready;
} broker_sync_t;

static void broker_sync_init(broker_sync_t *s) {
    pthread_mutex_init(&s->mutex, NULL);
    pthread_cond_init(&s->cond, NULL);
    s->ready = 0;
}

static void broker_sync_destroy(broker_sync_t *s) {
    pthread_mutex_destroy(&s->mutex);
    pthread_cond_destroy(&s->cond);
}

static void broker_sync_set(broker_sync_t *s, int val) {
    pthread_mutex_lock(&s->mutex);
    s->ready = val;
    pthread_cond_broadcast(&s->cond);
    pthread_mutex_unlock(&s->mutex);
}

static int broker_sync_wait(broker_sync_t *s) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 5;
    pthread_mutex_lock(&s->mutex);
    while (s->ready == 0) {
        if (pthread_cond_timedwait(&s->cond, &s->mutex, &ts) != 0) {
            break;
        }
    }
    int val = s->ready;
    pthread_mutex_unlock(&s->mutex);
    return val;
}

typedef struct {
    uint16_t       port;
    const uint8_t *connack;         /* NULL: hang up after CONNECT */
    size_t         connack_len;
    broker_sync_t  sync;
    uint8_t        connect_pkt[256];
    size_t         connect_len;
    uint8_t        after[16];       /* bytes received after the CONNACK */
    size_t         after_len;
    int            saw_close;       /* client closed its end */
} fake_broker_t;

static int broker_recv_exact(int fd, uint8_t *buf, size_t len) {
    while (len > 0) {
        ssize_t n = recv(fd, buf, len, 0);
        if (n <= 0) return -1;
        buf += n;
        len -= (size_t)n;
    }
    return 0;
}

static void *fake_broker(void *arg) {
    fake_broker_t *b = (fake_broker_t *)arg;
    eos_socket_t s = eos_net_socket(EOS_NET_TCP);
    if (s == EOS_SOCKET_INVALID || eos_net_bind(s, b->port) != 0 ||
        eos_net_listen(s, 1) != 0) {
        broker_sync_set(&b->sync, -1);
        return NULL;
    }
    broker_sync_set(&b->sync, 1);
    eos_socket_t c = eos_net_accept(s, NULL);
    eos_net_close(s);
    if (c == EOS_SOCKET_INVALID) return NULL;

    struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    /* Every CONNECT in these tests is under 128 bytes, so Remaining Length
     * is the single byte at offset 1. */
    uint8_t *p = b->connect_pkt;
    if (broker_recv_exact(c, p, 2) == 0 && (p[1] & 0x80u) == 0 &&
        broker_recv_exact(c, p + 2, p[1]) == 0) {
        b->connect_len = 2u + p[1];
    }

    if (b->connect_len > 0 && b->connack != NULL) {
        eos_net_send(c, b->connack, b->connack_len);
        for (;;) {
            ssize_t n = recv(c, b->after + b->after_len,
                             sizeof(b->after) - b->after_len, 0);
            if (n == 0) { b->saw_close = 1; break; }
            if (n < 0 && errno == EINTR) continue;
            if (n < 0) break;
            b->after_len += (size_t)n;
            if (b->after_len == sizeof(b->after)) break;
        }
    }
    eos_net_close(c);
    return NULL;
}

static void broker_start(fake_broker_t *b, pthread_t *t,
                         const uint8_t *connack, size_t connack_len) {
    memset(b, 0, sizeof(*b));
    broker_sync_init(&b->sync);
    b->port = g_broker_port++;
    b->connack = connack;
    b->connack_len = connack_len;
    pthread_create(t, NULL, fake_broker, b);
    CHECK(broker_sync_wait(&b->sync) == 1);
}

static void mqtt_test_config(eos_mqtt_config_t *cfg, uint16_t port) {
    memset(cfg, 0, sizeof(*cfg));
    strncpy(cfg->broker_host, "127.0.0.1", sizeof(cfg->broker_host) - 1);
    cfg->broker_port = port;
    strncpy(cfg->client_id, "eos-test", sizeof(cfg->client_id) - 1);
    cfg->keepalive_sec = 60;
}

static const uint8_t k_connack_accepted[] = { 0x20, 0x02, 0x00, 0x00 };

/* CONNECT for client "eos-test", keep-alive 60, clean session, no
 * credentials: MQTT 3.1.1 sections 3.1.2-3.1.3. */
static const uint8_t k_connect_plain[] = {
    0x10, 0x14,
    0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04, 0x02, 0x00, 0x3C,
    0x00, 0x08, 'e', 'o', 's', '-', 't', 'e', 's', 't'
};

static void test_mqtt_connect_disconnect_lifecycle(void) {
    eos_net_init();
    fake_broker_t b;
    pthread_t t;
    broker_start(&b, &t, k_connack_accepted, sizeof(k_connack_accepted));

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, b.port);
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);
    CHECK(eos_mqtt_disconnect(h) == 0);
    pthread_join(t, NULL);

    CHECK(b.connect_len == sizeof(k_connect_plain));
    CHECK(memcmp(b.connect_pkt, k_connect_plain, sizeof(k_connect_plain)) == 0);
    /* DISCONNECT, then the socket closed (MQTT-3.14.4-1). */
    CHECK(b.after_len == 2);
    CHECK(b.after[0] == 0xE0 && b.after[1] == 0x00);
    CHECK(b.saw_close == 1);

    /* The handle is released: a second disconnect has nothing to close. */
    CHECK(eos_mqtt_disconnect(h) == -1);
    eos_net_deinit();
    PASS("mqtt connect/disconnect lifecycle");
}

static void test_mqtt_connect_credentials(void) {
    eos_net_init();
    fake_broker_t b;
    pthread_t t;
    broker_start(&b, &t, k_connack_accepted, sizeof(k_connack_accepted));

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, b.port);
    strncpy(cfg.username, "u", sizeof(cfg.username) - 1);
    strncpy(cfg.password, "p", sizeof(cfg.password) - 1);
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);
    CHECK(eos_mqtt_disconnect(h) == 0);
    pthread_join(t, NULL);

    /* Username and password flags set (0xC0) beside clean session (0x02),
     * and both fields follow the client id. */
    static const uint8_t expected[] = {
        0x10, 0x1A,
        0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04, 0xC2, 0x00, 0x3C,
        0x00, 0x08, 'e', 'o', 's', '-', 't', 'e', 's', 't',
        0x00, 0x01, 'u',
        0x00, 0x01, 'p'
    };
    CHECK(b.connect_len == sizeof(expected));
    CHECK(memcmp(b.connect_pkt, expected, sizeof(expected)) == 0);
    eos_net_deinit();
    PASS("mqtt connect sends credentials");
}

static void test_mqtt_reconnect_after_disconnect(void) {
    eos_net_init();
    for (int round = 0; round < 2; round++) {
        fake_broker_t b;
        pthread_t t;
        broker_start(&b, &t, k_connack_accepted, sizeof(k_connack_accepted));
        eos_mqtt_config_t cfg;
        mqtt_test_config(&cfg, b.port);
        eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
        CHECK(h != EOS_MQTT_INVALID);
        CHECK(eos_mqtt_disconnect(h) == 0);
        pthread_join(t, NULL);
        CHECK(b.saw_close == 1);
    }
    eos_net_deinit();
    PASS("mqtt reconnect after disconnect");
}

/* Every way the broker can fail to accept must leave no valid handle and a
 * closed socket. */
static void expect_connect_fails(const uint8_t *connack, size_t connack_len) {
    fake_broker_t b;
    pthread_t t;
    broker_start(&b, &t, connack, connack_len);
    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, b.port);
    CHECK(eos_mqtt_connect(&cfg) == EOS_MQTT_INVALID);
    pthread_join(t, NULL);
    CHECK(b.connect_len == sizeof(k_connect_plain));
    if (connack != NULL) {
        CHECK(b.after_len == 0);
        CHECK(b.saw_close == 1);
    }
}

static void test_mqtt_connect_broker_refuses(void) {
    eos_net_init();
    static const uint8_t refused[] = { 0x20, 0x02, 0x00, 0x05 };  /* not authorized */
    expect_connect_fails(refused, sizeof(refused));
    eos_net_deinit();
    PASS("mqtt connect refused by broker");
}

static void test_mqtt_connect_malformed_connack(void) {
    eos_net_init();
    static const uint8_t bad_code[] = { 0x20, 0x02, 0x00, 0x06 };  /* code > 5 */
    static const uint8_t not_ack[]  = { 0xD0, 0x00, 0x20, 0x02 };  /* PINGRESP first */
    expect_connect_fails(bad_code, sizeof(bad_code));
    expect_connect_fails(not_ack, sizeof(not_ack));
    eos_net_deinit();
    PASS("mqtt connect rejects a malformed CONNACK");
}

static void test_mqtt_connect_broker_hangs_up(void) {
    eos_net_init();
    expect_connect_fails(NULL, 0);
    eos_net_deinit();
    PASS("mqtt connect fails when broker hangs up");
}

static void test_mqtt_connect_no_listener(void) {
    eos_net_init();
    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, 1);      /* nothing listens on port 1 */
    CHECK(eos_mqtt_connect(&cfg) == EOS_MQTT_INVALID);
    eos_net_deinit();
    PASS("mqtt connect to a closed port fails");
}

static void test_mqtt_connect_bad_config(void) {
    eos_net_init();
    eos_mqtt_config_t cfg;
    CHECK(eos_mqtt_connect(NULL) == EOS_MQTT_INVALID);

    mqtt_test_config(&cfg, 1883);
    cfg.broker_host[0] = '\0';
    CHECK(eos_mqtt_connect(&cfg) == EOS_MQTT_INVALID);

    mqtt_test_config(&cfg, 0);
    CHECK(eos_mqtt_connect(&cfg) == EOS_MQTT_INVALID);

    /* No terminator anywhere in the field. */
    mqtt_test_config(&cfg, 1883);
    memset(cfg.client_id, 'a', sizeof(cfg.client_id));
    CHECK(eos_mqtt_connect(&cfg) == EOS_MQTT_INVALID);
    eos_net_deinit();
    PASS("mqtt connect rejects a bad config");
}

static void test_mqtt_disconnect_bad_handle(void) {
    CHECK(eos_mqtt_disconnect(EOS_MQTT_INVALID) == -1);
    CHECK(eos_mqtt_disconnect(0) == -1);        /* never connected */
    CHECK(eos_mqtt_disconnect(9999) == -1);     /* out of range */
    PASS("mqtt disconnect rejects a bad handle");
}

/* ── MQTT Publish / Subscribe / Loop tests (#199 slice 3 / #259) ───────── */

static eos_socket_t broker_listen_ready(uint16_t port, broker_sync_t *sync) {
    eos_socket_t s = eos_net_socket(EOS_NET_TCP);
    if (s == EOS_SOCKET_INVALID || eos_net_bind(s, port) != 0 ||
        eos_net_listen(s, 1) != 0) {
        broker_sync_set(sync, -1);
        return EOS_SOCKET_INVALID;
    }
    broker_sync_set(sync, 1);
    return s;
}

static void wait_broker_ready(broker_sync_t *sync) {
    CHECK(broker_sync_wait(sync) == 1);
}

static eos_socket_t broker_accept_and_connack(eos_socket_t listener) {
    eos_socket_t c = eos_net_accept(listener, NULL);
    eos_net_close(listener);
    if (c == EOS_SOCKET_INVALID) return EOS_SOCKET_INVALID;
    uint8_t conn_pkt[128];
    if (broker_recv_exact(c, conn_pkt, 2) != 0) {
        eos_net_close(c);
        return EOS_SOCKET_INVALID;
    }
    if ((conn_pkt[1] & 0x80u) != 0 || broker_recv_exact(c, conn_pkt + 2, conn_pkt[1]) != 0) {
        eos_net_close(c);
        return EOS_SOCKET_INVALID;
    }
    if (eos_net_send(c, k_connack_accepted, sizeof(k_connack_accepted)) != (int)sizeof(k_connack_accepted)) {
        eos_net_close(c);
        return EOS_SOCKET_INVALID;
    }
    return c;
}

static void dummy_cb(const char *topic, const uint8_t *payload, size_t len, void *ctx) {
    (void)topic; (void)payload; (void)len; (void)ctx;
}

typedef struct {
    int call_count;
    char topic[128];
    char payload[128];
    size_t len;
} cb_record_t;

static void test_msg_cb(const char *topic, const uint8_t *payload, size_t len, void *ctx) {
    cb_record_t *rec = (cb_record_t *)ctx;
    rec->call_count++;
    strncpy(rec->topic, topic, sizeof(rec->topic) - 1);
    rec->topic[sizeof(rec->topic) - 1] = '\0';
    size_t copy_len = len < sizeof(rec->payload) - 1 ? len : sizeof(rec->payload) - 1;
    memcpy(rec->payload, payload, copy_len);
    rec->payload[copy_len] = '\0';
    rec->len = len;
}

typedef struct {
    uint16_t port;
    broker_sync_t sync;
    uint8_t pkt[128];
    size_t pkt_len;
} pub_broker_ctx_t;

static void *broker_pub_qos0(void *arg) {
    pub_broker_ctx_t *ctx = (pub_broker_ctx_t *)arg;
    eos_socket_t s = broker_listen_ready(ctx->port, &ctx->sync);
    if (s == EOS_SOCKET_INVALID) return NULL;
    eos_socket_t c = broker_accept_and_connack(s);
    if (c == EOS_SOCKET_INVALID) return NULL;

    uint8_t hdr[2];
    if (broker_recv_exact(c, hdr, 2) == 0) {
        ctx->pkt[0] = hdr[0];
        ctx->pkt[1] = hdr[1];
        if (broker_recv_exact(c, ctx->pkt + 2, hdr[1]) == 0) {
            ctx->pkt_len = 2 + hdr[1];
        }
    }
    uint8_t disc[2];
    (void)broker_recv_exact(c, disc, 2);
    eos_net_close(c);
    return NULL;
}

static void test_mqtt_publish_qos0(void) {
    eos_net_init();
    pub_broker_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    broker_sync_init(&ctx.sync);
    ctx.port = g_broker_port++;
    pthread_t t;
    pthread_create(&t, NULL, broker_pub_qos0, &ctx);
    wait_broker_ready(&ctx.sync);

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, ctx.port);
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);

    const char *topic = "sensor/temp";
    const char *payload = "21.5";
    CHECK(eos_mqtt_publish(h, topic, payload, strlen(payload), 0) == 0);
    CHECK(eos_mqtt_disconnect(h) == 0);
    pthread_join(t, NULL);
    broker_sync_destroy(&ctx.sync);

    CHECK(ctx.pkt_len > 0);
    CHECK((ctx.pkt[0] & 0xFE) == 0x30); /* PUBLISH QoS 0 */
    CHECK(ctx.pkt[2] == 0x00 && ctx.pkt[3] == (uint8_t)strlen(topic));
    CHECK(memcmp(ctx.pkt + 4, topic, strlen(topic)) == 0);
    CHECK(memcmp(ctx.pkt + 4 + strlen(topic), payload, strlen(payload)) == 0);

    eos_net_deinit();
    PASS("mqtt publish QoS 0");
}

static void *broker_pub_qos1(void *arg) {
    pub_broker_ctx_t *ctx = (pub_broker_ctx_t *)arg;
    eos_socket_t s = broker_listen_ready(ctx->port, &ctx->sync);
    if (s == EOS_SOCKET_INVALID) return NULL;
    eos_socket_t c = broker_accept_and_connack(s);
    if (c == EOS_SOCKET_INVALID) return NULL;

    uint8_t hdr[2];
    if (broker_recv_exact(c, hdr, 2) == 0) {
        ctx->pkt[0] = hdr[0];
        ctx->pkt[1] = hdr[1];
        if (broker_recv_exact(c, ctx->pkt + 2, hdr[1]) == 0) {
            ctx->pkt_len = 2 + hdr[1];
            uint16_t tlen = (uint16_t)((ctx->pkt[2] << 8) | ctx->pkt[3]);
            size_t pid_offset = 4 + tlen;
            uint16_t pid = (uint16_t)((ctx->pkt[pid_offset] << 8) | ctx->pkt[pid_offset + 1]);
            uint8_t puback[4] = { 0x40, 0x02, (uint8_t)(pid >> 8), (uint8_t)(pid & 0xFF) };
            eos_net_send(c, puback, sizeof(puback));
        }
    }
    uint8_t disc[2];
    (void)broker_recv_exact(c, disc, 2);
    eos_net_close(c);
    return NULL;
}

static void test_mqtt_publish_qos1_puback(void) {
    eos_net_init();
    pub_broker_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    broker_sync_init(&ctx.sync);
    ctx.port = g_broker_port++;
    pthread_t t;
    pthread_create(&t, NULL, broker_pub_qos1, &ctx);
    wait_broker_ready(&ctx.sync);

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, ctx.port);
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);

    const char *topic = "sensor/temp";
    const char *payload = "21.5";
    CHECK(eos_mqtt_publish(h, topic, payload, strlen(payload), 1) == 0);
    CHECK(eos_mqtt_disconnect(h) == 0);
    pthread_join(t, NULL);
    broker_sync_destroy(&ctx.sync);

    CHECK(ctx.pkt_len > 0);
    CHECK((ctx.pkt[0] & 0x06) == 0x02); /* QoS 1 */
    eos_net_deinit();
    PASS("mqtt publish QoS 1 and PUBACK");
}

typedef struct {
    uint16_t port;
    broker_sync_t sync;
    uint8_t pkt[128];
    size_t pkt_len;
} sub_broker_ctx_t;

static void *broker_sub(void *arg) {
    sub_broker_ctx_t *ctx = (sub_broker_ctx_t *)arg;
    eos_socket_t s = broker_listen_ready(ctx->port, &ctx->sync);
    if (s == EOS_SOCKET_INVALID) return NULL;
    eos_socket_t c = broker_accept_and_connack(s);
    if (c == EOS_SOCKET_INVALID) return NULL;

    uint8_t hdr[2];
    if (broker_recv_exact(c, hdr, 2) == 0) {
        ctx->pkt[0] = hdr[0];
        ctx->pkt[1] = hdr[1];
        if (broker_recv_exact(c, ctx->pkt + 2, hdr[1]) == 0) {
            ctx->pkt_len = 2 + hdr[1];
            uint16_t pid = (uint16_t)((ctx->pkt[2] << 8) | ctx->pkt[3]);
            uint8_t suback[5] = { 0x90, 0x03, (uint8_t)(pid >> 8), (uint8_t)(pid & 0xFF), 0x00 };
            eos_net_send(c, suback, sizeof(suback));
        }
    }
    uint8_t disc[2];
    (void)broker_recv_exact(c, disc, 2);
    eos_net_close(c);
    return NULL;
}

static void test_mqtt_subscribe_suback(void) {
    eos_net_init();
    sub_broker_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    broker_sync_init(&ctx.sync);
    ctx.port = g_broker_port++;
    pthread_t t;
    pthread_create(&t, NULL, broker_sub, &ctx);
    wait_broker_ready(&ctx.sync);

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, ctx.port);
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);

    CHECK(eos_mqtt_subscribe(h, "sensor/+", 0, dummy_cb, NULL) == 0);
    CHECK(eos_mqtt_disconnect(h) == 0);
    pthread_join(t, NULL);
    broker_sync_destroy(&ctx.sync);

    CHECK(ctx.pkt_len > 0);
    CHECK(ctx.pkt[0] == 0x82); /* SUBSCRIBE */
    eos_net_deinit();
    PASS("mqtt subscribe and SUBACK");
}

static void *broker_incoming_pub(void *arg) {
    sub_broker_ctx_t *ctx = (sub_broker_ctx_t *)arg;
    eos_socket_t s = broker_listen_ready(ctx->port, &ctx->sync);
    if (s == EOS_SOCKET_INVALID) return NULL;
    eos_socket_t c = broker_accept_and_connack(s);
    if (c == EOS_SOCKET_INVALID) return NULL;

    uint8_t hdr[2];
    if (broker_recv_exact(c, hdr, 2) == 0 && broker_recv_exact(c, ctx->pkt + 2, hdr[1]) == 0) {
        uint16_t pid = (uint16_t)((ctx->pkt[2] << 8) | ctx->pkt[3]);
        uint8_t suback[5] = { 0x90, 0x03, (uint8_t)(pid >> 8), (uint8_t)(pid & 0xFF), 0x00 };
        eos_net_send(c, suback, sizeof(suback));

        const char *t = "sensor/humidity";
        const char *p = "55%";
        uint8_t pub_pkt[64];
        pub_pkt[0] = 0x30;
        pub_pkt[1] = (uint8_t)(2 + strlen(t) + strlen(p));
        pub_pkt[2] = 0;
        pub_pkt[3] = (uint8_t)strlen(t);
        memcpy(pub_pkt + 4, t, strlen(t));
        memcpy(pub_pkt + 4 + strlen(t), p, strlen(p));
        eos_net_send(c, pub_pkt, 2 + pub_pkt[1]);
    }
    uint8_t disc[2];
    (void)broker_recv_exact(c, disc, 2);
    eos_net_close(c);
    return NULL;
}

static void test_mqtt_incoming_publish(void) {
    eos_net_init();
    sub_broker_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    broker_sync_init(&ctx.sync);
    ctx.port = g_broker_port++;
    pthread_t t;
    pthread_create(&t, NULL, broker_incoming_pub, &ctx);
    wait_broker_ready(&ctx.sync);

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, ctx.port);
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);

    cb_record_t rec;
    memset(&rec, 0, sizeof(rec));
    CHECK(eos_mqtt_subscribe(h, "sensor/#", 0, test_msg_cb, &rec) == 0);
    CHECK(eos_mqtt_loop(h, 1000) == 0);
    CHECK(rec.call_count == 1);
    CHECK(strcmp(rec.topic, "sensor/humidity") == 0);
    CHECK(strcmp(rec.payload, "55%") == 0);
    CHECK(rec.len == 3);

    CHECK(eos_mqtt_disconnect(h) == 0);
    pthread_join(t, NULL);
    broker_sync_destroy(&ctx.sync);
    eos_net_deinit();
    PASS("mqtt incoming PUBLISH callback delivery");
}

typedef struct {
    uint16_t port;
    broker_sync_t sync;
    uint8_t puback[4];
    size_t puback_len;
} puback_broker_ctx_t;

static void *broker_incoming_qos1(void *arg) {
    puback_broker_ctx_t *ctx = (puback_broker_ctx_t *)arg;
    eos_socket_t s = broker_listen_ready(ctx->port, &ctx->sync);
    if (s == EOS_SOCKET_INVALID) return NULL;
    eos_socket_t c = broker_accept_and_connack(s);
    if (c == EOS_SOCKET_INVALID) return NULL;

    uint8_t sub_hdr[2];
    uint8_t sub_body[64];
    if (broker_recv_exact(c, sub_hdr, 2) == 0 && broker_recv_exact(c, sub_body, sub_hdr[1]) == 0) {
        uint16_t pid = (uint16_t)((sub_body[0] << 8) | sub_body[1]);
        uint8_t suback[5] = { 0x90, 0x03, (uint8_t)(pid >> 8), (uint8_t)(pid & 0xFF), 0x01 };
        eos_net_send(c, suback, sizeof(suback));

        const char *t = "cmd/light";
        const char *p = "ON";
        uint8_t pub_pkt[64];
        pub_pkt[0] = 0x32; /* QoS 1 */
        pub_pkt[1] = (uint8_t)(2 + strlen(t) + 2 + strlen(p));
        pub_pkt[2] = 0;
        pub_pkt[3] = (uint8_t)strlen(t);
        memcpy(pub_pkt + 4, t, strlen(t));
        size_t off = 4 + strlen(t);
        pub_pkt[off++] = 0x12;
        pub_pkt[off++] = 0x34;
        memcpy(pub_pkt + off, p, strlen(p));
        off += strlen(p);
        eos_net_send(c, pub_pkt, off);

        if (broker_recv_exact(c, ctx->puback, 4) == 0) {
            ctx->puback_len = 4;
        }
    }
    uint8_t disc[2];
    (void)broker_recv_exact(c, disc, 2);
    eos_net_close(c);
    return NULL;
}

static void test_mqtt_incoming_qos1_puback(void) {
    eos_net_init();
    puback_broker_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    broker_sync_init(&ctx.sync);
    ctx.port = g_broker_port++;
    pthread_t t;
    pthread_create(&t, NULL, broker_incoming_qos1, &ctx);
    wait_broker_ready(&ctx.sync);

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, ctx.port);
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);

    cb_record_t rec;
    memset(&rec, 0, sizeof(rec));
    CHECK(eos_mqtt_subscribe(h, "cmd/light", 1, test_msg_cb, &rec) == 0);
    CHECK(eos_mqtt_loop(h, 1000) == 0);
    CHECK(rec.call_count == 1);
    CHECK(strcmp(rec.topic, "cmd/light") == 0);
    CHECK(strcmp(rec.payload, "ON") == 0);

    CHECK(eos_mqtt_disconnect(h) == 0);
    pthread_join(t, NULL);
    broker_sync_destroy(&ctx.sync);

    CHECK(ctx.puback_len == 4);
    CHECK(ctx.puback[0] == 0x40);
    CHECK(ctx.puback[2] == 0x12 && ctx.puback[3] == 0x34);

    eos_net_deinit();
    PASS("mqtt incoming QoS 1 and PUBACK response");
}

static void *broker_unsub(void *arg) {
    sub_broker_ctx_t *ctx = (sub_broker_ctx_t *)arg;
    eos_socket_t s = broker_listen_ready(ctx->port, &ctx->sync);
    if (s == EOS_SOCKET_INVALID) return NULL;
    eos_socket_t c = broker_accept_and_connack(s);
    if (c == EOS_SOCKET_INVALID) return NULL;

    uint8_t hdr[2];
    uint8_t body[64];
    if (broker_recv_exact(c, hdr, 2) == 0 && broker_recv_exact(c, body, hdr[1]) == 0) {
        uint16_t pid = (uint16_t)((body[0] << 8) | body[1]);
        uint8_t suback[5] = { 0x90, 0x03, (uint8_t)(pid >> 8), (uint8_t)(pid & 0xFF), 0x00 };
        eos_net_send(c, suback, sizeof(suback));

        if (broker_recv_exact(c, hdr, 2) == 0 && broker_recv_exact(c, body, hdr[1]) == 0) {
            uint16_t upid = (uint16_t)((body[0] << 8) | body[1]);
            uint8_t unsuback[4] = { 0xB0, 0x02, (uint8_t)(upid >> 8), (uint8_t)(upid & 0xFF) };
            eos_net_send(c, unsuback, sizeof(unsuback));

            const char *t = "sensor/temp";
            const char *p = "99.9";
            uint8_t pub_pkt[64];
            pub_pkt[0] = 0x30;
            pub_pkt[1] = (uint8_t)(2 + strlen(t) + strlen(p));
            pub_pkt[2] = 0;
            pub_pkt[3] = (uint8_t)strlen(t);
            memcpy(pub_pkt + 4, t, strlen(t));
            memcpy(pub_pkt + 4 + strlen(t), p, strlen(p));
            eos_net_send(c, pub_pkt, 2 + pub_pkt[1]);
        }
    }
    uint8_t disc[2];
    (void)broker_recv_exact(c, disc, 2);
    eos_net_close(c);
    return NULL;
}

static void test_mqtt_unsubscribe_unsuback(void) {
    eos_net_init();
    sub_broker_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    broker_sync_init(&ctx.sync);
    ctx.port = g_broker_port++;
    pthread_t t;
    pthread_create(&t, NULL, broker_unsub, &ctx);
    wait_broker_ready(&ctx.sync);

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, ctx.port);
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);

    cb_record_t rec;
    memset(&rec, 0, sizeof(rec));
    CHECK(eos_mqtt_subscribe(h, "sensor/temp", 0, test_msg_cb, &rec) == 0);
    CHECK(eos_mqtt_unsubscribe(h, "sensor/temp") == 0);

    /* Message received after unsubscribe must not trigger callback */
    CHECK(eos_mqtt_loop(h, 200) == 0);
    CHECK(rec.call_count == 0);

    CHECK(eos_mqtt_disconnect(h) == 0);
    pthread_join(t, NULL);
    broker_sync_destroy(&ctx.sync);
    eos_net_deinit();
    PASS("mqtt unsubscribe and UNSUBACK");
}

static void test_mqtt_invalid_handles_and_args(void) {
    eos_net_init();
    /* Invalid handles */
    CHECK(eos_mqtt_publish(EOS_MQTT_INVALID, "topic", "data", 4, 0) == -1);
    CHECK(eos_mqtt_publish(0, "topic", "data", 4, 0) == -1);
    CHECK(eos_mqtt_publish(999, "topic", "data", 4, 0) == -1);

    CHECK(eos_mqtt_subscribe(EOS_MQTT_INVALID, "topic", 0, dummy_cb, NULL) == -1);
    CHECK(eos_mqtt_subscribe(0, "topic", 0, dummy_cb, NULL) == -1);
    CHECK(eos_mqtt_subscribe(999, "topic", 0, dummy_cb, NULL) == -1);

    CHECK(eos_mqtt_unsubscribe(EOS_MQTT_INVALID, "topic") == -1);
    CHECK(eos_mqtt_unsubscribe(0, "topic") == -1);
    CHECK(eos_mqtt_unsubscribe(999, "topic") == -1);

    CHECK(eos_mqtt_loop(EOS_MQTT_INVALID, 100) == -1);
    CHECK(eos_mqtt_loop(0, 100) == -1);
    CHECK(eos_mqtt_loop(999, 100) == -1);

    /* Connected handle with invalid arguments */
    fake_broker_t b;
    pthread_t t;
    broker_start(&b, &t, k_connack_accepted, sizeof(k_connack_accepted));
    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, b.port);
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);

    /* Invalid publish arguments */
    CHECK(eos_mqtt_publish(h, NULL, "data", 4, 0) == -1);
    CHECK(eos_mqtt_publish(h, "", "data", 4, 0) == -1);
    CHECK(eos_mqtt_publish(h, "topic/+", "data", 4, 0) == -1);
    CHECK(eos_mqtt_publish(h, "topic/#", "data", 4, 0) == -1);
    CHECK(eos_mqtt_publish(h, "topic", NULL, 4, 0) == -1);
    CHECK(eos_mqtt_publish(h, "sensor\x80temp", "data", 4, 0) == -1);

    /* Invalid subscribe arguments */
    CHECK(eos_mqtt_subscribe(h, NULL, 0, dummy_cb, NULL) == -1);
    CHECK(eos_mqtt_subscribe(h, "", 0, dummy_cb, NULL) == -1);
    CHECK(eos_mqtt_subscribe(h, "topic", 0, NULL, NULL) == -1);
    CHECK(eos_mqtt_subscribe(h, "sport/tennis#", 0, dummy_cb, NULL) == -1);
    CHECK(eos_mqtt_subscribe(h, "sport/#/extra", 0, dummy_cb, NULL) == -1);
    CHECK(eos_mqtt_subscribe(h, "sport+", 0, dummy_cb, NULL) == -1);
    CHECK(eos_mqtt_subscribe(h, "sensor\x80temp", 0, dummy_cb, NULL) == -1);

    /* Invalid unsubscribe arguments */
    CHECK(eos_mqtt_unsubscribe(h, NULL) == -1);
    CHECK(eos_mqtt_unsubscribe(h, "") == -1);
    CHECK(eos_mqtt_unsubscribe(h, "sport/tennis#") == -1);
    CHECK(eos_mqtt_unsubscribe(h, "sensor\x80temp") == -1);

    CHECK(eos_mqtt_disconnect(h) == 0);
    pthread_join(t, NULL);
    broker_sync_destroy(&b.sync);

    /* After disconnect, handle is invalid */
    CHECK(eos_mqtt_publish(h, "topic", "data", 4, 0) == -1);
    CHECK(eos_mqtt_subscribe(h, "topic", 0, dummy_cb, NULL) == -1);
    CHECK(eos_mqtt_unsubscribe(h, "topic") == -1);
    CHECK(eos_mqtt_loop(h, 100) == -1);

    eos_net_deinit();
    PASS("mqtt invalid handles and arguments");
}

static void *broker_send_qos2_publish(void *arg) {
    sub_broker_ctx_t *ctx = (sub_broker_ctx_t *)arg;
    eos_socket_t s = broker_listen_ready(ctx->port, &ctx->sync);
    if (s == EOS_SOCKET_INVALID) return NULL;
    eos_socket_t c = broker_accept_and_connack(s);
    if (c == EOS_SOCKET_INVALID) return NULL;

    const char *t = "sensor/temp";
    const char *p = "data";
    uint8_t pub_pkt[64];
    pub_pkt[0] = 0x34; /* QoS 2 */
    pub_pkt[1] = (uint8_t)(2 + strlen(t) + 2 + strlen(p));
    pub_pkt[2] = 0;
    pub_pkt[3] = (uint8_t)strlen(t);
    memcpy(pub_pkt + 4, t, strlen(t));
    pub_pkt[4 + strlen(t)] = 0x00;
    pub_pkt[5 + strlen(t)] = 0x01;
    memcpy(pub_pkt + 6 + strlen(t), p, strlen(p));
    eos_net_send(c, pub_pkt, 2 + pub_pkt[1]);

    uint8_t buf[8];
    (void)broker_recv_exact(c, buf, sizeof(buf));
    eos_net_close(c);
    return NULL;
}

static void test_mqtt_unsupported_qos(void) {
    eos_net_init();
    sub_broker_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    broker_sync_init(&ctx.sync);
    ctx.port = g_broker_port++;
    pthread_t t;
    pthread_create(&t, NULL, broker_send_qos2_publish, &ctx);
    wait_broker_ready(&ctx.sync);

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, ctx.port);
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);

    CHECK(eos_mqtt_publish(h, "topic", "data", 4, 2) == -1);
    CHECK(eos_mqtt_publish(h, "topic", "data", 4, 3) == -1);
    CHECK(eos_mqtt_subscribe(h, "topic", 2, dummy_cb, NULL) == -1);
    CHECK(eos_mqtt_subscribe(h, "topic", 3, dummy_cb, NULL) == -1);

    /* Inbound QoS 2 packet must fail the connection */
    CHECK(eos_mqtt_loop(h, 1000) == -1);
    CHECK(eos_mqtt_publish(h, "topic", "data", 4, 0) == -1);

    pthread_join(t, NULL);
    broker_sync_destroy(&ctx.sync);
    eos_net_deinit();
    PASS("mqtt unsupported QoS rejected");
}

static void *broker_send_malformed(void *arg) {
    sub_broker_ctx_t *ctx = (sub_broker_ctx_t *)arg;
    eos_socket_t s = broker_listen_ready(ctx->port, &ctx->sync);
    if (s == EOS_SOCKET_INVALID) return NULL;
    eos_socket_t c = broker_accept_and_connack(s);
    if (c == EOS_SOCKET_INVALID) return NULL;

    uint8_t malformed[] = { 0x30, 0x80, 0x80, 0x80, 0x80, 0x01 };
    eos_net_send(c, malformed, sizeof(malformed));
    uint8_t buf[8];
    (void)broker_recv_exact(c, buf, sizeof(buf));
    eos_net_close(c);
    return NULL;
}

static void test_mqtt_malformed_packet(void) {
    eos_net_init();
    sub_broker_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    broker_sync_init(&ctx.sync);
    ctx.port = g_broker_port++;
    pthread_t t;
    pthread_create(&t, NULL, broker_send_malformed, &ctx);
    wait_broker_ready(&ctx.sync);

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, ctx.port);
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);

    CHECK(eos_mqtt_loop(h, 1000) == -1);

    pthread_join(t, NULL);
    broker_sync_destroy(&ctx.sync);
    eos_net_deinit();
    PASS("mqtt malformed packets rejected");
}

static void *broker_silent(void *arg) {
    sub_broker_ctx_t *ctx = (sub_broker_ctx_t *)arg;
    eos_socket_t s = broker_listen_ready(ctx->port, &ctx->sync);
    if (s == EOS_SOCKET_INVALID) return NULL;
    eos_socket_t c = broker_accept_and_connack(s);
    if (c == EOS_SOCKET_INVALID) return NULL;
    uint8_t disc[2];
    (void)broker_recv_exact(c, disc, 2);
    eos_net_close(c);
    return NULL;
}

static void test_mqtt_loop_timeout(void) {
    eos_net_init();
    sub_broker_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    broker_sync_init(&ctx.sync);
    ctx.port = g_broker_port++;
    pthread_t t;
    pthread_create(&t, NULL, broker_silent, &ctx);
    wait_broker_ready(&ctx.sync);

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, ctx.port);
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    CHECK(eos_mqtt_loop(h, 60) == 0);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    CHECK(elapsed_ms(&t0, &t1) >= 40.0);

    clock_gettime(CLOCK_MONOTONIC, &t0);
    CHECK(eos_mqtt_loop(h, 0) == 0);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    CHECK(elapsed_ms(&t0, &t1) < 50.0);

    CHECK(eos_mqtt_disconnect(h) == 0);
    pthread_join(t, NULL);
    broker_sync_destroy(&ctx.sync);
    eos_net_deinit();
    PASS("mqtt loop timeout behavior");
}

typedef struct {
    uint16_t port;
    broker_sync_t sync;
    uint8_t pingreq[2];
    size_t pingreq_len;
    bool send_pingresp;
} keepalive_broker_ctx_t;

static void *broker_keepalive(void *arg) {
    keepalive_broker_ctx_t *ctx = (keepalive_broker_ctx_t *)arg;
    eos_socket_t s = broker_listen_ready(ctx->port, &ctx->sync);
    if (s == EOS_SOCKET_INVALID) return NULL;
    eos_socket_t c = broker_accept_and_connack(s);
    if (c == EOS_SOCKET_INVALID) return NULL;

    if (broker_recv_exact(c, ctx->pingreq, 2) == 0) {
        ctx->pingreq_len = 2;
        if (ctx->send_pingresp) {
            uint8_t pingresp[2] = { 0xD0, 0x00 };
            eos_net_send(c, pingresp, 2);
        }
    }
    uint8_t disc[2];
    (void)broker_recv_exact(c, disc, 2);
    eos_net_close(c);
    return NULL;
}

static void test_mqtt_keepalive(void) {
    eos_net_init();
    /* Part 1: client sends PINGREQ when idle past keepalive_sec, broker sends PINGRESP */
    keepalive_broker_ctx_t ctx1;
    memset(&ctx1, 0, sizeof(ctx1));
    broker_sync_init(&ctx1.sync);
    ctx1.port = g_broker_port++;
    ctx1.send_pingresp = true;
    pthread_t t1;
    pthread_create(&t1, NULL, broker_keepalive, &ctx1);
    wait_broker_ready(&ctx1.sync);

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, ctx1.port);
    cfg.keepalive_sec = 1;
    eos_mqtt_handle_t h1 = eos_mqtt_connect(&cfg);
    CHECK(h1 != EOS_MQTT_INVALID);

    struct timespec sleep_ts = { .tv_sec = 1, .tv_nsec = 100 * 1000 * 1000 };
    nanosleep(&sleep_ts, NULL);

    CHECK(eos_mqtt_loop(h1, 500) == 0);
    CHECK(ctx1.pingreq_len == 2);
    CHECK(ctx1.pingreq[0] == 0xC0 && ctx1.pingreq[1] == 0x00);

    CHECK(eos_mqtt_disconnect(h1) == 0);
    pthread_join(t1, NULL);
    broker_sync_destroy(&ctx1.sync);

    /* Part 2: broker does NOT send PINGRESP, client detects missed PINGRESP window */
    keepalive_broker_ctx_t ctx2;
    memset(&ctx2, 0, sizeof(ctx2));
    broker_sync_init(&ctx2.sync);
    ctx2.port = g_broker_port++;
    ctx2.send_pingresp = false;
    pthread_t t2;
    pthread_create(&t2, NULL, broker_keepalive, &ctx2);
    wait_broker_ready(&ctx2.sync);

    mqtt_test_config(&cfg, ctx2.port);
    cfg.keepalive_sec = 1;
    eos_mqtt_handle_t h2 = eos_mqtt_connect(&cfg);
    CHECK(h2 != EOS_MQTT_INVALID);

    nanosleep(&sleep_ts, NULL);
    CHECK(eos_mqtt_loop(h2, 50) == 0);
    CHECK(ctx2.pingreq_len == 2);

    nanosleep(&sleep_ts, NULL);
    CHECK(eos_mqtt_loop(h2, 50) == -1);

    pthread_join(t2, NULL);
    broker_sync_destroy(&ctx2.sync);
    eos_net_deinit();
    PASS("mqtt keepalive behavior");
}

typedef struct {
    uint16_t port;
    broker_sync_t sync;
    uint8_t pkt1[128];
    size_t pkt1_len;
    uint8_t pkt2[128];
    size_t pkt2_len;
} retry_broker_ctx_t;

static void *broker_qos1_retry(void *arg) {
    retry_broker_ctx_t *ctx = (retry_broker_ctx_t *)arg;
    eos_socket_t s = broker_listen_ready(ctx->port, &ctx->sync);
    if (s == EOS_SOCKET_INVALID) return NULL;
    eos_socket_t c = broker_accept_and_connack(s);
    if (c == EOS_SOCKET_INVALID) return NULL;

    /* First publish packet: do not send PUBACK */
    uint8_t hdr[2];
    if (broker_recv_exact(c, hdr, 2) == 0 && broker_recv_exact(c, ctx->pkt1 + 2, hdr[1]) == 0) {
        ctx->pkt1[0] = hdr[0];
        ctx->pkt1[1] = hdr[1];
        ctx->pkt1_len = 2 + hdr[1];

        /* Second publish packet (retry with DUP=1) */
        if (broker_recv_exact(c, hdr, 2) == 0 && broker_recv_exact(c, ctx->pkt2 + 2, hdr[1]) == 0) {
            ctx->pkt2[0] = hdr[0];
            ctx->pkt2[1] = hdr[1];
            ctx->pkt2_len = 2 + hdr[1];

            uint16_t tlen = (uint16_t)((ctx->pkt2[2] << 8) | ctx->pkt2[3]);
            size_t pid_offset = 4 + tlen;
            uint16_t pid = (uint16_t)((ctx->pkt2[pid_offset] << 8) | ctx->pkt2[pid_offset + 1]);
            uint8_t puback[4] = { 0x40, 0x02, (uint8_t)(pid >> 8), (uint8_t)(pid & 0xFF) };
            eos_net_send(c, puback, sizeof(puback));
        }
    }
    uint8_t disc[2];
    (void)broker_recv_exact(c, disc, 2);
    eos_net_close(c);
    return NULL;
}

static void test_mqtt_qos1_retry(void) {
    eos_net_init();
    retry_broker_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    broker_sync_init(&ctx.sync);
    ctx.port = g_broker_port++;
    pthread_t t;
    pthread_create(&t, NULL, broker_qos1_retry, &ctx);
    wait_broker_ready(&ctx.sync);

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, ctx.port);
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);

    const char *topic = "sensor/retry";
    const char *payload = "val";
    CHECK(eos_mqtt_publish(h, topic, payload, strlen(payload), 1) == 0);
    CHECK(eos_mqtt_disconnect(h) == 0);
    pthread_join(t, NULL);
    broker_sync_destroy(&ctx.sync);

    CHECK(ctx.pkt1_len > 0);
    CHECK((ctx.pkt1[0] & 0x08) == 0); /* First transmission DUP = 0 */
    CHECK(ctx.pkt2_len > 0);
    CHECK((ctx.pkt2[0] & 0x08) == 0x08); /* Retransmission DUP = 1 */

    eos_net_deinit();
    PASS("mqtt QoS 1 retransmit with DUP=1");
}

static void *broker_bad_suback(void *arg) {
    sub_broker_ctx_t *ctx = (sub_broker_ctx_t *)arg;
    eos_socket_t s = broker_listen_ready(ctx->port, &ctx->sync);
    if (s == EOS_SOCKET_INVALID) return NULL;
    eos_socket_t c = broker_accept_and_connack(s);
    if (c == EOS_SOCKET_INVALID) return NULL;

    uint8_t hdr[2];
    uint8_t body[64];
    if (broker_recv_exact(c, hdr, 2) == 0 && broker_recv_exact(c, body, hdr[1]) == 0) {
        uint16_t pid = (uint16_t)((body[0] << 8) | body[1]);
        /* Return code 0x02 is higher than requested QoS 1; must be rejected */
        uint8_t suback[5] = { 0x90, 0x03, (uint8_t)(pid >> 8), (uint8_t)(pid & 0xFF), 0x02 };
        eos_net_send(c, suback, sizeof(suback));
    }
    uint8_t disc[2];
    (void)broker_recv_exact(c, disc, 2);
    eos_net_close(c);
    return NULL;
}

static void test_mqtt_suback_grant_validation(void) {
    eos_net_init();
    sub_broker_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    broker_sync_init(&ctx.sync);
    ctx.port = g_broker_port++;
    pthread_t t;
    pthread_create(&t, NULL, broker_bad_suback, &ctx);
    wait_broker_ready(&ctx.sync);

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, ctx.port);
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);

    /* Requested QoS 1, but broker returns granted QoS 2 -> must reject and release session */
    CHECK(eos_mqtt_subscribe(h, "sensor/grant", 1, dummy_cb, NULL) == -1);
    /* Handle must be released / disconnected */
    CHECK(eos_mqtt_publish(h, "sensor/grant", "val", 3, 0) == -1);
    CHECK(eos_mqtt_subscribe(h, "sensor/grant", 0, dummy_cb, NULL) == -1);
    CHECK(eos_mqtt_loop(h, 10) == -1);
    CHECK(eos_mqtt_disconnect(h) == -1);

    pthread_join(t, NULL);
    broker_sync_destroy(&ctx.sync);
    eos_net_deinit();
    PASS("mqtt reject SUBACK granted QoS > requested");
}

typedef struct {
    eos_mqtt_handle_t h;
    int pub_res;
    int sub_res;
    int unsub_res;
    int loop_res;
    int disconnect_res;
    eos_mqtt_handle_t connect_res;
    bool invoked;
} nested_test_ctx_t;

static void nested_reentrant_cb(const char *topic, const uint8_t *payload, size_t len, void *ctx) {
    (void)topic; (void)payload; (void)len;
    nested_test_ctx_t *tc = (nested_test_ctx_t *)ctx;
    tc->invoked = true;
    tc->pub_res = eos_mqtt_publish(tc->h, "nested/topic", "msg", 3, 0);
    tc->sub_res = eos_mqtt_subscribe(tc->h, "nested/sub", 0, dummy_cb, NULL);
    tc->unsub_res = eos_mqtt_unsubscribe(tc->h, "nested/sub");
    tc->loop_res = eos_mqtt_loop(tc->h, 10);
    tc->disconnect_res = eos_mqtt_disconnect(tc->h);
    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, 1883);
    tc->connect_res = eos_mqtt_connect(&cfg);
}

static void *broker_send_nested_pub(void *arg) {
    sub_broker_ctx_t *ctx = (sub_broker_ctx_t *)arg;
    eos_socket_t s = broker_listen_ready(ctx->port, &ctx->sync);
    if (s == EOS_SOCKET_INVALID) return NULL;
    eos_socket_t c = broker_accept_and_connack(s);
    if (c == EOS_SOCKET_INVALID) return NULL;

    uint8_t hdr[2];
    uint8_t body[64];
    if (broker_recv_exact(c, hdr, 2) == 0 && broker_recv_exact(c, body, hdr[1]) == 0) {
        uint16_t pid = (uint16_t)((body[0] << 8) | body[1]);
        uint8_t suback[5] = { 0x90, 0x03, (uint8_t)(pid >> 8), (uint8_t)(pid & 0xFF), 0x00 };
        eos_net_send(c, suback, sizeof(suback));

        const char *t = "nested/inbound";
        const char *p = "1";
        uint8_t pub_pkt[64];
        pub_pkt[0] = 0x30;
        pub_pkt[1] = (uint8_t)(2 + strlen(t) + strlen(p));
        pub_pkt[2] = 0;
        pub_pkt[3] = (uint8_t)strlen(t);
        memcpy(pub_pkt + 4, t, strlen(t));
        memcpy(pub_pkt + 4 + strlen(t), p, strlen(p));
        eos_net_send(c, pub_pkt, 2 + pub_pkt[1]);
    }
    uint8_t disc[2];
    (void)broker_recv_exact(c, disc, 2);
    eos_net_close(c);
    return NULL;
}

static void test_mqtt_nested_callback_rejection(void) {
    eos_net_init();
    sub_broker_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    broker_sync_init(&ctx.sync);
    ctx.port = g_broker_port++;
    pthread_t t;
    pthread_create(&t, NULL, broker_send_nested_pub, &ctx);
    wait_broker_ready(&ctx.sync);

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, ctx.port);
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);

    nested_test_ctx_t nctx;
    memset(&nctx, 0, sizeof(nctx));
    nctx.h = h;

    CHECK(eos_mqtt_subscribe(h, "nested/inbound", 0, nested_reentrant_cb, &nctx) == 0);
    CHECK(eos_mqtt_loop(h, 1000) == 0);
    CHECK(nctx.invoked == true);
    CHECK(nctx.pub_res == -1);
    CHECK(nctx.sub_res == -1);
    CHECK(nctx.unsub_res == -1);
    CHECK(nctx.loop_res == -1);
    CHECK(nctx.disconnect_res == -1);
    CHECK(nctx.connect_res == EOS_MQTT_INVALID);

    CHECK(eos_mqtt_disconnect(h) == 0);
    pthread_join(t, NULL);
    broker_sync_destroy(&ctx.sync);
    eos_net_deinit();
    PASS("mqtt reject nested synchronous calls from callback");
}

static void *broker_send_empty_level_wildcard(void *arg) {
    sub_broker_ctx_t *ctx = (sub_broker_ctx_t *)arg;
    eos_socket_t s = broker_listen_ready(ctx->port, &ctx->sync);
    if (s == EOS_SOCKET_INVALID) return NULL;
    eos_socket_t c = broker_accept_and_connack(s);
    if (c == EOS_SOCKET_INVALID) return NULL;

    uint8_t hdr[2];
    uint8_t body[64];
    if (broker_recv_exact(c, hdr, 2) == 0 && broker_recv_exact(c, body, hdr[1]) == 0) {
        uint16_t pid = (uint16_t)((body[0] << 8) | body[1]);
        uint8_t suback[5] = { 0x90, 0x03, (uint8_t)(pid >> 8), (uint8_t)(pid & 0xFF), 0x00 };
        eos_net_send(c, suback, sizeof(suback));

        /* Send publish to topic "sensor/" */
        const char *t = "sensor/";
        const char *p = "empty";
        uint8_t pub_pkt[64];
        pub_pkt[0] = 0x30;
        pub_pkt[1] = (uint8_t)(2 + strlen(t) + strlen(p));
        pub_pkt[2] = 0;
        pub_pkt[3] = (uint8_t)strlen(t);
        memcpy(pub_pkt + 4, t, strlen(t));
        memcpy(pub_pkt + 4 + strlen(t), p, strlen(p));
        eos_net_send(c, pub_pkt, 2 + pub_pkt[1]);
    }
    uint8_t disc[2];
    (void)broker_recv_exact(c, disc, 2);
    eos_net_close(c);
    return NULL;
}

static void test_mqtt_wildcard_empty_level(void) {
    eos_net_init();
    sub_broker_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    broker_sync_init(&ctx.sync);
    ctx.port = g_broker_port++;
    pthread_t t;
    pthread_create(&t, NULL, broker_send_empty_level_wildcard, &ctx);
    wait_broker_ready(&ctx.sync);

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, ctx.port);
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);

    cb_record_t rec;
    memset(&rec, 0, sizeof(rec));
    /* "sensor/+" MUST match "sensor/" (matching trailing empty level) */
    CHECK(eos_mqtt_subscribe(h, "sensor/+", 0, test_msg_cb, &rec) == 0);
    CHECK(eos_mqtt_loop(h, 1000) == 0);
    CHECK(rec.call_count == 1);
    CHECK(strcmp(rec.topic, "sensor/") == 0);

    CHECK(eos_mqtt_disconnect(h) == 0);
    pthread_join(t, NULL);
    broker_sync_destroy(&ctx.sync);
    eos_net_deinit();
    PASS("mqtt wildcard '+' matches empty topic level");
}

static void *broker_send_inbound_oversized(void *arg) {
    sub_broker_ctx_t *ctx = (sub_broker_ctx_t *)arg;
    eos_socket_t s = broker_listen_ready(ctx->port, &ctx->sync);
    if (s == EOS_SOCKET_INVALID) return NULL;
    eos_socket_t c = broker_accept_and_connack(s);
    if (c == EOS_SOCKET_INVALID) return NULL;

    /* Send inbound QoS 1 PUBLISH with topic length 150 (> EOS_MQTT_MAX_TOPIC_LEN 128)
     * Remaining length: 2 (topic len) + 150 (topic) + 2 (packet ID) + 4 (payload) = 158.
     * In MQTT 3.1.1 variable byte integer, 158 is encoded as 0x9E, 0x01. */
    uint8_t pub_pkt[256];
    pub_pkt[0] = 0x32; /* QoS 1 */
    size_t tlen = 150;
    pub_pkt[1] = 0x9E;
    pub_pkt[2] = 0x01;
    pub_pkt[3] = 0x00;
    pub_pkt[4] = (uint8_t)tlen;
    memset(pub_pkt + 5, 'a', tlen);
    size_t off = 5 + tlen;
    pub_pkt[off++] = 0x00;
    pub_pkt[off++] = 0x05;
    memcpy(pub_pkt + off, "data", 4);
    off += 4;
    eos_net_send(c, pub_pkt, off);

    /* Broker checks if client incorrectly sends PUBACK */
    struct timeval tv = { .tv_sec = 0, .tv_usec = 200 * 1000 };
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    uint8_t resp[4];
    int n = eos_net_recv(c, resp, sizeof(resp), 200);
    ctx->pkt_len = (n > 0) ? (size_t)n : 0; /* Should be 0 (no PUBACK sent) */

    eos_net_close(c);
    return NULL;
}

static void test_mqtt_inbound_oversized_topic_no_puback(void) {
    eos_net_init();
    sub_broker_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    broker_sync_init(&ctx.sync);
    ctx.port = g_broker_port++;
    pthread_t t;
    pthread_create(&t, NULL, broker_send_inbound_oversized, &ctx);
    wait_broker_ready(&ctx.sync);

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, ctx.port);
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);

    /* Loop must reject the packet and close connection without sending PUBACK */
    CHECK(eos_mqtt_loop(h, 1000) == -1);

    pthread_join(t, NULL);
    broker_sync_destroy(&ctx.sync);
    CHECK(ctx.pkt_len == 0); /* Verified: no PUBACK was transmitted */
    eos_net_deinit();
    PASS("mqtt reject inbound oversized topic without sending PUBACK");
}

static void *broker_send_bad_flags(void *arg) {
    sub_broker_ctx_t *ctx = (sub_broker_ctx_t *)arg;
    eos_socket_t s = broker_listen_ready(ctx->port, &ctx->sync);
    if (s == EOS_SOCKET_INVALID) return NULL;
    eos_socket_t c = broker_accept_and_connack(s);
    if (c == EOS_SOCKET_INVALID) return NULL;

    /* Send PINGRESP with invalid flags (0xD1 instead of 0xD0) */
    uint8_t bad_pingresp[2] = { 0xD1, 0x00 };
    eos_net_send(c, bad_pingresp, sizeof(bad_pingresp));

    uint8_t buf[8];
    (void)broker_recv_exact(c, buf, sizeof(buf));
    eos_net_close(c);
    return NULL;
}

static void test_mqtt_control_packet_flags(void) {
    eos_net_init();
    sub_broker_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    broker_sync_init(&ctx.sync);
    ctx.port = g_broker_port++;
    pthread_t t;
    pthread_create(&t, NULL, broker_send_bad_flags, &ctx);
    wait_broker_ready(&ctx.sync);

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, ctx.port);
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);

    /* Loop receives bad PINGRESP with invalid fixed-header flags -> fail connection */
    CHECK(eos_mqtt_loop(h, 1000) == -1);

    pthread_join(t, NULL);
    broker_sync_destroy(&ctx.sync);
    eos_net_deinit();
    PASS("mqtt validate fixed-header flags");
}

static void *broker_close_immediately(void *arg) {
    sub_broker_ctx_t *ctx = (sub_broker_ctx_t *)arg;
    eos_socket_t s = broker_listen_ready(ctx->port, &ctx->sync);
    if (s == EOS_SOCKET_INVALID) return NULL;
    eos_socket_t c = broker_accept_and_connack(s);
    if (c == EOS_SOCKET_INVALID) return NULL;

    /* Wait for client to complete connect, then close socket immediately */
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 50 * 1000 * 1000 };
    nanosleep(&ts, NULL);
    eos_net_close(c);
    return NULL;
}

static void test_mqtt_loop_eof_disconnect(void) {
    eos_net_init();
    sub_broker_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    broker_sync_init(&ctx.sync);
    ctx.port = g_broker_port++;
    pthread_t t;
    pthread_create(&t, NULL, broker_close_immediately, &ctx);
    wait_broker_ready(&ctx.sync);

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, ctx.port);
    cfg.keepalive_sec = 0; /* Keepalive disabled */
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);

    pthread_join(t, NULL);
    broker_sync_destroy(&ctx.sync);

    /* Broker closed connection: loop must detect EOF, release client, and return -1 */
    CHECK(eos_mqtt_loop(h, 500) == -1);
    /* Handle must now be invalid and released */
    CHECK(eos_mqtt_loop(h, 100) == -1);
    CHECK(eos_mqtt_publish(h, "topic", "data", 4, 0) == -1);

    eos_net_deinit();
    PASS("mqtt loop detects EOF and releases client");
}

static void *broker_retained_pub_before_suback(void *arg) {
    sub_broker_ctx_t *ctx = (sub_broker_ctx_t *)arg;
    eos_socket_t s = broker_listen_ready(ctx->port, &ctx->sync);
    if (s == EOS_SOCKET_INVALID) return NULL;
    eos_socket_t c = broker_accept_and_connack(s);
    if (c == EOS_SOCKET_INVALID) return NULL;

    uint8_t hdr[2];
    uint8_t body[64];
    if (broker_recv_exact(c, hdr, 2) == 0 && broker_recv_exact(c, body, hdr[1]) == 0) {
        uint16_t pid = (uint16_t)((body[0] << 8) | body[1]);

        /* Send retained PUBLISH BEFORE sending SUBACK */
        const char *t = "sensor/retained";
        const char *p = "init_val";
        uint8_t pub_pkt[64];
        pub_pkt[0] = 0x32; /* QoS 1 */
        pub_pkt[1] = (uint8_t)(2 + strlen(t) + 2 + strlen(p));
        pub_pkt[2] = 0;
        pub_pkt[3] = (uint8_t)strlen(t);
        memcpy(pub_pkt + 4, t, strlen(t));
        size_t off = 4 + strlen(t);
        pub_pkt[off++] = 0x22;
        pub_pkt[off++] = 0x33;
        memcpy(pub_pkt + off, p, strlen(p));
        off += strlen(p);
        eos_net_send(c, pub_pkt, off);

        /* Wait for client to send PUBACK for this QoS 1 message */
        uint8_t puback[4];
        if (broker_recv_exact(c, puback, sizeof(puback)) == 0) {
            if (puback[0] == 0x40 && puback[2] == 0x22 && puback[3] == 0x33) {
                /* Now send the SUBACK */
                uint8_t suback[5] = { 0x90, 0x03, (uint8_t)(pid >> 8), (uint8_t)(pid & 0xFF), 0x01 };
                eos_net_send(c, suback, sizeof(suback));
            }
        }
    }
    uint8_t disc[2];
    (void)broker_recv_exact(c, disc, 2);
    eos_net_close(c);
    return NULL;
}

static void test_mqtt_retained_publish_before_suback(void) {
    eos_net_init();
    sub_broker_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    broker_sync_init(&ctx.sync);
    ctx.port = g_broker_port++;
    pthread_t t;
    pthread_create(&t, NULL, broker_retained_pub_before_suback, &ctx);
    wait_broker_ready(&ctx.sync);

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, ctx.port);
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);

    cb_record_t rec;
    memset(&rec, 0, sizeof(rec));

    /* Subscribe will wait for SUBACK, but receives retained PUBLISH first.
     * The staged subscription must deliver this message to callback! */
    CHECK(eos_mqtt_subscribe(h, "sensor/retained", 1, test_msg_cb, &rec) == 0);
    CHECK(rec.call_count == 1);
    CHECK(strcmp(rec.topic, "sensor/retained") == 0);
    CHECK(strcmp(rec.payload, "init_val") == 0);

    CHECK(eos_mqtt_disconnect(h) == 0);
    pthread_join(t, NULL);
    broker_sync_destroy(&ctx.sync);
    eos_net_deinit();
    PASS("mqtt delivered retained PUBLISH before SUBACK");
}

static void *broker_delayed_pingresp(void *arg) {
    keepalive_broker_ctx_t *ctx = (keepalive_broker_ctx_t *)arg;
    eos_socket_t s = broker_listen_ready(ctx->port, &ctx->sync);
    if (s == EOS_SOCKET_INVALID) return NULL;
    eos_socket_t c = broker_accept_and_connack(s);
    if (c == EOS_SOCKET_INVALID) return NULL;

    if (broker_recv_exact(c, ctx->pingreq, 2) == 0) {
        ctx->pingreq_len = 2;
        /* Wait near the 1s edge of keepalive window before sending PINGRESP */
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 750 * 1000 * 1000 };
        nanosleep(&ts, NULL);
        uint8_t pingresp[2] = { 0xD0, 0x00 };
        eos_net_send(c, pingresp, 2);
    }
    uint8_t disc[2];
    (void)broker_recv_exact(c, disc, 2);
    eos_net_close(c);
    return NULL;
}

static void test_mqtt_pingresp_edge_window(void) {
    eos_net_init();
    keepalive_broker_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    broker_sync_init(&ctx.sync);
    ctx.port = g_broker_port++;
    pthread_t t;
    pthread_create(&t, NULL, broker_delayed_pingresp, &ctx);
    wait_broker_ready(&ctx.sync);

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, ctx.port);
    cfg.keepalive_sec = 1;
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);

    /* Idle past 1 second so loop will send PINGREQ */
    struct timespec sleep_ts = { .tv_sec = 1, .tv_nsec = 100 * 1000 * 1000 };
    nanosleep(&sleep_ts, NULL);

    /* First loop sends PINGREQ */
    CHECK(eos_mqtt_loop(h, 50) == 0);
    CHECK(ctx.pingreq_len == 2);

    /* Wait out broker delay (~750ms) */
    struct timespec wait_edge = { .tv_sec = 0, .tv_nsec = 800 * 1000 * 1000 };
    nanosleep(&wait_edge, NULL);

    /* Next loop call at the edge of the allowed window must drain input and accept PINGRESP,
     * not discard it or drop the connection */
    CHECK(eos_mqtt_loop(h, 100) == 0);
    CHECK(eos_mqtt_disconnect(h) == 0);

    pthread_join(t, NULL);
    broker_sync_destroy(&ctx.sync);
    eos_net_deinit();
    PASS("mqtt timely PINGRESP at window edge accepted");
}

typedef struct {
    uint16_t port;
    broker_sync_t sync;
    uint8_t pingreq[2];
    bool main_read_received_publish;
} drain_test_ctx_t;

static void drain_puback_cb(const char *topic, const uint8_t *payload, size_t len, void *ctx) {
    (void)topic; (void)payload; (void)len;
    drain_test_ctx_t *tc = (drain_test_ctx_t *)ctx;
    if (tc) {
        tc->main_read_received_publish = true;
    }
    /* Sleep 500ms in callback to guarantee elapsed_ms >= keepalive_ms (1000ms)
     * when control returns to mqtt_wait_for_ack, forcing keepalive drain to run. */
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 500 * 1000 * 1000 };
    nanosleep(&ts, NULL);
}

static void *broker_drain_queued_puback(void *arg) {
    drain_test_ctx_t *ctx = (drain_test_ctx_t *)arg;
    eos_socket_t s = broker_listen_ready(ctx->port, &ctx->sync);
    if (s == EOS_SOCKET_INVALID) return NULL;
    eos_socket_t c = broker_accept_and_connack(s);
    if (c == EOS_SOCKET_INVALID) return NULL;

    /* Receive SUBSCRIBE from client (PID 1) */
    uint8_t sub_hdr[2];
    if (broker_recv_exact(c, sub_hdr, 2) == 0) {
        uint8_t sub_buf[64];
        if (broker_recv_exact(c, sub_buf, sub_hdr[1]) == 0) {
            uint16_t sub_pid = (uint16_t)((sub_buf[0] << 8) | sub_buf[1]);
            uint8_t suback[5] = { 0x90, 0x03, (uint8_t)(sub_pid >> 8), (uint8_t)(sub_pid & 0xFF), 0x00 };
            eos_net_send(c, suback, sizeof(suback));
        }
    }

    /* Receive QoS 1 PUBLISH (PID 2) */
    uint8_t hdr[2];
    if (broker_recv_exact(c, hdr, 2) == 0) {
        uint8_t buf[64];
        if (broker_recv_exact(c, buf, hdr[1]) == 0) {
            /* Now wait for PINGREQ from keep-alive before sending PUBACK */
            if (broker_recv_exact(c, ctx->pingreq, 2) == 0) {
                /* Delay 600ms after PINGREQ (comfortably within the 1000ms keepalive window with 400ms margin) */
                struct timespec ts = { .tv_sec = 0, .tv_nsec = 600 * 1000 * 1000 };
                nanosleep(&ts, NULL);

                /* Send an interleaved inbound PUBLISH on "sensor/dummy" followed by
                 * queued PUBACK for PID 2 and PINGRESP in a single payload.
                 * The main read consumes the inbound PUBLISH, its callback pushes elapsed_ms
                 * past 1000ms, and mqtt_keepalive_check_deadline drains the queued PUBACK & PINGRESP.
                 */
                uint8_t pkts[] = {
                    /* Inbound QoS 0 PUBLISH on "sensor/dummy": 0x30, rem_len 16 (0x10), topic len 12 (0x00, 0x0C), "sensor/dummy", payload "hi" */
                    0x30, 0x10,
                    0x00, 0x0C, 's', 'e', 'n', 's', 'o', 'r', '/', 'd', 'u', 'm', 'm', 'y',
                    'h', 'i',
                    /* PUBACK PID 2 */
                    0x40, 0x02, 0x00, 0x02,
                    /* PINGRESP */
                    0xD0, 0x00
                };
                eos_net_send(c, pkts, sizeof(pkts));
            }
        }
    }
    uint8_t disc[2];
    (void)broker_recv_exact(c, disc, 2);
    eos_net_close(c);
    return NULL;
}

static void test_mqtt_drain_queued_puback(void) {
    eos_net_init();
    drain_test_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    broker_sync_init(&ctx.sync);
    ctx.port = g_broker_port++;
    pthread_t t;
    pthread_create(&t, NULL, broker_drain_queued_puback, &ctx);
    wait_broker_ready(&ctx.sync);

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, ctx.port);
    cfg.keepalive_sec = 1;
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);

    /* Subscribe first (PID 1) to "sensor/dummy" with callback that delays 500ms */
    CHECK(eos_mqtt_subscribe(h, "sensor/dummy", 0, drain_puback_cb, &ctx) == 0);

    /* Publish with QoS 1 (PID 2). Timeline:
     * - Keep-alive interval (1s) expires while waiting, sending PINGREQ.
     * - Broker delays 600ms, then sends [Inbound PUBLISH] + [PUBACK PID 2] + [PINGRESP].
     * - Main read in mqtt_wait_for_ack consumes the inbound PUBLISH at ~600ms.
     * - drain_puback_cb records main_read_received_publish = true and delays 500ms, pushing elapsed_ms to ~1100ms >= 1000ms.
     * - mqtt_keepalive_check_deadline executes the drain loop, consumes the queued PUBACK,
     *   routes it to pending_ack.completed = true, drains PINGRESP, and returns 0.
     * - The waiter verifies pending_ack.completed is true and returns 0.
     */
    CHECK(eos_mqtt_publish(h, "sensor/data", "val", 3, 1) == 0);
    CHECK(ctx.main_read_received_publish == true);
    CHECK(eos_mqtt_disconnect(h) == 0);

    pthread_join(t, NULL);
    broker_sync_destroy(&ctx.sync);
    eos_net_deinit();
    PASS("mqtt queued PUBACK routed correctly during keepalive drain");
}

typedef struct {
    uint16_t port;
    broker_sync_t sync;
    uint8_t pingreq[2];
    bool main_read_received_publish;
} drain_eof_ctx_t;

static void drain_eof_cb(const char *topic, const uint8_t *payload, size_t len, void *ctx) {
    (void)topic; (void)payload; (void)len;
    drain_eof_ctx_t *tc = (drain_eof_ctx_t *)ctx;
    if (tc) {
        tc->main_read_received_publish = true;
    }
    /* Delay 500ms in callback so elapsed_ms >= keepalive_ms (1000ms) when control
     * returns to eos_mqtt_loop, guaranteeing the keepalive drain loop executes. */
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 500 * 1000 * 1000 };
    nanosleep(&ts, NULL);
}

static void *broker_drain_eof(void *arg) {
    drain_eof_ctx_t *ctx = (drain_eof_ctx_t *)arg;
    eos_socket_t s = broker_listen_ready(ctx->port, &ctx->sync);
    if (s == EOS_SOCKET_INVALID) return NULL;
    eos_socket_t c = broker_accept_and_connack(s);
    if (c == EOS_SOCKET_INVALID) return NULL;

    /* Receive SUBSCRIBE from client (PID 1) */
    uint8_t sub_hdr[2];
    if (broker_recv_exact(c, sub_hdr, 2) == 0) {
        uint8_t sub_buf[64];
        if (broker_recv_exact(c, sub_buf, sub_hdr[1]) == 0) {
            uint16_t sub_pid = (uint16_t)((sub_buf[0] << 8) | sub_buf[1]);
            uint8_t suback[5] = { 0x90, 0x03, (uint8_t)(sub_pid >> 8), (uint8_t)(sub_pid & 0xFF), 0x00 };
            eos_net_send(c, suback, sizeof(suback));
        }
    }

    if (broker_recv_exact(c, ctx->pingreq, 2) == 0) {
        /* Delay 600ms after PINGREQ (comfortably within the 1000ms keepalive window with 400ms margin) */
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 600 * 1000 * 1000 };
        nanosleep(&ts, NULL);

        /* Send an inbound PUBLISH so the main read consumes it successfully */
        uint8_t pub_pkt[] = {
            /* Inbound QoS 0 PUBLISH on "sensor/dummy" */
            0x30, 0x10,
            0x00, 0x0C, 's', 'e', 'n', 's', 'o', 'r', '/', 'd', 'u', 'm', 'm', 'y',
            'h', 'i'
        };
        eos_net_send(c, pub_pkt, sizeof(pub_pkt));

        /* Immediately close the connection (EOF) */
        eos_net_close(c);
        return NULL;
    }
    eos_net_close(c);
    return NULL;
}

static void test_mqtt_drain_eof_returns_error(void) {
    eos_net_init();
    drain_eof_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    broker_sync_init(&ctx.sync);
    ctx.port = g_broker_port++;
    pthread_t t;
    pthread_create(&t, NULL, broker_drain_eof, &ctx);
    wait_broker_ready(&ctx.sync);

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, ctx.port);
    cfg.keepalive_sec = 1;
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);

    /* Subscribe first so client has a handler for inbound PUBLISH */
    CHECK(eos_mqtt_subscribe(h, "sensor/dummy", 0, drain_eof_cb, &ctx) == 0);

    /* Idle past 1 second so loop will send PINGREQ */
    struct timespec sleep_ts = { .tv_sec = 1, .tv_nsec = 100 * 1000 * 1000 };
    nanosleep(&sleep_ts, NULL);

    /* First loop sends PINGREQ. Ping sent at T0, deadline is T0 + 1000ms. */
    CHECK(eos_mqtt_loop(h, 50) == 0);

    /* Call eos_mqtt_loop with 1000ms timeout:
     * - Note: mqtt_keepalive_prepare clamps wait_ms to the remaining time in the
     *   keepalive window (~1000ms), so passing 1000ms accurately reflects the effective timeout.
     * - The broker delays 600ms (400ms margin before the 1000ms deadline).
     * - At 600ms, broker sends inbound PUBLISH and immediately closes the socket (EOF).
     * - The main read in eos_mqtt_loop consumes the PUBLISH cleanly (rc == 0).
     * - drain_eof_cb records main_read_received_publish = true and delays 500ms.
     * - When the callback returns, elapsed_ms is ~1100ms >= 1000ms.
     * - Next, mqtt_keepalive_check_deadline executes the drain loop because elapsed_ms >= keepalive_ms.
     * - Drain loop calls mqtt_process_one_packet(c, 0, rem) and encounters EOF on the closed socket.
     * - Drain loop returns -1 via net.c:541-544, and eos_mqtt_loop returns -1, releasing the handle.
     */
    CHECK(eos_mqtt_loop(h, 1000) == -1);

    /* Verify that the main read received the PUBLISH and dispatched the callback
     * prior to the drain, proving this test exercised the drain-EOF path and not
     * an idle timeout or premature disconnect. */
    CHECK(ctx.main_read_received_publish == true);

    /* Client session must now be released */
    CHECK(eos_mqtt_loop(h, 100) == -1);

    pthread_join(t, NULL);
    broker_sync_destroy(&ctx.sync);
    eos_net_deinit();
    PASS("mqtt EOF during keepalive drain returns error and releases client");
}

typedef struct {
    uint16_t port;
    broker_sync_t sync;
    uint8_t pingreq[2];
} streaming_drain_ctx_t;

static void *broker_drain_streaming(void *arg) {
    streaming_drain_ctx_t *ctx = (streaming_drain_ctx_t *)arg;
    eos_socket_t s = broker_listen_ready(ctx->port, &ctx->sync);
    if (s == EOS_SOCKET_INVALID) return NULL;
    eos_socket_t c = broker_accept_and_connack(s);
    if (c == EOS_SOCKET_INVALID) return NULL;

    /* Wait for PINGREQ from client */
    if (broker_recv_exact(c, ctx->pingreq, 2) == 0) {
        /* Delay 1000ms so keepalive deadline expires */
        struct timespec ts = { .tv_sec = 1, .tv_nsec = 10 * 1000 * 1000 };
        nanosleep(&ts, NULL);

        /* Send 40 QoS 0 PUBLISH packets followed by PINGRESP */
        uint8_t pub_pkt[] = {
            0x30, 0x08,
            0x00, 0x03, 'a', '/', 'b',
            'm', 's', 'g'
        };
        for (int i = 0; i < 40; i++) {
            eos_net_send(c, pub_pkt, sizeof(pub_pkt));
        }

        uint8_t pingresp[2] = { 0xD0, 0x00 };
        eos_net_send(c, pingresp, sizeof(pingresp));
    }

    uint8_t disc[2];
    (void)broker_recv_exact(c, disc, 2);
    eos_net_close(c);
    return NULL;
}

static void test_mqtt_drain_streaming_publish_does_not_disconnect(void) {
    eos_net_init();
    streaming_drain_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    broker_sync_init(&ctx.sync);
    ctx.port = g_broker_port++;
    pthread_t t;
    pthread_create(&t, NULL, broker_drain_streaming, &ctx);
    wait_broker_ready(&ctx.sync);

    eos_mqtt_config_t cfg;
    mqtt_test_config(&cfg, ctx.port);
    cfg.keepalive_sec = 1;
    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    CHECK(h != EOS_MQTT_INVALID);

    /* Idle past 1 second so loop sends PINGREQ */
    struct timespec sleep_ts = { .tv_sec = 1, .tv_nsec = 100 * 1000 * 1000 };
    nanosleep(&sleep_ts, NULL);

    CHECK(eos_mqtt_loop(h, 50) == 0);

    /* Broker delays 1s then sends 40 PUBLISH packets + PINGRESP.
     * With max_drain_packets = 64 and 100ms budget, the drain processes the burst and PINGRESP
     * without falsely disconnecting the live broker.
     */
    struct timespec wait_broker = { .tv_sec = 1, .tv_nsec = 50 * 1000 * 1000 };
    nanosleep(&wait_broker, NULL);

    CHECK(eos_mqtt_loop(h, 100) == 0);
    /* Client session remains valid and connected */
    CHECK(eos_mqtt_publish(h, "client/test", "alive", 5, 0) == 0);
    CHECK(eos_mqtt_disconnect(h) == 0);

    pthread_join(t, NULL);
    broker_sync_destroy(&ctx.sync);
    eos_net_deinit();
    PASS("mqtt streaming broker during keepalive drain does not disconnect");
}
#endif /* EOS_NET_POSIX */

int main(void) {
    printf("=== EoS Networking Tests ===\n");
#ifdef EOS_NET_POSIX
    configure_test_destination();
#endif
    test_net_init();
    test_net_socket_tcp();
    test_net_socket_udp();
    test_net_socket_invalid_protocol();
    test_net_bind();
    test_net_listen();
    test_net_close();
    test_net_close_invalid();
    test_net_resolve();
    test_net_resolve_null();
    test_net_send();
    test_net_recv();
    test_http_get();
    test_http_post();
    test_mqtt_connect();
    test_mdns_init();
#ifdef EOS_NET_POSIX
    test_net_tcp_round_trip();
    test_net_resolve_localhost();
    test_net_connect_refused();
    test_net_recv_zero_timeout_polls();
    test_net_udp_zero_length_datagram();
    test_mqtt_connect_disconnect_lifecycle();
    test_mqtt_connect_credentials();
    test_mqtt_reconnect_after_disconnect();
    test_mqtt_connect_broker_refuses();
    test_mqtt_connect_malformed_connack();
    test_mqtt_connect_broker_hangs_up();
    test_mqtt_connect_no_listener();
    test_mqtt_connect_bad_config();
    test_mqtt_disconnect_bad_handle();
    test_mqtt_publish_qos0();
    test_mqtt_publish_qos1_puback();
    test_mqtt_subscribe_suback();
    test_mqtt_incoming_publish();
    test_mqtt_incoming_qos1_puback();
    test_mqtt_unsubscribe_unsuback();
    test_mqtt_invalid_handles_and_args();
    test_mqtt_unsupported_qos();
    test_mqtt_malformed_packet();
    test_mqtt_loop_timeout();
    test_mqtt_keepalive();
    test_mqtt_qos1_retry();
    test_mqtt_suback_grant_validation();
    test_mqtt_nested_callback_rejection();
    test_mqtt_wildcard_empty_level();
    test_mqtt_inbound_oversized_topic_no_puback();
    test_mqtt_control_packet_flags();
    test_mqtt_loop_eof_disconnect();
    test_mqtt_retained_publish_before_suback();
    test_mqtt_pingresp_edge_window();
    test_mqtt_drain_queued_puback();
    test_mqtt_drain_eof_returns_error();
    test_mqtt_drain_streaming_publish_does_not_disconnect();
#endif
    printf("\n=== ALL %d NET TESTS PASSED ===\n", passed);
    return 0;
}
