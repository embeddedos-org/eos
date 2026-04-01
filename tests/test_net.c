// SPDX-License-Identifier: MIT
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define EOS_ENABLE_NET 1
#include "eos/eos_config.h"
#include "eos/net.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

static void test_net_init(void) {
    assert(eos_net_init() == 0);
    eos_net_deinit();
    PASS("net init/deinit");
}
static void test_net_socket_tcp(void) {
    eos_net_init();
    eos_socket_t s = eos_net_socket(EOS_NET_TCP);
    assert(s != EOS_SOCKET_INVALID);
    eos_net_close(s);
    eos_net_deinit();
    PASS("net socket TCP");
}
static void test_net_socket_udp(void) {
    eos_net_init();
    eos_socket_t s = eos_net_socket(EOS_NET_UDP);
    assert(s != EOS_SOCKET_INVALID);
    eos_net_close(s);
    eos_net_deinit();
    PASS("net socket UDP");
}
static void test_net_bind(void) {
    eos_net_init();
    eos_socket_t s = eos_net_socket(EOS_NET_TCP);
    assert(eos_net_bind(s, 8080) == 0);
    eos_net_close(s);
    eos_net_deinit();
    PASS("net bind");
}
static void test_net_listen(void) {
    eos_net_init();
    eos_socket_t s = eos_net_socket(EOS_NET_TCP);
    eos_net_bind(s, 9090);
    assert(eos_net_listen(s, 5) == 0);
    eos_net_close(s);
    eos_net_deinit();
    PASS("net listen");
}
static void test_net_close(void) {
    eos_net_init();
    eos_socket_t s = eos_net_socket(EOS_NET_TCP);
    assert(eos_net_close(s) == 0);
    eos_net_deinit();
    PASS("net close");
}
static void test_net_close_invalid(void) {
    eos_net_init();
    assert(eos_net_close(EOS_SOCKET_INVALID) != 0);
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
    assert(r != 0);
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
int main(void) {
    printf("=== EoS Networking Tests ===\n");
    test_net_init();
    test_net_socket_tcp();
    test_net_socket_udp();
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
    printf("\n=== ALL %d NET TESTS PASSED ===\n", passed);
    return 0;
}
