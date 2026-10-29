// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file mqtt_codec.c
 * @brief Pure MQTT 3.1.1 packet codec — implementation.
 *
 * See mqtt_codec.h for the contract and docs/eos-mqtt-client-design.md
 * for the design (#199, slice 1: codec core + fixtures). The client
 * state machine that drives it lives in net.c.
 */

#include "mqtt_codec.h"

#include <string.h>

/* ================================================================
 * Remaining Length
 * ================================================================ */

size_t eos_mqtt_remaining_length_size(uint32_t len)
{
    if (len < 128u)        return 1;
    if (len < 16384u)      return 2;
    if (len < 2097152u)    return 3;
    if (len <= EOS_MQTT_MAX_REMAINING_LENGTH) return 4;
    return 0;
}

int eos_mqtt_encode_remaining_length(uint8_t out[4], uint32_t len)
{
    size_t n = eos_mqtt_remaining_length_size(len);
    if (n == 0) return -1;
    for (size_t i = 0; i < n; i++) {
        uint8_t byte = (uint8_t)(len % 128u);
        len /= 128u;
        if (i + 1 < n) byte |= 0x80u;
        out[i] = byte;
    }
    return (int)n;
}

eos_mqtt_status_t eos_mqtt_decode_remaining_length(const uint8_t *buf,
                                                   size_t len,
                                                   uint32_t *value,
                                                   size_t *used)
{
    uint32_t result = 0;
    uint32_t multiplier = 1;
    for (size_t i = 0; i < 4; i++) {
        if (i >= len) return EOS_MQTT_INCOMPLETE;
        uint8_t byte = buf[i];
        result += (uint32_t)(byte & 0x7Fu) * multiplier;
        if ((byte & 0x80u) == 0) {
            /* The sender MUST NOT use a non-minimal encoding
             * (MQTT-2.2.3-2): with more than one byte, the most
             * significant group must be non-zero. */
            if (i > 0 && (byte & 0x7Fu) == 0) return EOS_MQTT_MALFORMED;
            *value = result;
            *used = i + 1;
            return EOS_MQTT_OK;
        }
        multiplier *= 128u;
    }
    /* Continuation bit still set on the fourth byte: the encoding
     * would need a fifth byte, which 3.1.1 forbids. */
    return EOS_MQTT_MALFORMED;
}

/* ================================================================
 * Fixed header
 * ================================================================ */

eos_mqtt_status_t eos_mqtt_parse_header(const uint8_t *buf, size_t len,
                                        eos_mqtt_header_t *hdr)
{
    if (len < 2) return EOS_MQTT_INCOMPLETE;
    uint32_t remaining = 0;
    size_t rl_used = 0;
    eos_mqtt_status_t st =
        eos_mqtt_decode_remaining_length(buf + 1, len - 1, &remaining, &rl_used);
    if (st != EOS_MQTT_OK) return st;
    size_t header_len = 1 + rl_used;
    if (len - header_len < remaining) return EOS_MQTT_INCOMPLETE;
    hdr->type = (uint8_t)(buf[0] >> 4);
    hdr->flags = (uint8_t)(buf[0] & 0x0Fu);
    hdr->remaining_length = remaining;
    hdr->header_len = header_len;
    return EOS_MQTT_OK;
}

/* ================================================================
 * Encode helpers
 * ================================================================ */

/* Length-prefixed UTF-8 string field size, or 0 when unusable. */
static size_t string_field_size(const char *s)
{
    size_t n = strlen(s);
    if (n > 0xFFFFu) return 0;
    return 2 + n;
}

static uint8_t *write_string(uint8_t *p, const char *s)
{
    size_t n = strlen(s);
    *p++ = (uint8_t)(n >> 8);
    *p++ = (uint8_t)(n & 0xFFu);
    memcpy(p, s, n);
    return p + n;
}

static uint8_t *write_u16(uint8_t *p, uint16_t v)
{
    *p++ = (uint8_t)(v >> 8);
    *p++ = (uint8_t)(v & 0xFFu);
    return p;
}

/* Frame a packet: fixed header byte + Remaining Length + body size
 * check. Returns the total length, or -1 when cap is too small. */
static int frame(uint8_t *buf, size_t cap, uint8_t header_byte,
                 uint32_t body_len, uint8_t **body_out)
{
    size_t rl = eos_mqtt_remaining_length_size(body_len);
    if (rl == 0) return -1;
    if (cap < 1 + rl + (size_t)body_len) return -1;
    buf[0] = header_byte;
    if (eos_mqtt_encode_remaining_length(buf + 1, body_len) < 0) return -1;
    *body_out = buf + 1 + rl;
    return (int)(1 + rl + (size_t)body_len);
}

static bool topic_name_valid(const char *topic)
{
    if (topic == NULL || topic[0] == '\0') return false;
    /* PUBLISH topic names carry no wildcards (MQTT-4.7.1-1). */
    return strchr(topic, '+') == NULL && strchr(topic, '#') == NULL;
}

/* ================================================================
 * Encoders
 * ================================================================ */

int eos_mqtt_encode_connect(uint8_t *buf, size_t cap,
                            const char *client_id,
                            const char *username, const char *password,
                            uint16_t keepalive_sec, bool clean_session)
{
    if (buf == NULL || client_id == NULL) return -1;
    if (password != NULL && username == NULL) return -1;
    if (client_id[0] == '\0' && !clean_session) return -1;

    size_t client_field = string_field_size(client_id);
    size_t user_field = (username != NULL) ? string_field_size(username) : 0;
    size_t pass_field = (password != NULL) ? string_field_size(password) : 0;
    if (client_field == 0) return -1;
    if (username != NULL && user_field == 0) return -1;
    if (password != NULL && pass_field == 0) return -1;

    /* Variable header: protocol name (6) + level (1) + flags (1)
     * + keep-alive (2) = 10 bytes, then the payload fields. */
    uint32_t body_len = (uint32_t)(10 + client_field + user_field + pass_field);
    uint8_t *p = NULL;
    int total = frame(buf, cap, (uint8_t)(EOS_MQTT_PKT_CONNECT << 4),
                      body_len, &p);
    if (total < 0) return -1;

    p = write_string(p, "MQTT");
    *p++ = 4; /* protocol level: 3.1.1 */
    uint8_t flags = 0;
    if (username != NULL) flags |= 0x80u;
    if (password != NULL) flags |= 0x40u;
    if (clean_session)    flags |= 0x02u;
    *p++ = flags;
    p = write_u16(p, keepalive_sec);
    p = write_string(p, client_id);
    if (username != NULL) p = write_string(p, username);
    if (password != NULL) p = write_string(p, password);
    return total;
}

int eos_mqtt_encode_publish(uint8_t *buf, size_t cap, const char *topic,
                            const uint8_t *payload, size_t payload_len,
                            uint8_t qos, bool retain, bool dup,
                            uint16_t packet_id)
{
    if (buf == NULL || !topic_name_valid(topic)) return -1;
    if (qos > 1) return -1;
    if (qos == 1 && packet_id == 0) return -1;
    if (payload_len > 0 && payload == NULL) return -1;
    if (payload_len > EOS_MQTT_MAX_REMAINING_LENGTH) return -1;

    size_t topic_field = string_field_size(topic);
    if (topic_field == 0) return -1;
    uint32_t body_len = (uint32_t)(topic_field + (qos > 0 ? 2u : 0u) +
                                   (uint32_t)payload_len);
    uint8_t header_byte = (uint8_t)((EOS_MQTT_PKT_PUBLISH << 4) |
                                    (dup ? 0x08u : 0u) |
                                    ((uint8_t)(qos << 1)) |
                                    (retain ? 0x01u : 0u));
    uint8_t *p = NULL;
    int total = frame(buf, cap, header_byte, body_len, &p);
    if (total < 0) return -1;

    p = write_string(p, topic);
    if (qos > 0) p = write_u16(p, packet_id);
    if (payload_len > 0) {
        memcpy(p, payload, payload_len);
        p += payload_len;
    }
    return total;
}

int eos_mqtt_encode_puback(uint8_t *buf, size_t cap, uint16_t packet_id)
{
    if (buf == NULL || packet_id == 0) return -1;
    uint8_t *p = NULL;
    int total = frame(buf, cap, (uint8_t)(EOS_MQTT_PKT_PUBACK << 4), 2, &p);
    if (total < 0) return -1;
    write_u16(p, packet_id);
    return total;
}

int eos_mqtt_encode_subscribe(uint8_t *buf, size_t cap, uint16_t packet_id,
                              const char *topic, uint8_t qos)
{
    if (buf == NULL || packet_id == 0) return -1;
    if (topic == NULL || topic[0] == '\0') return -1;
    if (qos > 1) return -1;
    size_t topic_field = string_field_size(topic);
    if (topic_field == 0) return -1;
    uint32_t body_len = (uint32_t)(2 + topic_field + 1);
    uint8_t *p = NULL;
    int total = frame(buf, cap,
                      (uint8_t)((EOS_MQTT_PKT_SUBSCRIBE << 4) | 0x02u),
                      body_len, &p);
    if (total < 0) return -1;
    p = write_u16(p, packet_id);
    p = write_string(p, topic);
    *p = qos;
    return total;
}

int eos_mqtt_encode_unsubscribe(uint8_t *buf, size_t cap, uint16_t packet_id,
                                const char *topic)
{
    if (buf == NULL || packet_id == 0) return -1;
    if (topic == NULL || topic[0] == '\0') return -1;
    size_t topic_field = string_field_size(topic);
    if (topic_field == 0) return -1;
    uint32_t body_len = (uint32_t)(2 + topic_field);
    uint8_t *p = NULL;
    int total = frame(buf, cap,
                      (uint8_t)((EOS_MQTT_PKT_UNSUBSCRIBE << 4) | 0x02u),
                      body_len, &p);
    if (total < 0) return -1;
    p = write_u16(p, packet_id);
    write_string(p, topic);
    return total;
}

int eos_mqtt_encode_pingreq(uint8_t *buf, size_t cap)
{
    if (buf == NULL) return -1;
    uint8_t *p = NULL;
    return frame(buf, cap, (uint8_t)(EOS_MQTT_PKT_PINGREQ << 4), 0, &p);
}

int eos_mqtt_encode_disconnect(uint8_t *buf, size_t cap)
{
    if (buf == NULL) return -1;
    uint8_t *p = NULL;
    return frame(buf, cap, (uint8_t)(EOS_MQTT_PKT_DISCONNECT << 4), 0, &p);
}

/* ================================================================
 * Decoders
 * ================================================================ */

/* Parse the fixed header and hand back the body span. */
static eos_mqtt_status_t body_span(const uint8_t *buf, size_t len,
                                   uint8_t expected_type,
                                   const uint8_t **body, size_t *body_len,
                                   uint8_t *flags)
{
    eos_mqtt_header_t hdr;
    eos_mqtt_status_t st = eos_mqtt_parse_header(buf, len, &hdr);
    if (st != EOS_MQTT_OK) return st;
    if (hdr.type != expected_type) return EOS_MQTT_MALFORMED;
    if (len != hdr.header_len + hdr.remaining_length)
        return EOS_MQTT_MALFORMED;
    *body = buf + hdr.header_len;
    *body_len = hdr.remaining_length;
    if (flags != NULL) *flags = hdr.flags;
    return EOS_MQTT_OK;
}

eos_mqtt_status_t eos_mqtt_decode_connack(const uint8_t *buf, size_t len,
                                          eos_mqtt_connack_t *out)
{
    const uint8_t *body = NULL;
    size_t body_len = 0;
    eos_mqtt_status_t st = body_span(buf, len, EOS_MQTT_PKT_CONNACK,
                                     &body, &body_len, NULL);
    if (st != EOS_MQTT_OK) return st;
    if (body_len != 2) return EOS_MQTT_MALFORMED;
    /* Acknowledge flags: only bit 0 (session present) may be set. */
    if ((body[0] & 0xFEu) != 0) return EOS_MQTT_MALFORMED;
    if (body[1] > 5) return EOS_MQTT_MALFORMED;
    out->session_present = (body[0] & 0x01u) != 0;
    out->return_code = body[1];
    return EOS_MQTT_OK;
}

eos_mqtt_status_t eos_mqtt_decode_publish(const uint8_t *buf, size_t len,
                                          eos_mqtt_publish_t *out)
{
    const uint8_t *body = NULL;
    size_t body_len = 0;
    uint8_t flags = 0;
    eos_mqtt_status_t st = body_span(buf, len, EOS_MQTT_PKT_PUBLISH,
                                     &body, &body_len, &flags);
    if (st != EOS_MQTT_OK) return st;
    uint8_t qos = (uint8_t)((flags >> 1) & 0x03u);
    if (qos == 3) return EOS_MQTT_MALFORMED; /* reserved (MQTT-3.3.1-4) */
    if (body_len < 2) return EOS_MQTT_MALFORMED;
    uint16_t topic_len = (uint16_t)((body[0] << 8) | body[1]);
    if (topic_len == 0) return EOS_MQTT_MALFORMED;
    if ((size_t)topic_len > body_len - 2) return EOS_MQTT_MALFORMED;
    const char *topic = (const char *)(body + 2);
    for (uint16_t i = 0; i < topic_len; i++) {
        if (topic[i] == '+' || topic[i] == '#') return EOS_MQTT_MALFORMED;
    }
    size_t pos = 2 + (size_t)topic_len;
    uint16_t packet_id = 0;
    if (qos > 0) {
        if (body_len - pos < 2) return EOS_MQTT_MALFORMED;
        packet_id = (uint16_t)((body[pos] << 8) | body[pos + 1]);
        if (packet_id == 0) return EOS_MQTT_MALFORMED;
        pos += 2;
    }
    out->topic = topic;
    out->topic_len = topic_len;
    out->packet_id = packet_id;
    out->payload = body + pos;
    out->payload_len = body_len - pos;
    out->qos = qos;
    out->retain = (flags & 0x01u) != 0;
    out->dup = (flags & 0x08u) != 0;
    return EOS_MQTT_OK;
}

eos_mqtt_status_t eos_mqtt_decode_packet_id(const uint8_t *buf, size_t len,
                                            uint8_t expected_type,
                                            uint16_t *packet_id)
{
    const uint8_t *body = NULL;
    size_t body_len = 0;
    eos_mqtt_status_t st = body_span(buf, len, expected_type,
                                     &body, &body_len, NULL);
    if (st != EOS_MQTT_OK) return st;
    if (body_len != 2) return EOS_MQTT_MALFORMED;
    uint16_t id = (uint16_t)((body[0] << 8) | body[1]);
    if (id == 0) return EOS_MQTT_MALFORMED;
    *packet_id = id;
    return EOS_MQTT_OK;
}

eos_mqtt_status_t eos_mqtt_decode_suback(const uint8_t *buf, size_t len,
                                         eos_mqtt_suback_t *out)
{
    const uint8_t *body = NULL;
    size_t body_len = 0;
    eos_mqtt_status_t st = body_span(buf, len, EOS_MQTT_PKT_SUBACK,
                                     &body, &body_len, NULL);
    if (st != EOS_MQTT_OK) return st;
    /* One subscription per SUBSCRIBE in this client: id + 1 code. */
    if (body_len != 3) return EOS_MQTT_MALFORMED;
    uint16_t id = (uint16_t)((body[0] << 8) | body[1]);
    if (id == 0) return EOS_MQTT_MALFORMED;
    uint8_t code = body[2];
    if (!(code <= 2 || code == 0x80u)) return EOS_MQTT_MALFORMED;
    out->packet_id = id;
    out->return_code = code;
    return EOS_MQTT_OK;
}
