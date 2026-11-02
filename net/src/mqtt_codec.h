// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file mqtt_codec.h
 * @brief Pure MQTT 3.1.1 packet codec for the eos_mqtt_* client (#199).
 *
 * Encode/decode only: buffer in, buffer out, no I/O, no allocation,
 * no dependency beyond <stdint.h>/<stddef.h>/<stdbool.h>. The handle
 * state machine that drives this codec over the eos_net socket API
 * lives with the eos_mqtt_* implementation in net.c (slice 2/3 of
 * the plan in docs/eos-mqtt-client-design.md); this header is the
 * codec's whole contract, so the codec can be lifted on its own.
 *
 * Scope (design doc section 5): MQTT 3.1.1, QoS 0/1. QoS 2 packet
 * types (PUBREC/PUBREL/PUBCOMP) are not encoded; an inbound QoS 2
 * PUBLISH decodes with qos == 2 surfaced so the caller can fail the
 * connection deliberately, as the design requires.
 */

#ifndef EOS_MQTT_CODEC_H
#define EOS_MQTT_CODEC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* MQTT 3.1.1 control packet types (fixed-header high nibble). */
enum {
    EOS_MQTT_PKT_CONNECT     = 1,
    EOS_MQTT_PKT_CONNACK     = 2,
    EOS_MQTT_PKT_PUBLISH     = 3,
    EOS_MQTT_PKT_PUBACK      = 4,
    EOS_MQTT_PKT_SUBSCRIBE   = 8,
    EOS_MQTT_PKT_SUBACK      = 9,
    EOS_MQTT_PKT_UNSUBSCRIBE = 10,
    EOS_MQTT_PKT_UNSUBACK    = 11,
    EOS_MQTT_PKT_PINGREQ     = 12,
    EOS_MQTT_PKT_PINGRESP    = 13,
    EOS_MQTT_PKT_DISCONNECT  = 14
};

/* Largest Remaining Length value (MQTT 3.1.1 section 2.2.3). */
#define EOS_MQTT_MAX_REMAINING_LENGTH 268435455u

/* Decode outcome. Encoders return a byte count or -1 instead. */
typedef enum {
    EOS_MQTT_OK         = 0,  /* a complete, well-formed packet */
    EOS_MQTT_INCOMPLETE = 1,  /* buffer ends before the packet does */
    EOS_MQTT_MALFORMED  = -1  /* bytes violate the 3.1.1 framing */
} eos_mqtt_status_t;

/* A parsed fixed header: the body follows at buf + header_len and
 * runs for remaining_length bytes. parse_header only returns
 * EOS_MQTT_OK when the whole packet is present in the buffer. */
typedef struct {
    uint8_t  type;              /* EOS_MQTT_PKT_* */
    uint8_t  flags;             /* fixed-header low nibble */
    uint32_t remaining_length;
    size_t   header_len;        /* fixed header size in bytes */
} eos_mqtt_header_t;

/* ---- Remaining Length (variable byte integer) ---- */

/* Bytes the encoding of len occupies (1-4), or 0 if len is too big. */
size_t eos_mqtt_remaining_length_size(uint32_t len);

/* Write the encoding of len into out[4]; bytes written, or -1. */
int eos_mqtt_encode_remaining_length(uint8_t out[4], uint32_t len);

/* Read an encoding from buf; EOS_MQTT_OK and *used set on success. */
eos_mqtt_status_t eos_mqtt_decode_remaining_length(const uint8_t *buf,
                                                   size_t len,
                                                   uint32_t *value,
                                                   size_t *used);

/* ---- Fixed header ---- */

eos_mqtt_status_t eos_mqtt_parse_header(const uint8_t *buf, size_t len,
                                        eos_mqtt_header_t *hdr);

/* ---- Encoders ----
 * Each returns the total packet length written to buf, or -1 when
 * an argument is invalid or cap is too small. Nothing is written
 * past cap; on -1 the buffer contents are unspecified. */

/* CONNECT. username may be NULL (flag clear); password requires a
 * username (MQTT-3.1.2-22). A zero-length client_id is accepted
 * only with clean_session (MQTT-3.1.3-7). Will fields are not part
 * of the declared eos_mqtt_config_t surface and are not encoded. */
int eos_mqtt_encode_connect(uint8_t *buf, size_t cap,
                            const char *client_id,
                            const char *username, const char *password,
                            uint16_t keepalive_sec, bool clean_session);

/* PUBLISH, QoS 0/1 (QoS 2 is out of scope and rejected). packet_id
 * is required (non-zero) for QoS 1 and ignored for QoS 0. The topic
 * must be a valid topic name: non-empty, no wildcards. */
int eos_mqtt_encode_publish(uint8_t *buf, size_t cap, const char *topic,
                            const uint8_t *payload, size_t payload_len,
                            uint8_t qos, bool retain, bool dup,
                            uint16_t packet_id);

/* PUBACK for a QoS 1 delivery. packet_id must be non-zero. */
int eos_mqtt_encode_puback(uint8_t *buf, size_t cap, uint16_t packet_id);

/* SUBSCRIBE for a single topic filter at the requested QoS (0/1). */
int eos_mqtt_encode_subscribe(uint8_t *buf, size_t cap, uint16_t packet_id,
                              const char *topic, uint8_t qos);

/* UNSUBSCRIBE for a single topic filter. */
int eos_mqtt_encode_unsubscribe(uint8_t *buf, size_t cap, uint16_t packet_id,
                                const char *topic);

/* PINGREQ and DISCONNECT take no variable header or payload. */
int eos_mqtt_encode_pingreq(uint8_t *buf, size_t cap);
int eos_mqtt_encode_disconnect(uint8_t *buf, size_t cap);

/* ---- Decoders ----
 * Each takes one complete packet (as validated by parse_header or
 * an equivalent framing check) and returns EOS_MQTT_OK or
 * EOS_MQTT_MALFORMED. Pointers in out structs reference buf. */

typedef struct {
    bool    session_present;
    uint8_t return_code;        /* 0 = accepted, 1-5 = refusal */
} eos_mqtt_connack_t;

eos_mqtt_status_t eos_mqtt_decode_connack(const uint8_t *buf, size_t len,
                                          eos_mqtt_connack_t *out);

typedef struct {
    const char    *topic;       /* not NUL-terminated; use topic_len */
    uint16_t       topic_len;
    uint16_t       packet_id;   /* 0 when qos == 0 */
    const uint8_t *payload;
    size_t         payload_len;
    uint8_t        qos;         /* 0, 1, or 2 (2: caller must reject) */
    bool           retain;
    bool           dup;
} eos_mqtt_publish_t;

eos_mqtt_status_t eos_mqtt_decode_publish(const uint8_t *buf, size_t len,
                                          eos_mqtt_publish_t *out);

/* Shared shape of PUBACK and UNSUBACK: type + packet identifier. */
eos_mqtt_status_t eos_mqtt_decode_packet_id(const uint8_t *buf, size_t len,
                                            uint8_t expected_type,
                                            uint16_t *packet_id);

typedef struct {
    uint16_t packet_id;
    uint8_t  return_code;       /* granted QoS 0/1, or 0x80 = failure */
} eos_mqtt_suback_t;

eos_mqtt_status_t eos_mqtt_decode_suback(const uint8_t *buf, size_t len,
                                         eos_mqtt_suback_t *out);

#ifdef __cplusplus
}
#endif

#endif /* EOS_MQTT_CODEC_H */
