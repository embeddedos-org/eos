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

static uint16_t g_broker_port = 45780;

typedef struct {
    uint16_t       port;
    const uint8_t *connack;         /* NULL: hang up after CONNECT */
    size_t         connack_len;
    volatile int   ready;
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
        b->ready = -1;
        return NULL;
    }
    b->ready = 1;
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
    b->port = g_broker_port++;
    b->connack = connack;
    b->connack_len = connack_len;
    pthread_create(t, NULL, fake_broker, b);
    for (int i = 0; i < 400 && b->ready == 0; i++) {
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 5 * 1000 * 1000;
        nanosleep(&ts, NULL);
    }
    CHECK(b->ready == 1);
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
    test_mqtt_connect_disconnect_lifecycle();
    test_mqtt_connect_credentials();
    test_mqtt_reconnect_after_disconnect();
    test_mqtt_connect_broker_refuses();
    test_mqtt_connect_malformed_connack();
    test_mqtt_connect_broker_hangs_up();
    test_mqtt_connect_no_listener();
    test_mqtt_connect_bad_config();
    test_mqtt_disconnect_bad_handle();
#endif
    printf("\n=== ALL %d NET TESTS PASSED ===\n", passed);
    return 0;
}
