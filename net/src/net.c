// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file net.c
 * @brief EoS Networking ΓÇö Stub implementation
 */

#include <eos/net.h>
#include <string.h>
#include <time.h>

#include "mqtt_codec.h"

#if EOS_ENABLE_NET

/* The socket layer below is the freestanding fallback. On a host, ADR-014
 * point 3 maps this API onto POSIX sockets instead; CMake compiles
 * net_posix.c and defines EOS_NET_POSIX, and these stubs step aside so there
 * is exactly one definition of each symbol.
 *
 * Left returning -1 rather than deleted: on a target with no IP stack yet,
 * failing is the honest answer, and it is what ADR-014 records as the
 * Planned state for the Nano and Edge profiles until lwIP lands. */
#ifndef EOS_NET_POSIX

static bool net_initialized = false;

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

#ifndef EOS_MQTT_MAX_SUBSCRIPTIONS
#define EOS_MQTT_MAX_SUBSCRIPTIONS 8
#endif

#ifndef EOS_MQTT_MAX_TOPIC_LEN
#define EOS_MQTT_MAX_TOPIC_LEN 128
#endif

#ifndef EOS_MQTT_BUFFER_SIZE
#define EOS_MQTT_BUFFER_SIZE 1024
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
    bool in_use;
    char topic[EOS_MQTT_MAX_TOPIC_LEN];
    uint8_t qos;
    eos_mqtt_msg_callback_t cb;
    void *ctx;
} mqtt_sub_t;

typedef struct {
    mqtt_state_t state;
    eos_socket_t sock;
    uint16_t keepalive_sec;
    uint32_t last_send_sec;
    uint32_t last_recv_sec;
    bool ping_pending;
    uint32_t ping_sent_ms;
    uint16_t next_packet_id;
    bool in_callback;
    mqtt_sub_t subscriptions[EOS_MQTT_MAX_SUBSCRIPTIONS];
    struct {
        bool in_flight;
        uint16_t packet_id;
        uint8_t pkt[EOS_MQTT_BUFFER_SIZE];
        size_t pkt_len;
    } pending_pub;
    uint8_t rx_buf[EOS_MQTT_BUFFER_SIZE];
    uint8_t tx_buf[EOS_MQTT_BUFFER_SIZE];
} mqtt_client_t;

static mqtt_client_t mqtt_clients[EOS_MQTT_MAX_CLIENTS];

static uint32_t mqtt_now_sec(void)
{
#if defined(CLOCK_MONOTONIC)
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint32_t)ts.tv_sec;
    }
#endif
    return (uint32_t)time(NULL);
}

static uint32_t mqtt_now_ms(void)
{
#if defined(CLOCK_MONOTONIC)
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    }
#endif
    return (uint32_t)(time(NULL) * 1000);
}

static uint32_t mqtt_time_rem_ms(uint32_t start_ms, uint32_t timeout_ms)
{
    uint32_t now = mqtt_now_ms();
    uint32_t elapsed = (now >= start_ms) ? (now - start_ms) : (0xFFFFFFFFu - start_ms + now);
    if (elapsed >= timeout_ms) return 0;
    return timeout_ms - elapsed;
}

/* The config strings are fixed arrays the caller fills; one without a
 * terminator would send strlen() past the end of the struct. */
static bool mqtt_field_ok(const char *field, size_t size)
{
    return memchr(field, '\0', size) != NULL;
}

/* Note: The eos_net socket abstraction (eos_net_send in net/include/eos/net.h)
 * does not support a send timeout parameter. A blocking socket send will block
 * until data is accepted by the underlying network stack. To avoid unbounded
 * hangs, higher-level timeouts or non-blocking socket extensions would require
 * modifying net.h, which is outside the scope of Issue #259. */
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

/* Note: eos_net_recv() returns >0 on byte count, 0 on timeout,
 * EOS_NET_EOF (-2) on orderly TCP close by peer, and -1 on error.
 * In any non-positive case (<=0), expected packet bytes did not arrive,
 * so the read aborts and fails the packet.
 *
 * All packet reads are bounded by a single overall deadline calculated from
 * start_ms and timeout_ms, ensuring slow or partial reads cannot exceed the
 * caller's specified timeout budget. */
static int mqtt_recv_exact_deadline(eos_socket_t sock, uint8_t *buf, size_t len,
                                    uint32_t start_ms, uint32_t timeout_ms)
{
    while (len > 0) {
        uint32_t rem = mqtt_time_rem_ms(start_ms, timeout_ms);
        if (rem == 0) return -1;
        int n = eos_net_recv(sock, buf, len, rem);
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
    c->keepalive_sec = 0;
    c->last_send_sec = 0;
    c->last_recv_sec = 0;
    c->ping_pending = false;
    c->ping_sent_ms = 0;
    c->next_packet_id = 0;
    c->in_callback = false;
    memset(c->subscriptions, 0, sizeof(c->subscriptions));
    memset(&c->pending_pub, 0, sizeof(c->pending_pub));
}

/* RFC 3629 UTF-8 validator enforcing MQTT 3.1.1 section 1.5.3:
 * well-formed UTF-8 and MUST NOT include null character U+0000 (MQTT-1.5.3-2). */
static bool mqtt_utf8_valid(const char *data, size_t len)
{
    if (!data) return false;
    const uint8_t *p = (const uint8_t *)data;
    size_t i = 0;
    while (i < len) {
        uint8_t c = p[i++];
        if (c == 0x00) {
            /* MQTT-1.5.3-2: MUST NOT include null character U+0000 */
            return false;
        }
        if (c <= 0x7F) {
            continue;
        } else if (c >= 0xC2 && c <= 0xDF) {
            if (i >= len) return false;
            uint8_t c2 = p[i++];
            if ((c2 & 0xC0) != 0x80) return false;
        } else if (c == 0xE0) {
            if (i + 1 >= len) return false;
            uint8_t c2 = p[i++];
            uint8_t c3 = p[i++];
            if (c2 < 0xA0 || c2 > 0xBF) return false;
            if ((c3 & 0xC0) != 0x80) return false;
        } else if ((c >= 0xE1 && c <= 0xEC) || c == 0xEE || c == 0xEF) {
            if (i + 1 >= len) return false;
            uint8_t c2 = p[i++];
            uint8_t c3 = p[i++];
            if ((c2 & 0xC0) != 0x80) return false;
            if ((c3 & 0xC0) != 0x80) return false;
        } else if (c == 0xED) {
            /* Reject UTF-16 surrogates U+D800..U+DFFF */
            if (i + 1 >= len) return false;
            uint8_t c2 = p[i++];
            uint8_t c3 = p[i++];
            if (c2 < 0x80 || c2 > 0x9F) return false;
            if ((c3 & 0xC0) != 0x80) return false;
        } else if (c == 0xF0) {
            if (i + 2 >= len) return false;
            uint8_t c2 = p[i++];
            uint8_t c3 = p[i++];
            uint8_t c4 = p[i++];
            if (c2 < 0x90 || c2 > 0xBF) return false;
            if ((c3 & 0xC0) != 0x80) return false;
            if ((c4 & 0xC0) != 0x80) return false;
        } else if (c >= 0xF1 && c <= 0xF3) {
            if (i + 2 >= len) return false;
            uint8_t c2 = p[i++];
            uint8_t c3 = p[i++];
            uint8_t c4 = p[i++];
            if ((c2 & 0xC0) != 0x80) return false;
            if ((c3 & 0xC0) != 0x80) return false;
            if ((c4 & 0xC0) != 0x80) return false;
        } else if (c == 0xF4) {
            if (i + 2 >= len) return false;
            uint8_t c2 = p[i++];
            uint8_t c3 = p[i++];
            uint8_t c4 = p[i++];
            if (c2 < 0x80 || c2 > 0x8F) return false;
            if ((c3 & 0xC0) != 0x80) return false;
            if ((c4 & 0xC0) != 0x80) return false;
        } else {
            return false;
        }
    }
    return true;
}

static bool mqtt_topic_name_valid(const char *topic)
{
    if (topic == NULL || topic[0] == '\0') return false;
    size_t len = strlen(topic);
    if (len > EOS_MQTT_MAX_TOPIC_LEN - 1) return false;
    if (!mqtt_utf8_valid(topic, len)) return false;
    return strchr(topic, '+') == NULL && strchr(topic, '#') == NULL;
}

static bool mqtt_topic_filter_valid(const char *filter)
{
    if (filter == NULL || filter[0] == '\0') return false;
    size_t len = strlen(filter);
    if (len > EOS_MQTT_MAX_TOPIC_LEN - 1) return false;
    if (!mqtt_utf8_valid(filter, len)) return false;

    for (size_t i = 0; i < len; i++) {
        if (filter[i] == '#') {
            if (i != len - 1) return false;
            if (i > 0 && filter[i - 1] != '/') return false;
        } else if (filter[i] == '+') {
            if (i > 0 && filter[i - 1] != '/') return false;
            if (i + 1 < len && filter[i + 1] != '/') return false;
        }
    }
    return true;
}

static bool mqtt_topic_matches(const char *filter, const char *topic)
{
    if (!filter || !topic) return false;
    if (topic[0] == '$' && (filter[0] == '+' || filter[0] == '#')) return false;

    while (*filter && *topic) {
        if (*filter == '#') {
            return *(filter + 1) == '\0';
        }
        if (*filter == '+') {
            while (*topic && *topic != '/') {
                topic++;
            }
            filter++;
            if (*filter == '/' && *topic == '/') {
                filter++;
                topic++;
                continue;
            }
            if (*filter == '\0' && *topic == '\0') {
                return true;
            }
            if (*filter == '/' && *(filter + 1) == '#' && *topic == '\0') {
                return true;
            }
            return false;
        }
        if (*filter != *topic) {
            return false;
        }
        filter++;
        topic++;
    }

    if (*filter == '#') {
        return *(filter + 1) == '\0';
    }
    if (*filter == '/' && *(filter + 1) == '#' && *(filter + 2) == '\0' && *topic == '\0') {
        return true;
    }
    /* If filter has a trailing '+' after a slash and topic ended with a slash,
     * the '+' matches the trailing empty level. (e.g. "sport/+" matches "sport/") */
    if (*filter == '+' && *(filter + 1) == '\0' && *topic == '\0') {
        return true;
    }

    return *filter == '\0' && *topic == '\0';
}

static uint16_t mqtt_next_packet_id(mqtt_client_t *c)
{
    c->next_packet_id++;
    if (c->next_packet_id == 0) {
        c->next_packet_id = 1;
    }
    return c->next_packet_id;
}

static int mqtt_process_one_packet(mqtt_client_t *c, uint32_t timeout_ms,
                                   bool *got_packet, uint8_t *out_type,
                                   uint16_t *out_pid, uint8_t *out_code);

static int mqtt_keepalive_prepare(mqtt_client_t *c, uint32_t *wait_ms)
{
    if (c->keepalive_sec == 0) return 0;
    uint32_t now_sec = mqtt_now_sec();
    uint32_t keepalive_ms = (uint32_t)c->keepalive_sec * 1000u;

    if (!c->ping_pending) {
        if (now_sec - c->last_send_sec >= c->keepalive_sec) {
            uint8_t ping[2];
            int n = eos_mqtt_encode_pingreq(ping, sizeof(ping));
            if (n <= 0 || mqtt_send_all(c->sock, ping, (size_t)n) != 0) {
                mqtt_release(c);
                return -1;
            }
            c->last_send_sec = now_sec;
            c->ping_pending = true;
            c->ping_sent_ms = mqtt_now_ms();
        }
    }

    if (wait_ms) {
        if (c->ping_pending) {
            uint32_t now_ms = mqtt_now_ms();
            uint32_t elapsed_ms = (now_ms >= c->ping_sent_ms) ? (now_ms - c->ping_sent_ms)
                                                              : (0xFFFFFFFFu - c->ping_sent_ms + now_ms);
            uint32_t rem_ms = (elapsed_ms < keepalive_ms) ? (keepalive_ms - elapsed_ms) : 0;
            if (rem_ms < *wait_ms) {
                *wait_ms = (rem_ms > 0) ? rem_ms : 1;
            }
        } else {
            uint32_t elapsed_sec = now_sec - c->last_send_sec;
            uint32_t rem_sec = (elapsed_sec < c->keepalive_sec) ? (c->keepalive_sec - elapsed_sec) : 0;
            uint32_t rem_ms = rem_sec * 1000u;
            if (rem_ms < *wait_ms) {
                *wait_ms = (rem_ms > 0) ? rem_ms : 1;
            }
        }
    }
    return 0;
}

static int mqtt_keepalive_check_deadline(mqtt_client_t *c)
{
    if (c->keepalive_sec == 0 || !c->ping_pending) return 0;
    uint32_t now_ms = mqtt_now_ms();
    uint32_t keepalive_ms = (uint32_t)c->keepalive_sec * 1000u;
    uint32_t elapsed_ms = (now_ms >= c->ping_sent_ms) ? (now_ms - c->ping_sent_ms)
                                                      : (0xFFFFFFFFu - c->ping_sent_ms + now_ms);

    if (elapsed_ms >= keepalive_ms) {
        /* Drain any queued input before declaring PINGRESP missed */
        bool got_pkt = false;
        while (mqtt_process_one_packet(c, 0, &got_pkt, NULL, NULL, NULL) == 0 && got_pkt && c->ping_pending) {
        }
        if (c->ping_pending) {
            /* Missed PINGRESP within keepalive window */
            mqtt_release(c);
            return -1;
        }
    }
    return 0;
}

static int mqtt_process_one_packet(mqtt_client_t *c, uint32_t timeout_ms,
                                   bool *got_packet, uint8_t *out_type,
                                   uint16_t *out_pid, uint8_t *out_code)
{
    if (got_packet) *got_packet = false;
    if (out_type) *out_type = 0;
    if (out_pid) *out_pid = 0;
    if (out_code) *out_code = 0;

    int n = eos_net_recv(c->sock, &c->rx_buf[0], 1, timeout_ms);
    if (n == 0) {
        return 0;
    }
    if (n < 0) {
        mqtt_release(c);
        return -1;
    }

    if (got_packet) *got_packet = true;
    uint32_t pkt_start_ms = mqtt_now_ms();
    uint32_t pkt_budget_ms = (timeout_ms > 0) ? timeout_ms : EOS_MQTT_CONNACK_TIMEOUT_MS;

    size_t rl_count = 0;
    while (rl_count < 4) {
        uint32_t rem = mqtt_time_rem_ms(pkt_start_ms, pkt_budget_ms);
        if (rem == 0) {
            mqtt_release(c);
            return -1;
        }
        n = eos_net_recv(c->sock, &c->rx_buf[1 + rl_count], 1, rem);
        if (n <= 0) {
            mqtt_release(c);
            return -1;
        }
        rl_count++;
        if ((c->rx_buf[rl_count] & 0x80u) == 0) {
            break;
        }
    }

    uint32_t remaining_len = 0;
    size_t rl_used = 0;
    eos_mqtt_status_t st = eos_mqtt_decode_remaining_length(
        &c->rx_buf[1], rl_count, &remaining_len, &rl_used);
    if (st != EOS_MQTT_OK || rl_used != rl_count) {
        mqtt_release(c);
        return -1;
    }

    size_t header_len = 1 + rl_count;
    if (remaining_len > sizeof(c->rx_buf) - header_len) {
        mqtt_release(c);
        return -1;
    }

    if (remaining_len > 0) {
        if (mqtt_recv_exact_deadline(c->sock, &c->rx_buf[header_len], remaining_len,
                                     pkt_start_ms, pkt_budget_ms) != 0) {
            mqtt_release(c);
            return -1;
        }
    }

    size_t total_len = header_len + remaining_len;
    c->last_recv_sec = mqtt_now_sec();

    uint8_t flags = c->rx_buf[0] & 0x0Fu;
    eos_mqtt_header_t hdr;
    st = eos_mqtt_parse_header(c->rx_buf, total_len, &hdr);
    if (st != EOS_MQTT_OK) {
        mqtt_release(c);
        return -1;
    }

    /* MQTT 3.1.1 Table 2.2 flag bits and packet length validation */
    switch (hdr.type) {
    case EOS_MQTT_PKT_PUBLISH:
        if (((flags >> 1) & 0x03u) == 3) {
            /* Reserved QoS value 3 */
            mqtt_release(c);
            return -1;
        }
        if (((flags >> 1) & 0x03u) == 0 && (flags & 0x08u) != 0) {
            /* DUP flag MUST be 0 for QoS 0 (MQTT-3.3.1-2) */
            mqtt_release(c);
            return -1;
        }
        break;
    case EOS_MQTT_PKT_PUBACK:
        if (flags != 0x00 || remaining_len != 2) {
            mqtt_release(c);
            return -1;
        }
        break;
    case EOS_MQTT_PKT_SUBACK:
        if (flags != 0x00 || remaining_len < 3) {
            mqtt_release(c);
            return -1;
        }
        break;
    case EOS_MQTT_PKT_UNSUBACK:
        if (flags != 0x00 || remaining_len != 2) {
            mqtt_release(c);
            return -1;
        }
        break;
    case EOS_MQTT_PKT_PINGRESP:
        if (flags != 0x00 || remaining_len != 0) {
            mqtt_release(c);
            return -1;
        }
        break;
    case EOS_MQTT_PKT_PINGREQ:
        if (flags != 0x00 || remaining_len != 0) {
            mqtt_release(c);
            return -1;
        }
        break;
    default:
        mqtt_release(c);
        return -1;
    }

    if (out_type) *out_type = hdr.type;

    switch (hdr.type) {
    case EOS_MQTT_PKT_PUBLISH: {
        eos_mqtt_publish_t pub;
        st = eos_mqtt_decode_publish(c->rx_buf, total_len, &pub);
        if (st != EOS_MQTT_OK) {
            mqtt_release(c);
            return -1;
        }
        if (pub.qos == 2) {
            /* Design doc section 5: QoS 2 is a protocol error, fail connection */
            mqtt_release(c);
            return -1;
        }
        if (out_pid) *out_pid = pub.packet_id;

        char topic_str[EOS_MQTT_MAX_TOPIC_LEN];
        if (pub.topic_len == 0 || pub.topic_len >= sizeof(topic_str)) {
            mqtt_release(c);
            return -1;
        }
        if (!mqtt_utf8_valid(pub.topic, pub.topic_len)) {
            mqtt_release(c);
            return -1;
        }
        if (memchr(pub.topic, '+', pub.topic_len) != NULL ||
            memchr(pub.topic, '#', pub.topic_len) != NULL) {
            /* Topic Name in PUBLISH packet MUST NOT contain wildcard characters (MQTT-3.3.2-2) */
            mqtt_release(c);
            return -1;
        }
        memcpy(topic_str, pub.topic, pub.topic_len);
        topic_str[pub.topic_len] = '\0';

        /* Acknowledge QoS 1 only AFTER validating topic safely fits */
        if (pub.qos == 1) {
            uint8_t puback[4];
            int ack_len = eos_mqtt_encode_puback(puback, sizeof(puback), pub.packet_id);
            if (ack_len <= 0 || mqtt_send_all(c->sock, puback, (size_t)ack_len) != 0) {
                mqtt_release(c);
                return -1;
            }
            c->last_send_sec = mqtt_now_sec();
        }

        c->in_callback = true;
        for (int i = 0; i < EOS_MQTT_MAX_SUBSCRIPTIONS; i++) {
            if (c->subscriptions[i].in_use &&
                mqtt_topic_matches(c->subscriptions[i].topic, topic_str)) {
                if (c->subscriptions[i].cb) {
                    c->subscriptions[i].cb(topic_str, pub.payload, pub.payload_len,
                                           c->subscriptions[i].ctx);
                }
            }
        }
        c->in_callback = false;
        return 0;
    }
    case EOS_MQTT_PKT_PUBACK: {
        uint16_t pid = 0;
        st = eos_mqtt_decode_packet_id(c->rx_buf, total_len, EOS_MQTT_PKT_PUBACK, &pid);
        if (st != EOS_MQTT_OK) {
            mqtt_release(c);
            return -1;
        }
        if (out_pid) *out_pid = pid;
        if (c->pending_pub.in_flight && c->pending_pub.packet_id == pid) {
            c->pending_pub.in_flight = false;
        }
        return 0;
    }
    case EOS_MQTT_PKT_SUBACK: {
        eos_mqtt_suback_t suback;
        st = eos_mqtt_decode_suback(c->rx_buf, total_len, &suback);
        if (st != EOS_MQTT_OK) {
            mqtt_release(c);
            return -1;
        }
        if (out_pid) *out_pid = suback.packet_id;
        if (out_code) *out_code = suback.return_code;
        return 0;
    }
    case EOS_MQTT_PKT_UNSUBACK: {
        uint16_t pid = 0;
        st = eos_mqtt_decode_packet_id(c->rx_buf, total_len, EOS_MQTT_PKT_UNSUBACK, &pid);
        if (st != EOS_MQTT_OK) {
            mqtt_release(c);
            return -1;
        }
        if (out_pid) *out_pid = pid;
        return 0;
    }
    case EOS_MQTT_PKT_PINGRESP: {
        c->ping_pending = false;
        return 0;
    }
    case EOS_MQTT_PKT_PINGREQ: {
        uint8_t resp[2] = { (uint8_t)(EOS_MQTT_PKT_PINGRESP << 4), 0x00 };
        if (mqtt_send_all(c->sock, resp, sizeof(resp)) != 0) {
            mqtt_release(c);
            return -1;
        }
        c->last_send_sec = mqtt_now_sec();
        return 0;
    }
    default:
        mqtt_release(c);
        return -1;
    }
}

static int mqtt_wait_for_ack(mqtt_client_t *c, uint8_t expected_type,
                             uint16_t expected_pid, uint32_t timeout_ms,
                             uint8_t *out_code)
{
    uint32_t start_ms = mqtt_now_ms();
    bool retried = false;
    uint32_t retry_threshold_ms = timeout_ms / 2;

    for (;;) {
        uint32_t rem_ms = mqtt_time_rem_ms(start_ms, timeout_ms);
        if (rem_ms == 0) {
            /* Timeout waiting for ack: invalidate session fail-closed */
            mqtt_release(c);
            return -1;
        }

        if (mqtt_keepalive_prepare(c, &rem_ms) != 0) {
            return -1;
        }

        /* QoS 1 retry with DUP=1 if initial wait exceeded retry threshold */
        if (!retried && expected_type == EOS_MQTT_PKT_PUBACK && c->pending_pub.in_flight) {
            uint32_t now = mqtt_now_ms();
            uint32_t elapsed = (now >= start_ms) ? (now - start_ms) : (0xFFFFFFFFu - start_ms + now);
            if (elapsed >= retry_threshold_ms) {
                retried = true;
                c->pending_pub.pkt[0] |= 0x08u; /* DUP = 1 */
                if (mqtt_send_all(c->sock, c->pending_pub.pkt, c->pending_pub.pkt_len) != 0) {
                    mqtt_release(c);
                    return -1;
                }
                c->last_send_sec = mqtt_now_sec();
            } else {
                uint32_t time_to_retry = retry_threshold_ms - elapsed;
                if (time_to_retry < rem_ms) {
                    rem_ms = time_to_retry;
                }
            }
        }

        bool got_pkt = false;
        uint8_t pkt_type = 0;
        uint16_t pkt_pid = 0;
        uint8_t code = 0;

        int rc = mqtt_process_one_packet(c, rem_ms, &got_pkt, &pkt_type, &pkt_pid, &code);
        if (rc < 0) {
            return -1;
        }
        if (mqtt_keepalive_check_deadline(c) != 0) {
            return -1;
        }
        if (got_pkt && pkt_type == expected_type && pkt_pid == expected_pid) {
            if (out_code) *out_code = code;
            return 0;
        }
    }
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
    uint32_t hs_start = mqtt_now_ms();
    if (mqtt_recv_exact_deadline(c->sock, ack_buf, sizeof(ack_buf),
                                 hs_start, EOS_MQTT_CONNACK_TIMEOUT_MS) != 0) {
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
    for (int i = 0; i < EOS_MQTT_MAX_CLIENTS; i++) {
        if (mqtt_clients[i].in_callback) {
            return EOS_MQTT_INVALID;
        }
    }
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
    c->keepalive_sec = cfg->keepalive_sec;
    uint32_t now = mqtt_now_sec();
    c->last_send_sec = now;
    c->last_recv_sec = now;
    c->ping_pending = false;
    c->ping_sent_ms = 0;
    c->next_packet_id = 0;
    c->in_callback = false;
    memset(c->subscriptions, 0, sizeof(c->subscriptions));
    memset(&c->pending_pub, 0, sizeof(c->pending_pub));
    return (eos_mqtt_handle_t)idx;
}

int eos_mqtt_disconnect(eos_mqtt_handle_t handle)
{
    if (handle < 0 || handle >= EOS_MQTT_MAX_CLIENTS) return -1;
    mqtt_client_t *c = &mqtt_clients[handle];
    if (c->state != MQTT_CONNECTED || c->in_callback) return -1;
    for (int i = 0; i < EOS_MQTT_MAX_CLIENTS; i++) {
        if (mqtt_clients[i].in_callback) {
            return -1;
        }
    }

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

int eos_mqtt_publish(eos_mqtt_handle_t handle, const char *topic,
                     const void *payload, size_t len, uint8_t qos)
{
    if (handle < 0 || handle >= EOS_MQTT_MAX_CLIENTS) return -1;
    mqtt_client_t *c = &mqtt_clients[handle];
    if (c->state != MQTT_CONNECTED || c->in_callback) return -1;

    if (!mqtt_topic_name_valid(topic)) return -1;
    if (qos > 1) return -1;
    if (len > 0 && payload == NULL) return -1;

    uint16_t pid = 0;
    if (qos == 1) {
        pid = mqtt_next_packet_id(c);
    }

    int pkt_len = eos_mqtt_encode_publish(c->tx_buf, sizeof(c->tx_buf),
                                          topic, (const uint8_t *)payload, len,
                                          qos, false, false, pid);
    if (pkt_len < 0) return -1;

    if (mqtt_send_all(c->sock, c->tx_buf, (size_t)pkt_len) != 0) {
        mqtt_release(c);
        return -1;
    }
    c->last_send_sec = mqtt_now_sec();

    if (qos == 0) {
        return 0;
    }

    /* QoS 1: store packet for potential DUP=1 retry, wait for PUBACK */
    c->pending_pub.in_flight = true;
    c->pending_pub.packet_id = pid;
    c->pending_pub.pkt_len = (size_t)pkt_len;
    memcpy(c->pending_pub.pkt, c->tx_buf, (size_t)pkt_len);

    if (mqtt_wait_for_ack(c, EOS_MQTT_PKT_PUBACK, pid,
                          EOS_MQTT_CONNACK_TIMEOUT_MS, NULL) != 0) {
        return -1;
    }
    return 0;
}

int eos_mqtt_subscribe(eos_mqtt_handle_t handle, const char *topic,
                       uint8_t qos, eos_mqtt_msg_callback_t cb, void *ctx)
{
    if (handle < 0 || handle >= EOS_MQTT_MAX_CLIENTS) return -1;
    mqtt_client_t *c = &mqtt_clients[handle];
    if (c->state != MQTT_CONNECTED || c->in_callback) return -1;

    if (!mqtt_topic_filter_valid(topic)) return -1;
    if (qos > 1) return -1;
    if (cb == NULL) return -1;

    int slot = -1;
    for (int i = 0; i < EOS_MQTT_MAX_SUBSCRIPTIONS; i++) {
        if (c->subscriptions[i].in_use && strcmp(c->subscriptions[i].topic, topic) == 0) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        for (int i = 0; i < EOS_MQTT_MAX_SUBSCRIPTIONS; i++) {
            if (!c->subscriptions[i].in_use) {
                slot = i;
                break;
            }
        }
    }
    if (slot < 0) {
        return -1;
    }

    /* B2: Stage subscription before waiting for SUBACK so that retained PUBLISH
     * messages arriving prior to SUBACK are delivered rather than dropped. */
    mqtt_sub_t old_sub = c->subscriptions[slot];
    strncpy(c->subscriptions[slot].topic, topic, sizeof(c->subscriptions[slot].topic) - 1);
    c->subscriptions[slot].topic[sizeof(c->subscriptions[slot].topic) - 1] = '\0';
    c->subscriptions[slot].qos = qos;
    c->subscriptions[slot].cb = cb;
    c->subscriptions[slot].ctx = ctx;
    c->subscriptions[slot].in_use = true;

    uint16_t pid = mqtt_next_packet_id(c);
    int pkt_len = eos_mqtt_encode_subscribe(c->tx_buf, sizeof(c->tx_buf),
                                            pid, topic, qos);
    if (pkt_len < 0) {
        c->subscriptions[slot] = old_sub;
        return -1;
    }

    if (mqtt_send_all(c->sock, c->tx_buf, (size_t)pkt_len) != 0) {
        mqtt_release(c);
        return -1;
    }
    c->last_send_sec = mqtt_now_sec();

    uint8_t suback_code = 0;
    if (mqtt_wait_for_ack(c, EOS_MQTT_PKT_SUBACK, pid,
                          EOS_MQTT_CONNACK_TIMEOUT_MS, &suback_code) != 0) {
        return -1;
    }
    if (suback_code > qos || suback_code > 1) {
        /* B4: Invalid SUBACK grant is a protocol violation; close and release session */
        mqtt_release(c);
        return -1;
    }

    c->subscriptions[slot].qos = suback_code;
    return 0;
}

int eos_mqtt_unsubscribe(eos_mqtt_handle_t handle, const char *topic)
{
    if (handle < 0 || handle >= EOS_MQTT_MAX_CLIENTS) return -1;
    mqtt_client_t *c = &mqtt_clients[handle];
    if (c->state != MQTT_CONNECTED || c->in_callback) return -1;

    if (!mqtt_topic_filter_valid(topic)) return -1;

    uint16_t pid = mqtt_next_packet_id(c);
    int pkt_len = eos_mqtt_encode_unsubscribe(c->tx_buf, sizeof(c->tx_buf),
                                              pid, topic);
    if (pkt_len < 0) return -1;

    if (mqtt_send_all(c->sock, c->tx_buf, (size_t)pkt_len) != 0) {
        mqtt_release(c);
        return -1;
    }
    c->last_send_sec = mqtt_now_sec();

    if (mqtt_wait_for_ack(c, EOS_MQTT_PKT_UNSUBACK, pid,
                          EOS_MQTT_CONNACK_TIMEOUT_MS, NULL) != 0) {
        return -1;
    }

    for (int i = 0; i < EOS_MQTT_MAX_SUBSCRIPTIONS; i++) {
        if (c->subscriptions[i].in_use && strcmp(c->subscriptions[i].topic, topic) == 0) {
            c->subscriptions[i].in_use = false;
            c->subscriptions[i].topic[0] = '\0';
            c->subscriptions[i].cb = NULL;
            c->subscriptions[i].ctx = NULL;
            break;
        }
    }
    return 0;
}

int eos_mqtt_loop(eos_mqtt_handle_t handle, uint32_t timeout_ms)
{
    if (handle < 0 || handle >= EOS_MQTT_MAX_CLIENTS) return -1;
    mqtt_client_t *c = &mqtt_clients[handle];
    if (c->state != MQTT_CONNECTED || c->in_callback) return -1;

    uint32_t wait_ms = timeout_ms;
    if (mqtt_keepalive_prepare(c, &wait_ms) != 0) {
        return -1;
    }

    bool got_pkt = false;
    int rc = mqtt_process_one_packet(c, wait_ms, &got_pkt, NULL, NULL, NULL);
    if (rc < 0) {
        return -1;
    }

    if (mqtt_keepalive_check_deadline(c) != 0) {
        return -1;
    }

    return 0;
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