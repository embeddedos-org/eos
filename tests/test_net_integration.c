// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file test_net_integration.c
 * @brief lwIP socket integration tests — TCP/UDP lifecycle, HTTP, MQTT, mDNS
 *
 * Tests the socket abstraction layer (net.c) without requiring a real
 * network stack. Exercises the fallback stub paths and validates API
 * contracts, error handling, and protocol state machines.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#define EOS_ENABLE_NET 1
#include "eos/net.h"

static int passed = 0;
#define PASS(name) do { printf("  [PASS] %s\n", name); passed++; } while(0)

/* ============================================================
 * Socket Lifecycle Tests
 * ============================================================ */

static void test_init_deinit(void) {
    assert(eos_net_init() == 0);
    assert(eos_net_init() == 0);  /* Double init should be safe */
    eos_net_deinit();
    eos_net_deinit();              /* Double deinit should be safe */
    PASS("init/deinit idempotent");
}

static void test_tcp_socket_lifecycle(void) {
    eos_net_init();
    eos_socket_t s = eos_net_socket(EOS_NET_TCP);
    assert(s != EOS_SOCKET_INVALID);
    assert(eos_net_close(s) == 0);
    eos_net_deinit();
    PASS("TCP socket create/close");
}

static void test_udp_socket_lifecycle(void) {
    eos_net_init();
    eos_socket_t s = eos_net_socket(EOS_NET_UDP);
    assert(s != EOS_SOCKET_INVALID);
    assert(eos_net_close(s) == 0);
    eos_net_deinit();
    PASS("UDP socket create/close");
}

static void test_multiple_sockets(void) {
    eos_net_init();
    eos_socket_t s1 = eos_net_socket(EOS_NET_TCP);
    eos_socket_t s2 = eos_net_socket(EOS_NET_UDP);
    eos_socket_t s3 = eos_net_socket(EOS_NET_TCP);
    assert(s1 != EOS_SOCKET_INVALID);
    assert(s2 != EOS_SOCKET_INVALID);
    assert(s3 != EOS_SOCKET_INVALID);
    assert(s1 != s2 && s2 != s3 && s1 != s3);  /* Unique FDs */
    eos_net_close(s1);
    eos_net_close(s2);
    eos_net_close(s3);
    eos_net_deinit();
    PASS("multiple sockets unique FDs");
}

static void test_close_invalid(void) {
    eos_net_init();
    assert(eos_net_close(EOS_SOCKET_INVALID) != 0);
    eos_net_deinit();
    PASS("close invalid socket");
}

/* ============================================================
 * Bind / Listen / Accept Tests
 * ============================================================ */

static void test_bind_listen(void) {
    eos_net_init();
    eos_socket_t s = eos_net_socket(EOS_NET_TCP);
    assert(eos_net_bind(s, 8080) == 0);
    assert(eos_net_listen(s, 5) == 0);
    eos_net_close(s);
    eos_net_deinit();
    PASS("bind + listen");
}

static void test_accept_no_client(void) {
    eos_net_init();
    eos_socket_t s = eos_net_socket(EOS_NET_TCP);
    eos_net_bind(s, 9090);
    eos_net_listen(s, 1);
    /* Accept with no pending connection — should return INVALID */
    eos_net_addr_t client;
    eos_socket_t c = eos_net_accept(s, &client);
    assert(c == EOS_SOCKET_INVALID);  /* No connection pending in stub */
    eos_net_close(s);
    eos_net_deinit();
    PASS("accept no client");
}

/* ============================================================
 * Send / Recv Tests (stub behavior)
 * ============================================================ */

static void test_send_without_connect(void) {
    eos_net_init();
    eos_socket_t s = eos_net_socket(EOS_NET_TCP);
    const char *data = "hello";
    int rc = eos_net_send(s, data, 5);
    /* Stub returns -1 (not connected) */
    assert(rc == -1);
    eos_net_close(s);
    eos_net_deinit();
    PASS("send without connect");
}

static void test_recv_without_connect(void) {
    eos_net_init();
    eos_socket_t s = eos_net_socket(EOS_NET_TCP);
    char buf[32];
    int rc = eos_net_recv(s, buf, sizeof(buf), 100);
    assert(rc == -1);
    eos_net_close(s);
    eos_net_deinit();
    PASS("recv without connect");
}

static void test_sendto_udp(void) {
    eos_net_init();
    eos_socket_t s = eos_net_socket(EOS_NET_UDP);
    eos_net_addr_t dest = {.ip = 0x0100007F, .port = 1234};  /* 127.0.0.1:1234 */
    int rc = eos_net_sendto(s, "test", 4, &dest);
    /* Stub returns -1 */
    assert(rc == -1);
    eos_net_close(s);
    eos_net_deinit();
    PASS("sendto UDP");
}

static void test_recvfrom_udp(void) {
    eos_net_init();
    eos_socket_t s = eos_net_socket(EOS_NET_UDP);
    eos_net_bind(s, 5000);
    char buf[64];
    eos_net_addr_t src;
    int rc = eos_net_recvfrom(s, buf, sizeof(buf), &src, 100);
    assert(rc == -1);  /* No data in stub */
    eos_net_close(s);
    eos_net_deinit();
    PASS("recvfrom UDP");
}

/* ============================================================
 * DNS Resolve Tests
 * ============================================================ */

static void test_resolve(void) {
    eos_net_init();
    uint32_t ip = 0;
    int rc = eos_net_resolve("example.com", &ip);
    /* Stub returns -1 (no DNS resolver without lwIP) */
    assert(rc == -1);
    eos_net_deinit();
    PASS("DNS resolve (stub)");
}

static void test_resolve_null(void) {
    eos_net_init();
    int rc = eos_net_resolve(NULL, NULL);
    assert(rc == -1);
    eos_net_deinit();
    PASS("DNS resolve NULL args");
}

/* ============================================================
 * HTTP Client Tests
 * ============================================================ */

static void test_http_get(void) {
    eos_net_init();
    eos_http_response_t resp;
    memset(&resp, 0, sizeof(resp));
    int rc = eos_http_get("http://example.com", &resp);
    /* Stub returns -1 (no network) */
    assert(rc == -1);
    assert(resp.body == NULL);
    assert(resp.body_len == 0);
    eos_http_response_free(&resp);
    eos_net_deinit();
    PASS("HTTP GET (stub)");
}

static void test_http_post(void) {
    eos_net_init();
    eos_http_response_t resp;
    memset(&resp, 0, sizeof(resp));
    int rc = eos_http_post("http://example.com/api", "{\"key\":\"val\"}", 13,
                           "application/json", &resp);
    assert(rc == -1);
    eos_http_response_free(&resp);
    eos_net_deinit();
    PASS("HTTP POST (stub)");
}

static void test_http_response_free_null(void) {
    eos_http_response_free(NULL);  /* Must not crash */
    PASS("HTTP response free NULL");
}

/* ============================================================
 * MQTT Client Tests
 * ============================================================ */

static void test_mqtt_connect(void) {
    eos_net_init();
    eos_mqtt_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.broker_port = 1883;
    strncpy(cfg.broker_host, "mqtt.example.com", sizeof(cfg.broker_host) - 1);
    strncpy(cfg.client_id, "eos_test", sizeof(cfg.client_id) - 1);

    eos_mqtt_handle_t h = eos_mqtt_connect(&cfg);
    assert(h == EOS_MQTT_INVALID);  /* No network in stub */
    eos_net_deinit();
    PASS("MQTT connect (stub)");
}

static void test_mqtt_disconnect_invalid(void) {
    eos_net_init();
    int rc = eos_mqtt_disconnect(EOS_MQTT_INVALID);
    assert(rc == -1);
    eos_net_deinit();
    PASS("MQTT disconnect invalid");
}

static void test_mqtt_publish_invalid(void) {
    eos_net_init();
    int rc = eos_mqtt_publish(EOS_MQTT_INVALID, "topic", "data", 4, 0);
    assert(rc == -1);
    eos_net_deinit();
    PASS("MQTT publish invalid handle");
}

static void test_mqtt_subscribe_invalid(void) {
    eos_net_init();
    int rc = eos_mqtt_subscribe(EOS_MQTT_INVALID, "topic/#", 0, NULL, NULL);
    assert(rc == -1);
    eos_net_deinit();
    PASS("MQTT subscribe invalid handle");
}

static void test_mqtt_loop_invalid(void) {
    eos_net_init();
    int rc = eos_mqtt_loop(EOS_MQTT_INVALID, 100);
    assert(rc == -1);
    eos_net_deinit();
    PASS("MQTT loop invalid handle");
}

/* ============================================================
 * mDNS Tests
 * ============================================================ */

static void test_mdns_init(void) {
    int rc = eos_mdns_init();
    assert(rc == -1);  /* No multicast in stub */
    eos_mdns_deinit();
    PASS("mDNS init/deinit (stub)");
}

static void test_mdns_register(void) {
    int rc = eos_mdns_register("eos-device", "_http._tcp", 80);
    assert(rc == -1);
    PASS("mDNS register (stub)");
}

static void test_mdns_resolve(void) {
    eos_net_addr_t addr;
    int rc = eos_mdns_resolve("_http._tcp", &addr, 1000);
    assert(rc == -1);
    PASS("mDNS resolve (stub)");
}

/* ============================================================
 * Main
 * ============================================================ */

int main(void) {
    printf("=== EoS Network Integration Tests ===\n\n");

    /* Socket lifecycle */
    test_init_deinit();
    test_tcp_socket_lifecycle();
    test_udp_socket_lifecycle();
    test_multiple_sockets();
    test_close_invalid();

    /* Server ops */
    test_bind_listen();
    test_accept_no_client();

    /* Send/recv */
    test_send_without_connect();
    test_recv_without_connect();
    test_sendto_udp();
    test_recvfrom_udp();

    /* DNS */
    test_resolve();
    test_resolve_null();

    /* HTTP */
    test_http_get();
    test_http_post();
    test_http_response_free_null();

    /* MQTT */
    test_mqtt_connect();
    test_mqtt_disconnect_invalid();
    test_mqtt_publish_invalid();
    test_mqtt_subscribe_invalid();
    test_mqtt_loop_invalid();

    /* mDNS */
    test_mdns_init();
    test_mdns_register();
    test_mdns_resolve();

    printf("\n=== ALL %d NET INTEGRATION TESTS PASSED ===\n", passed);
    return 0;
}
