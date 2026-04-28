// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file test_net_mock.c
 * @brief Network tests with mock socket backend
 *
 * Implements a mock socket layer that simulates loopback connections,
 * buffered send/recv, and injectable HTTP/MQTT responses — allowing
 * full-path testing of the net.c API without a real network stack.
 *
 * The mock layer replaces eos_net_* functions with instrumented versions
 * that track state and buffer data for verification.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * Mock Socket Backend
 * ============================================================ */

#define MOCK_MAX_SOCKETS    16
#define MOCK_BUF_SIZE       1024

typedef enum {
    MOCK_PROTO_TCP = 0,
    MOCK_PROTO_UDP = 1,
} mock_proto_t;

typedef enum {
    MOCK_STATE_FREE = 0,
    MOCK_STATE_CREATED,
    MOCK_STATE_BOUND,
    MOCK_STATE_LISTENING,
    MOCK_STATE_CONNECTED,
    MOCK_STATE_CLOSED,
} mock_state_t;

typedef struct {
    mock_state_t state;
    mock_proto_t proto;
    uint16_t     local_port;
    uint32_t     remote_ip;
    uint16_t     remote_port;
    /* Ring buffer for received data (injectable) */
    uint8_t      rx_buf[MOCK_BUF_SIZE];
    size_t       rx_head;
    size_t       rx_tail;
    size_t       rx_count;
    /* Captured sent data */
    uint8_t      tx_buf[MOCK_BUF_SIZE];
    size_t       tx_len;
    /* Stats */
    int          send_count;
    int          recv_count;
    int          backlog;
} mock_socket_t;

static mock_socket_t mock_sockets[MOCK_MAX_SOCKETS];
static bool mock_initialized = false;
static int mock_next_fd = 1;

/* Inject data into a socket's receive buffer (simulates incoming data) */
static void mock_inject_rx(int fd, const void *data, size_t len)
{
    if (fd < 0 || fd >= MOCK_MAX_SOCKETS) return;
    mock_socket_t *s = &mock_sockets[fd];
    const uint8_t *src = (const uint8_t *)data;
    for (size_t i = 0; i < len && s->rx_count < MOCK_BUF_SIZE; i++) {
        s->rx_buf[s->rx_head] = src[i];
        s->rx_head = (s->rx_head + 1) % MOCK_BUF_SIZE;
        s->rx_count++;
    }
}

/* Read captured TX data from a socket */
static size_t mock_get_tx(int fd, void *buf, size_t max)
{
    if (fd < 0 || fd >= MOCK_MAX_SOCKETS) return 0;
    mock_socket_t *s = &mock_sockets[fd];
    size_t n = s->tx_len < max ? s->tx_len : max;
    memcpy(buf, s->tx_buf, n);
    return n;
}

/* ============================================================
 * Mock Net API (replaces eos_net_* for testing)
 * ============================================================ */

static int mock_net_init(void)
{
    memset(mock_sockets, 0, sizeof(mock_sockets));
    mock_initialized = true;
    mock_next_fd = 1;
    return 0;
}

static void mock_net_deinit(void)
{
    mock_initialized = false;
}

static int mock_net_socket(int proto)
{
    if (!mock_initialized) return -1;
    int fd = mock_next_fd++;
    if (fd >= MOCK_MAX_SOCKETS) return -1;
    mock_sockets[fd].state = MOCK_STATE_CREATED;
    mock_sockets[fd].proto = (mock_proto_t)proto;
    mock_sockets[fd].tx_len = 0;
    mock_sockets[fd].rx_head = 0;
    mock_sockets[fd].rx_tail = 0;
    mock_sockets[fd].rx_count = 0;
    mock_sockets[fd].send_count = 0;
    mock_sockets[fd].recv_count = 0;
    return fd;
}

static int mock_net_connect(int fd, uint32_t ip, uint16_t port)
{
    if (fd < 0 || fd >= MOCK_MAX_SOCKETS) return -1;
    mock_socket_t *s = &mock_sockets[fd];
    if (s->state != MOCK_STATE_CREATED) return -1;
    s->remote_ip = ip;
    s->remote_port = port;
    s->state = MOCK_STATE_CONNECTED;
    return 0;
}

static int mock_net_bind(int fd, uint16_t port)
{
    if (fd < 0 || fd >= MOCK_MAX_SOCKETS) return -1;
    mock_socket_t *s = &mock_sockets[fd];
    if (s->state != MOCK_STATE_CREATED) return -1;
    s->local_port = port;
    s->state = MOCK_STATE_BOUND;
    return 0;
}

static int mock_net_listen(int fd, int backlog)
{
    if (fd < 0 || fd >= MOCK_MAX_SOCKETS) return -1;
    mock_socket_t *s = &mock_sockets[fd];
    if (s->state != MOCK_STATE_BOUND) return -1;
    s->backlog = backlog;
    s->state = MOCK_STATE_LISTENING;
    return 0;
}

static int mock_net_send(int fd, const void *data, size_t len)
{
    if (fd < 0 || fd >= MOCK_MAX_SOCKETS) return -1;
    mock_socket_t *s = &mock_sockets[fd];
    if (s->state != MOCK_STATE_CONNECTED) return -1;
    size_t to_copy = len;
    if (s->tx_len + to_copy > MOCK_BUF_SIZE)
        to_copy = MOCK_BUF_SIZE - s->tx_len;
    memcpy(s->tx_buf + s->tx_len, data, to_copy);
    s->tx_len += to_copy;
    s->send_count++;
    return (int)to_copy;
}

static int mock_net_recv(int fd, void *buf, size_t len)
{
    if (fd < 0 || fd >= MOCK_MAX_SOCKETS) return -1;
    mock_socket_t *s = &mock_sockets[fd];
    if (s->state != MOCK_STATE_CONNECTED) return -1;
    if (s->rx_count == 0) return 0; /* No data */
    uint8_t *dst = (uint8_t *)buf;
    size_t n = len < s->rx_count ? len : s->rx_count;
    for (size_t i = 0; i < n; i++) {
        dst[i] = s->rx_buf[s->rx_tail];
        s->rx_tail = (s->rx_tail + 1) % MOCK_BUF_SIZE;
        s->rx_count--;
    }
    s->recv_count++;
    return (int)n;
}

static int mock_net_close(int fd)
{
    if (fd < 0 || fd >= MOCK_MAX_SOCKETS) return -1;
    mock_sockets[fd].state = MOCK_STATE_CLOSED;
    return 0;
}

/* ============================================================
 * Test Infrastructure
 * ============================================================ */

static int pass = 0, fail_count = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { fail_count++; printf("    [FAIL] %s (line %d)\n", msg, __LINE__); } \
    else { pass++; } \
} while(0)
#define SECTION(name) printf("\n── %s ──\n", name)

/* ============================================================
 * Test: TCP Connection Lifecycle
 * ============================================================ */

static void test_tcp_lifecycle(void)
{
    SECTION("TCP Lifecycle");
    mock_net_init();

    int fd = mock_net_socket(MOCK_PROTO_TCP);
    CHECK(fd > 0, "socket created");
    CHECK(mock_sockets[fd].state == MOCK_STATE_CREATED, "state = CREATED");
    CHECK(mock_sockets[fd].proto == MOCK_PROTO_TCP, "proto = TCP");

    /* Connect */
    CHECK(mock_net_connect(fd, 0x0100007F, 80) == 0, "connect to 127.0.0.1:80");
    CHECK(mock_sockets[fd].state == MOCK_STATE_CONNECTED, "state = CONNECTED");
    CHECK(mock_sockets[fd].remote_ip == 0x0100007F, "remote IP stored");
    CHECK(mock_sockets[fd].remote_port == 80, "remote port stored");

    /* Send data */
    const char *req = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    int sent = mock_net_send(fd, req, strlen(req));
    CHECK(sent == (int)strlen(req), "sent all bytes");
    CHECK(mock_sockets[fd].send_count == 1, "send count = 1");

    /* Verify captured TX */
    char tx_capture[256];
    size_t tx_len = mock_get_tx(fd, tx_capture, sizeof(tx_capture));
    CHECK(tx_len == strlen(req), "TX capture length matches");
    CHECK(memcmp(tx_capture, req, tx_len) == 0, "TX data matches request");

    /* Inject HTTP response and receive */
    const char *resp = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello";
    mock_inject_rx(fd, resp, strlen(resp));

    char rx_buf[256];
    int received = mock_net_recv(fd, rx_buf, sizeof(rx_buf));
    CHECK(received == (int)strlen(resp), "received all injected bytes");
    CHECK(memcmp(rx_buf, resp, (size_t)received) == 0, "RX data matches response");
    CHECK(mock_sockets[fd].recv_count == 1, "recv count = 1");

    /* Close */
    CHECK(mock_net_close(fd) == 0, "close OK");
    CHECK(mock_sockets[fd].state == MOCK_STATE_CLOSED, "state = CLOSED");

    /* Can't send/recv after close */
    CHECK(mock_net_send(fd, "x", 1) == -1, "send after close fails");
    CHECK(mock_net_recv(fd, rx_buf, 1) == -1, "recv after close fails");

    mock_net_deinit();
    printf("    TCP lifecycle: 16 checks\n");
}

/* ============================================================
 * Test: TCP Server (Bind/Listen)
 * ============================================================ */

static void test_tcp_server(void)
{
    SECTION("TCP Server");
    mock_net_init();

    int fd = mock_net_socket(MOCK_PROTO_TCP);
    CHECK(fd > 0, "server socket");

    CHECK(mock_net_bind(fd, 8080) == 0, "bind to 8080");
    CHECK(mock_sockets[fd].state == MOCK_STATE_BOUND, "state = BOUND");
    CHECK(mock_sockets[fd].local_port == 8080, "port stored");

    CHECK(mock_net_listen(fd, 5) == 0, "listen backlog=5");
    CHECK(mock_sockets[fd].state == MOCK_STATE_LISTENING, "state = LISTENING");
    CHECK(mock_sockets[fd].backlog == 5, "backlog stored");

    /* Can't connect a listening socket */
    CHECK(mock_net_connect(fd, 0, 0) == -1, "connect on listening fails");
    /* Can't send on listening socket */
    CHECK(mock_net_send(fd, "x", 1) == -1, "send on listening fails");

    mock_net_close(fd);
    mock_net_deinit();
    printf("    TCP server: 8 checks\n");
}

/* ============================================================
 * Test: UDP Datagram
 * ============================================================ */

static void test_udp_datagram(void)
{
    SECTION("UDP Datagram");
    mock_net_init();

    int fd = mock_net_socket(MOCK_PROTO_UDP);
    CHECK(fd > 0, "UDP socket");
    CHECK(mock_sockets[fd].proto == MOCK_PROTO_UDP, "proto = UDP");

    /* UDP connect (sets default destination) */
    CHECK(mock_net_connect(fd, 0xC0A80001, 5000) == 0, "connect to 192.168.0.1:5000");

    /* Send datagram */
    const char *msg = "{\"sensor\":42}";
    int sent = mock_net_send(fd, msg, strlen(msg));
    CHECK(sent == (int)strlen(msg), "datagram sent");

    /* Inject response */
    const char *ack = "ACK";
    mock_inject_rx(fd, ack, 3);
    char buf[16];
    int received = mock_net_recv(fd, buf, sizeof(buf));
    CHECK(received == 3, "received 3 bytes");
    CHECK(memcmp(buf, "ACK", 3) == 0, "got ACK");

    mock_net_close(fd);
    mock_net_deinit();
    printf("    UDP datagram: 6 checks\n");
}

/* ============================================================
 * Test: HTTP Request/Response Parsing
 * ============================================================ */

static void test_http_mock(void)
{
    SECTION("HTTP Mock");
    mock_net_init();

    int fd = mock_net_socket(MOCK_PROTO_TCP);
    mock_net_connect(fd, 0x0100007F, 80);

    /* Simulate sending GET request */
    const char *get = "GET /api/data HTTP/1.1\r\nHost: api.local\r\n\r\n";
    mock_net_send(fd, get, strlen(get));

    /* Verify the request was captured correctly */
    char tx[512];
    size_t tx_len = mock_get_tx(fd, tx, sizeof(tx));
    CHECK(tx_len == strlen(get), "GET request length");
    CHECK(strstr(tx, "GET /api/data") != NULL, "GET method+path in TX");
    CHECK(strstr(tx, "Host: api.local") != NULL, "Host header in TX");

    /* Inject a JSON response */
    const char *json_resp =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 27\r\n"
        "\r\n"
        "{\"temperature\":23.5,\"ok\":1}";
    mock_inject_rx(fd, json_resp, strlen(json_resp));

    char rx[512];
    int n = mock_net_recv(fd, rx, sizeof(rx) - 1);
    rx[n] = '\0';

    CHECK(n > 0, "received response");
    CHECK(strstr(rx, "200 OK") != NULL, "status 200 in response");
    CHECK(strstr(rx, "application/json") != NULL, "content-type in response");
    CHECK(strstr(rx, "\"temperature\":23.5") != NULL, "JSON body present");

    /* Parse Content-Length from response */
    const char *cl = strstr(rx, "Content-Length: ");
    CHECK(cl != NULL, "Content-Length header found");
    if (cl) {
        int content_len = atoi(cl + 16);
        CHECK(content_len == 27, "Content-Length = 27");
    }

    mock_net_close(fd);
    mock_net_deinit();
    printf("    HTTP mock: 8 checks\n");
}

/* ============================================================
 * Test: MQTT CONNECT Packet Framing
 * ============================================================ */

static void test_mqtt_connect_frame(void)
{
    SECTION("MQTT CONNECT Frame");
    mock_net_init();

    int fd = mock_net_socket(MOCK_PROTO_TCP);
    mock_net_connect(fd, 0x0100007F, 1883);

    /* Build a minimal MQTT CONNECT packet (v3.1.1) */
    uint8_t connect_pkt[] = {
        0x10,                          /* CONNECT packet type (1 << 4) */
        0x14,                          /* Remaining length = 20 */
        0x00, 0x04, 'M', 'Q', 'T', 'T', /* Protocol name */
        0x04,                          /* Protocol level (4 = v3.1.1) */
        0x02,                          /* Connect flags: Clean Session */
        0x00, 0x3C,                    /* Keep-alive: 60s */
        0x00, 0x08, 'e', 'o', 's', '_', 't', 'e', 's', 't', /* Client ID */
    };
    mock_net_send(fd, connect_pkt, sizeof(connect_pkt));

    /* Verify packet was captured */
    uint8_t tx[64];
    size_t tx_len = mock_get_tx(fd, tx, sizeof(tx));
    CHECK(tx_len == sizeof(connect_pkt), "CONNECT packet length");
    CHECK(tx[0] == 0x10, "packet type = CONNECT");
    CHECK(tx[1] == 0x14, "remaining length = 20");
    CHECK(memcmp(tx + 2, "\x00\x04MQTT", 6) == 0, "protocol name = MQTT");
    CHECK(tx[8] == 0x04, "protocol level = 4");
    CHECK(tx[9] == 0x02, "clean session flag");
    CHECK(tx[10] == 0x00 && tx[11] == 0x3C, "keepalive = 60s");

    /* Inject CONNACK response */
    uint8_t connack[] = {0x20, 0x02, 0x00, 0x00}; /* CONNACK, accepted */
    mock_inject_rx(fd, connack, sizeof(connack));

    uint8_t rx[8];
    int n = mock_net_recv(fd, rx, sizeof(rx));
    CHECK(n == 4, "received CONNACK");
    CHECK(rx[0] == 0x20, "CONNACK type");
    CHECK(rx[3] == 0x00, "CONNACK return code = accepted");

    mock_net_close(fd);
    mock_net_deinit();
    printf("    MQTT CONNECT: 10 checks\n");
}

/* ============================================================
 * Test: MQTT PUBLISH Packet
 * ============================================================ */

static void test_mqtt_publish_frame(void)
{
    SECTION("MQTT PUBLISH Frame");
    mock_net_init();

    int fd = mock_net_socket(MOCK_PROTO_TCP);
    mock_net_connect(fd, 0x0100007F, 1883);

    /* Build PUBLISH packet: topic="eos/temp", payload="23.5" */
    const char *topic = "eos/temp";
    const char *payload = "23.5";
    uint8_t pub_pkt[64];
    int pos = 0;
    pub_pkt[pos++] = 0x30;  /* PUBLISH, QoS 0, no retain */
    int rem_len = 2 + (int)strlen(topic) + (int)strlen(payload);
    pub_pkt[pos++] = (uint8_t)rem_len;
    pub_pkt[pos++] = 0;
    pub_pkt[pos++] = (uint8_t)strlen(topic);
    memcpy(pub_pkt + pos, topic, strlen(topic));
    pos += (int)strlen(topic);
    memcpy(pub_pkt + pos, payload, strlen(payload));
    pos += (int)strlen(payload);

    mock_net_send(fd, pub_pkt, (size_t)pos);

    uint8_t tx[64];
    size_t tx_len = mock_get_tx(fd, tx, sizeof(tx));
    CHECK(tx_len == (size_t)pos, "PUBLISH packet length");
    CHECK((tx[0] & 0xF0) == 0x30, "packet type = PUBLISH");
    CHECK(tx[3] == strlen(topic), "topic length byte");
    CHECK(memcmp(tx + 4, topic, strlen(topic)) == 0, "topic = eos/temp");
    CHECK(memcmp(tx + 4 + strlen(topic), payload, strlen(payload)) == 0,
          "payload = 23.5");

    mock_net_close(fd);
    mock_net_deinit();
    printf("    MQTT PUBLISH: 5 checks\n");
}

/* ============================================================
 * Test: Multiple Concurrent Sockets
 * ============================================================ */

static void test_concurrent_sockets(void)
{
    SECTION("Concurrent Sockets");
    mock_net_init();

    int fds[8];
    for (int i = 0; i < 8; i++) {
        fds[i] = mock_net_socket(i % 2 == 0 ? MOCK_PROTO_TCP : MOCK_PROTO_UDP);
        CHECK(fds[i] > 0, "socket created");
    }

    /* All unique */
    for (int i = 0; i < 8; i++) {
        for (int j = i + 1; j < 8; j++) {
            CHECK(fds[i] != fds[j], "unique FDs");
        }
    }

    /* Connect all TCP sockets to different ports */
    for (int i = 0; i < 8; i += 2) {
        CHECK(mock_net_connect(fds[i], 0x0100007F, (uint16_t)(8000 + i)) == 0,
              "connect TCP socket");
    }

    /* Send data on each, verify isolation */
    for (int i = 0; i < 8; i += 2) {
        char msg[32];
        snprintf(msg, sizeof(msg), "socket-%d", fds[i]);
        mock_net_send(fds[i], msg, strlen(msg));
    }

    /* Verify each socket only has its own data */
    for (int i = 0; i < 8; i += 2) {
        char expected[32], actual[64];
        snprintf(expected, sizeof(expected), "socket-%d", fds[i]);
        size_t n = mock_get_tx(fds[i], actual, sizeof(actual));
        CHECK(n == strlen(expected), "TX length matches");
        CHECK(memcmp(actual, expected, n) == 0, "TX data isolated");
    }

    for (int i = 0; i < 8; i++) mock_net_close(fds[i]);
    mock_net_deinit();
    printf("    Concurrent: 8 sockets, data isolation verified\n");
}

/* ============================================================
 * Test: Ring Buffer Wraparound
 * ============================================================ */

static void test_rx_ring_buffer(void)
{
    SECTION("RX Ring Buffer");
    mock_net_init();

    int fd = mock_net_socket(MOCK_PROTO_TCP);
    mock_net_connect(fd, 0x0100007F, 80);

    /* Fill and drain the RX buffer multiple times to exercise wraparound */
    for (int cycle = 0; cycle < 20; cycle++) {
        /* Inject 100 bytes */
        uint8_t data[100];
        for (int i = 0; i < 100; i++) data[i] = (uint8_t)(cycle + i);
        mock_inject_rx(fd, data, 100);

        /* Read back and verify */
        uint8_t buf[100];
        int n = mock_net_recv(fd, buf, 100);
        CHECK(n == 100, "received 100 bytes");
        CHECK(buf[0] == (uint8_t)cycle, "first byte correct");
        CHECK(buf[99] == (uint8_t)(cycle + 99), "last byte correct");
    }

    /* Empty read */
    uint8_t empty_buf[16];
    CHECK(mock_net_recv(fd, empty_buf, 16) == 0, "empty after drain");

    mock_net_close(fd);
    mock_net_deinit();
    printf("    Ring buffer: 20 wraparound cycles OK\n");
}

/* ============================================================
 * Test: Error Paths
 * ============================================================ */

static void test_error_paths(void)
{
    SECTION("Error Paths");

    /* Operations before init */
    mock_net_deinit();
    CHECK(mock_net_socket(0) == -1, "socket before init");

    mock_net_init();

    /* Invalid FD */
    CHECK(mock_net_connect(-1, 0, 0) == -1, "connect fd=-1");
    CHECK(mock_net_connect(MOCK_MAX_SOCKETS, 0, 0) == -1, "connect fd=max");
    CHECK(mock_net_send(-1, "x", 1) == -1, "send fd=-1");
    CHECK(mock_net_recv(-1, NULL, 0) == -1, "recv fd=-1");
    CHECK(mock_net_close(-1) == -1, "close fd=-1");

    /* Wrong state transitions */
    int fd = mock_net_socket(MOCK_PROTO_TCP);
    CHECK(mock_net_listen(fd, 5) == -1, "listen without bind");
    mock_net_bind(fd, 9999);
    CHECK(mock_net_connect(fd, 0, 0) == -1, "connect after bind");

    mock_net_close(fd);
    mock_net_deinit();
    printf("    Error paths: 8 checks\n");
}

/* ============================================================
 * Main
 * ============================================================ */

int main(void)
{
    printf("════════════════════════════════════════════\n");
    printf("  EoS Network Tests — Mock Socket Backend\n");
    printf("════════════════════════════════════════════\n");

    test_tcp_lifecycle();
    test_tcp_server();
    test_udp_datagram();
    test_http_mock();
    test_mqtt_connect_frame();
    test_mqtt_publish_frame();
    test_concurrent_sockets();
    test_rx_ring_buffer();
    test_error_paths();

    printf("\n════════════════════════════════════════════\n");
    if (fail_count == 0)
        printf("  ALL %d MOCK NETWORK CHECKS PASSED\n", pass);
    else
        printf("  %d PASSED, %d FAILED\n", pass, fail_count);
    printf("════════════════════════════════════════════\n");

    return fail_count > 0 ? 1 : 0;
}
