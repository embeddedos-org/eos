// SPDX-License-Identifier: MIT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define EOS_ENABLE_NET 1
#include "eos/eos_config.h"
#include "eos/net.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

/* Not CHECK(): CI builds Release, and NDEBUG deletes CHECK() together with
 * the expression inside it. Half these checks wrap the call under test --
 * CHECK(eos_net_connect(...) == 0) stopped connecting at all, the echo
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
#include <pthread.h>
#include <time.h>

static uint16_t g_test_port = 45771;

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
    addr.ip = inet_addr("127.0.0.1");
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
#endif /* EOS_NET_POSIX */

int main(void) {
    printf("=== EoS Networking Tests ===\n");
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
#endif
    printf("\n=== ALL %d NET TESTS PASSED ===\n", passed);
    return 0;
}
