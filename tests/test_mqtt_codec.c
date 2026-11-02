// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file test_mqtt_codec.c
 * @brief Byte-exact fixture tests for the MQTT 3.1.1 codec (#199 slice 1)
 *
 * The codec in net/src/mqtt_codec.c is pure (buffer in, buffer out),
 * so these tests need no socket, mock or otherwise. The CONNECT and
 * PUBLISH fixtures are seeded byte-for-byte from the packets
 * test_net_mock.c already pins; the rest of the section 5 packet
 * set (docs/eos-mqtt-client-design.md) is pinned here for the
 * first time, together with the malformed cases the design names.
 *
 * No behaviour changes in this slice: the eos_mqtt_* functions in
 * net.c remain stubs until slice 2, and no existing assertion is
 * touched.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "mqtt_codec.h"

static int pass = 0;
static int fail_count = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fail_count++; printf("    [FAIL] %s (line %d)\n", msg, __LINE__); } \
    else { pass++; } \
} while (0)

#define SECTION(name) printf("\n-- %s --\n", name)

/* ----------------------------------------------------------------
 * Remaining Length: boundaries, consumption, malformed encodings
 * ---------------------------------------------------------------- */
static void test_remaining_length(void)
{
    SECTION("Remaining Length boundaries");

    static const struct { uint32_t value; uint8_t bytes[4]; size_t used; } cases[] = {
        {0u,         {0x00, 0, 0, 0},             1},
        {127u,       {0x7F, 0, 0, 0},             1},
        {128u,       {0x80, 0x01, 0, 0},          2},
        {16383u,     {0xFF, 0x7F, 0, 0},          2},
        {16384u,     {0x80, 0x80, 0x01, 0},       3},
        {2097151u,   {0xFF, 0xFF, 0x7F, 0},       3},
        {2097152u,   {0x80, 0x80, 0x80, 0x01},    4},
        {268435455u, {0xFF, 0xFF, 0xFF, 0x7F},    4},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        uint8_t out[4] = {0, 0, 0, 0};
        int n = eos_mqtt_encode_remaining_length(out, cases[i].value);
        CHECK(n == (int)cases[i].used, "RL encode byte count");
        CHECK(memcmp(out, cases[i].bytes, cases[i].used) == 0, "RL encode bytes");
        CHECK(eos_mqtt_remaining_length_size(cases[i].value) == cases[i].used,
              "RL size agrees");
        uint32_t value = 0;
        size_t used = 0;
        CHECK(eos_mqtt_decode_remaining_length(cases[i].bytes, cases[i].used,
                                               &value, &used) == EOS_MQTT_OK,
              "RL decode ok");
        CHECK(value == cases[i].value && used == cases[i].used,
              "RL decode round-trips with consumed count");
    }

    uint8_t out[4];
    CHECK(eos_mqtt_encode_remaining_length(out, 268435456u) == -1,
          "RL above the maximum is rejected");
    CHECK(eos_mqtt_remaining_length_size(268435456u) == 0,
          "RL size 0 above the maximum");

    uint32_t value = 0;
    size_t used = 0;
    static const uint8_t five_deep[] = {0x80, 0x80, 0x80, 0x80, 0x01};
    CHECK(eos_mqtt_decode_remaining_length(five_deep, sizeof(five_deep),
                                           &value, &used) == EOS_MQTT_MALFORMED,
          "RL continuation on the 4th byte is malformed");
    static const uint8_t nonminimal_zero[] = {0x80, 0x00};
    CHECK(eos_mqtt_decode_remaining_length(nonminimal_zero,
                                           sizeof(nonminimal_zero),
                                           &value, &used) == EOS_MQTT_MALFORMED,
          "RL non-minimal encoding of 0 is malformed");
    static const uint8_t nonminimal_128[] = {0x80, 0x81, 0x00};
    CHECK(eos_mqtt_decode_remaining_length(nonminimal_128,
                                           sizeof(nonminimal_128),
                                           &value, &used) == EOS_MQTT_MALFORMED,
          "RL non-minimal encoding of 128 is malformed");
    static const uint8_t cut[] = {0x80};
    CHECK(eos_mqtt_decode_remaining_length(cut, sizeof(cut),
                                           &value, &used) == EOS_MQTT_INCOMPLETE,
          "RL cut mid-encoding is incomplete");
}

/* ----------------------------------------------------------------
 * CONNECT: the mock-pinned fixture, credentials, rejections
 * ---------------------------------------------------------------- */
static void test_connect(void)
{
    SECTION("CONNECT fixtures");

    /* Byte-exact against the packet test_net_mock.c pins. */
    static const uint8_t expect[] = {
        0x10, 0x14,
        0x00, 0x04, 'M', 'Q', 'T', 'T',
        0x04, 0x02, 0x00, 0x3C,
        0x00, 0x08, 'e', 'o', 's', '_', 't', 'e', 's', 't'
    };
    uint8_t buf[256];
    int n = eos_mqtt_encode_connect(buf, sizeof(buf), "eos_test",
                                    NULL, NULL, 60, true);
    CHECK(n == (int)sizeof(expect), "CONNECT length matches the mock fixture");
    CHECK(memcmp(buf, expect, sizeof(expect)) == 0,
          "CONNECT bytes match the mock fixture");

    eos_mqtt_header_t hdr;
    CHECK(eos_mqtt_parse_header(buf, (size_t)n, &hdr) == EOS_MQTT_OK,
          "CONNECT parses back");
    CHECK(hdr.type == EOS_MQTT_PKT_CONNECT && hdr.remaining_length == 20,
          "CONNECT header fields");

    /* Credentials: flags carry user (bit 7) + password (bit 6) +
     * clean session (bit 1); payload order is client, user, pass. */
    n = eos_mqtt_encode_connect(buf, sizeof(buf), "dev1",
                                "aswin", "s3cret", 30, true);
    CHECK(n > 0, "CONNECT with credentials encodes");
    CHECK(buf[9] == 0xC2, "CONNECT flags: user + password + clean session");
    const char *expect_tail = "\x00\x04" "dev1"
                              "\x00\x05" "aswin"
                              "\x00\x06" "s3cret";
    CHECK(memcmp(buf + n - 21, expect_tail, 21) == 0,
          "CONNECT payload order is client, username, password");

    CHECK(eos_mqtt_encode_connect(buf, sizeof(buf), "dev1",
                                  NULL, "s3cret", 30, true) == -1,
          "password without username is rejected");
    CHECK(eos_mqtt_encode_connect(buf, sizeof(buf), "",
                                  NULL, NULL, 30, false) == -1,
          "empty client id without clean session is rejected");
    CHECK(eos_mqtt_encode_connect(buf, sizeof(buf), "",
                                  NULL, NULL, 30, true) > 0,
          "empty client id with clean session encodes");
    CHECK(eos_mqtt_encode_connect(buf, sizeof(buf), NULL,
                                  NULL, NULL, 30, true) == -1,
          "NULL client id is rejected");
    CHECK(eos_mqtt_encode_connect(buf, 8, "eos_test",
                                  NULL, NULL, 60, true) == -1,
          "CONNECT into a too-small buffer is rejected");
}

/* ----------------------------------------------------------------
 * CONNACK: accepted, refusal, session present, malformed
 * ---------------------------------------------------------------- */
static void test_connack(void)
{
    SECTION("CONNACK decode");

    eos_mqtt_connack_t ack;
    static const uint8_t accepted[] = {0x20, 0x02, 0x00, 0x00};
    CHECK(eos_mqtt_decode_connack(accepted, sizeof(accepted), &ack)
              == EOS_MQTT_OK,
          "CONNACK accepted decodes");
    CHECK(!ack.session_present && ack.return_code == 0,
          "CONNACK accepted fields");

    static const uint8_t session[] = {0x20, 0x02, 0x01, 0x00};
    CHECK(eos_mqtt_decode_connack(session, sizeof(session), &ack)
              == EOS_MQTT_OK && ack.session_present,
          "CONNACK session-present decodes");

    static const uint8_t refused[] = {0x20, 0x02, 0x00, 0x05};
    CHECK(eos_mqtt_decode_connack(refused, sizeof(refused), &ack)
              == EOS_MQTT_OK && ack.return_code == 5,
          "CONNACK refusal code 5 surfaces for the connect call to fail");

    static const uint8_t bad_rc[] = {0x20, 0x02, 0x00, 0x06};
    CHECK(eos_mqtt_decode_connack(bad_rc, sizeof(bad_rc), &ack)
              == EOS_MQTT_MALFORMED,
          "CONNACK return code 6 is malformed");
    static const uint8_t bad_flags[] = {0x20, 0x02, 0x02, 0x00};
    CHECK(eos_mqtt_decode_connack(bad_flags, sizeof(bad_flags), &ack)
              == EOS_MQTT_MALFORMED,
          "CONNACK acknowledge flags with reserved bits is malformed");
}

/* ----------------------------------------------------------------
 * PUBLISH: mock fixture, QoS 1 round-trip, RL boundary, rejects
 * ---------------------------------------------------------------- */
static void test_publish(void)
{
    SECTION("PUBLISH fixtures");

    static const uint8_t expect[] = {
        0x30, 0x0E, 0x00, 0x08,
        'e', 'o', 's', '/', 't', 'e', 'm', 'p', '2', '3', '.', '5'
    };
    uint8_t buf[512];
    int n = eos_mqtt_encode_publish(buf, sizeof(buf), "eos/temp",
                                    (const uint8_t *)"23.5", 4,
                                    0, false, false, 0);
    CHECK(n == (int)sizeof(expect), "PUBLISH QoS 0 length matches the mock fixture");
    CHECK(memcmp(buf, expect, sizeof(expect)) == 0,
          "PUBLISH QoS 0 bytes match the mock fixture");

    /* QoS 1 with DUP + RETAIN: decode must surface every field. */
    n = eos_mqtt_encode_publish(buf, sizeof(buf), "eos/temp",
                                (const uint8_t *)"23.5", 4,
                                1, true, true, 0x1234);
    CHECK(n > 0 && buf[0] == 0x3B, "PUBLISH QoS 1 header: dup + qos1 + retain");
    eos_mqtt_publish_t pub;
    CHECK(eos_mqtt_decode_publish(buf, (size_t)n, &pub) == EOS_MQTT_OK,
          "PUBLISH QoS 1 decodes");
    CHECK(pub.topic_len == 8 && memcmp(pub.topic, "eos/temp", 8) == 0,
          "PUBLISH decoded topic");
    CHECK(pub.packet_id == 0x1234 && pub.qos == 1 && pub.dup && pub.retain,
          "PUBLISH decoded id and flags");
    CHECK(pub.payload_len == 4 && memcmp(pub.payload, "23.5", 4) == 0,
          "PUBLISH decoded payload");

    /* A 200-byte payload pushes Remaining Length past 127, so the
     * fixed header grows a byte; the round-trip must still hold. */
    uint8_t big[200];
    memset(big, 'x', sizeof(big));
    n = eos_mqtt_encode_publish(buf, sizeof(buf), "t", big, sizeof(big),
                                0, false, false, 0);
    CHECK(n == 1 + 2 + 2 + 1 + 200, "PUBLISH large payload total length");
    CHECK(buf[1] == 0xCB && buf[2] == 0x01,
          "PUBLISH large payload uses a 2-byte Remaining Length");
    CHECK(eos_mqtt_decode_publish(buf, (size_t)n, &pub) == EOS_MQTT_OK &&
              pub.payload_len == 200,
          "PUBLISH large payload round-trips");

    CHECK(eos_mqtt_encode_publish(buf, sizeof(buf), "eos/temp",
                                  (const uint8_t *)"x", 1,
                                  2, false, false, 1) == -1,
          "PUBLISH QoS 2 encode is out of scope and rejected");
    CHECK(eos_mqtt_encode_publish(buf, sizeof(buf), "eos/temp",
                                  (const uint8_t *)"x", 1,
                                  3, false, false, 1) == -1,
          "PUBLISH QoS 3 encode is rejected");
    CHECK(eos_mqtt_encode_publish(buf, sizeof(buf), "eos/temp",
                                  (const uint8_t *)"x", 1,
                                  1, false, false, 0) == -1,
          "PUBLISH QoS 1 with packet id 0 is rejected");
    CHECK(eos_mqtt_encode_publish(buf, sizeof(buf), "eos/+",
                                  (const uint8_t *)"x", 1,
                                  0, false, false, 0) == -1,
          "PUBLISH to a wildcard topic is rejected");
    CHECK(eos_mqtt_encode_publish(buf, sizeof(buf), "",
                                  (const uint8_t *)"x", 1,
                                  0, false, false, 0) == -1,
          "PUBLISH to an empty topic is rejected");
    CHECK(eos_mqtt_encode_publish(buf, 8, "eos/temp",
                                  (const uint8_t *)"23.5", 4,
                                  0, false, false, 0) == -1,
          "PUBLISH into a too-small buffer is rejected");
}

/* ----------------------------------------------------------------
 * PUBLISH decode: malformed set + the QoS 2 surface
 * ---------------------------------------------------------------- */
static void test_publish_malformed(void)
{
    SECTION("PUBLISH decode malformed cases");

    eos_mqtt_publish_t pub;

    /* Topic length field overruns the packet. */
    static const uint8_t overrun[] = {0x30, 0x04, 0x00, 0x08, 'e', 'o'};
    CHECK(eos_mqtt_decode_publish(overrun, sizeof(overrun), &pub)
              == EOS_MQTT_MALFORMED,
          "PUBLISH with an overrun topic length is malformed");

    /* QoS bits 3 are reserved. */
    static const uint8_t qos3[] = {0x36, 0x03, 0x00, 0x01, 't'};
    CHECK(eos_mqtt_decode_publish(qos3, sizeof(qos3), &pub)
              == EOS_MQTT_MALFORMED,
          "PUBLISH with reserved QoS 3 flags is malformed");

    /* A wildcard in an inbound topic name is a protocol error. */
    static const uint8_t wild[] = {0x30, 0x05, 0x00, 0x03, 'a', '/', '+'};
    CHECK(eos_mqtt_decode_publish(wild, sizeof(wild), &pub)
              == EOS_MQTT_MALFORMED,
          "PUBLISH with a wildcard topic is malformed");

    /* An inbound QoS 2 PUBLISH decodes with qos == 2 surfaced, so
     * eos_mqtt_loop (slice 3) can fail the connection on it
     * deliberately, as the design requires. */
    static const uint8_t qos2[] = {
        0x34, 0x09, 0x00, 0x03, 'a', '/', 'b', 0x00, 0x07, 'x', 'y'
    };
    CHECK(eos_mqtt_decode_publish(qos2, sizeof(qos2), &pub) == EOS_MQTT_OK,
          "Inbound QoS 2 PUBLISH decodes");
    CHECK(pub.qos == 2 && pub.packet_id == 7 && pub.payload_len == 2,
          "Inbound QoS 2 surfaces qos and packet id to the caller");
}

/* ----------------------------------------------------------------
 * SUBSCRIBE / SUBACK / UNSUBSCRIBE / UNSUBACK / PUBACK / PING
 * ---------------------------------------------------------------- */
static void test_subscribe_family(void)
{
    SECTION("SUBSCRIBE family fixtures");

    static const uint8_t expect_sub[] = {
        0x82, 0x0D, 0x00, 0x07, 0x00, 0x08,
        'e', 'o', 's', '/', 't', 'e', 'm', 'p', 0x01
    };
    uint8_t buf[128];
    int n = eos_mqtt_encode_subscribe(buf, sizeof(buf), 7, "eos/temp", 1);
    CHECK(n == (int)sizeof(expect_sub), "SUBSCRIBE length");
    CHECK(memcmp(buf, expect_sub, sizeof(expect_sub)) == 0,
          "SUBSCRIBE bytes (flags nibble 0x2, id, filter, QoS)");

    eos_mqtt_suback_t suback;
    static const uint8_t granted[] = {0x90, 0x03, 0x00, 0x07, 0x01};
    CHECK(eos_mqtt_decode_suback(granted, sizeof(granted), &suback)
              == EOS_MQTT_OK && suback.packet_id == 7 &&
              suback.return_code == 1,
          "SUBACK granted QoS 1 decodes");
    static const uint8_t failed[] = {0x90, 0x03, 0x00, 0x07, 0x80};
    CHECK(eos_mqtt_decode_suback(failed, sizeof(failed), &suback)
              == EOS_MQTT_OK && suback.return_code == 0x80,
          "SUBACK failure code 0x80 decodes");
    static const uint8_t bad_code[] = {0x90, 0x03, 0x00, 0x07, 0x03};
    CHECK(eos_mqtt_decode_suback(bad_code, sizeof(bad_code), &suback)
              == EOS_MQTT_MALFORMED,
          "SUBACK return code 0x03 is malformed");
    static const uint8_t zero_id[] = {0x90, 0x03, 0x00, 0x00, 0x01};
    CHECK(eos_mqtt_decode_suback(zero_id, sizeof(zero_id), &suback)
              == EOS_MQTT_MALFORMED,
          "SUBACK packet id 0 is malformed");

    static const uint8_t expect_unsub[] = {
        0xA2, 0x0C, 0x00, 0x07, 0x00, 0x08,
        'e', 'o', 's', '/', 't', 'e', 'm', 'p'
    };
    n = eos_mqtt_encode_unsubscribe(buf, sizeof(buf), 7, "eos/temp");
    CHECK(n == (int)sizeof(expect_unsub), "UNSUBSCRIBE length");
    CHECK(memcmp(buf, expect_unsub, sizeof(expect_unsub)) == 0,
          "UNSUBSCRIBE bytes (flags nibble 0x2, id, filter)");

    uint16_t packet_id = 0;
    static const uint8_t unsuback[] = {0xB0, 0x02, 0x00, 0x07};
    CHECK(eos_mqtt_decode_packet_id(unsuback, sizeof(unsuback),
                                    EOS_MQTT_PKT_UNSUBACK, &packet_id)
              == EOS_MQTT_OK && packet_id == 7,
          "UNSUBACK packet id decodes");

    n = eos_mqtt_encode_puback(buf, sizeof(buf), 0x1234);
    static const uint8_t expect_puback[] = {0x40, 0x02, 0x12, 0x34};
    CHECK(n == (int)sizeof(expect_puback) &&
              memcmp(buf, expect_puback, sizeof(expect_puback)) == 0,
          "PUBACK bytes");
    packet_id = 0;
    CHECK(eos_mqtt_decode_packet_id(buf, (size_t)n, EOS_MQTT_PKT_PUBACK,
                                    &packet_id) == EOS_MQTT_OK &&
              packet_id == 0x1234,
          "PUBACK round-trips");
    CHECK(eos_mqtt_decode_packet_id(buf, (size_t)n, EOS_MQTT_PKT_UNSUBACK,
                                    &packet_id) == EOS_MQTT_MALFORMED,
          "A PUBACK-shaped packet fails an UNSUBACK expectation");
    CHECK(eos_mqtt_encode_puback(buf, sizeof(buf), 0) == -1,
          "PUBACK with packet id 0 is rejected");

    CHECK(eos_mqtt_encode_subscribe(buf, sizeof(buf), 0, "eos/temp", 1)
              == -1,
          "SUBSCRIBE with packet id 0 is rejected");
    CHECK(eos_mqtt_encode_subscribe(buf, sizeof(buf), 7, "eos/temp", 2)
              == -1,
          "SUBSCRIBE with QoS 2 is rejected");
    CHECK(eos_mqtt_encode_subscribe(buf, 6, 7, "eos/temp", 1) == -1,
          "SUBSCRIBE into a too-small buffer is rejected");
    CHECK(eos_mqtt_encode_unsubscribe(buf, 6, 7, "eos/temp") == -1,
          "UNSUBSCRIBE into a too-small buffer is rejected");
}

static void test_ping_and_disconnect(void)
{
    SECTION("PINGREQ / DISCONNECT / PINGRESP");

    uint8_t buf[16];
    int n = eos_mqtt_encode_pingreq(buf, sizeof(buf));
    CHECK(n == 2 && buf[0] == 0xC0 && buf[1] == 0x00, "PINGREQ bytes");
    n = eos_mqtt_encode_disconnect(buf, sizeof(buf));
    CHECK(n == 2 && buf[0] == 0xE0 && buf[1] == 0x00, "DISCONNECT bytes");
    CHECK(eos_mqtt_encode_pingreq(buf, 1) == -1,
          "PINGREQ into a 1-byte buffer is rejected");

    static const uint8_t pingresp[] = {0xD0, 0x00};
    eos_mqtt_header_t hdr;
    CHECK(eos_mqtt_parse_header(pingresp, sizeof(pingresp), &hdr)
              == EOS_MQTT_OK && hdr.type == EOS_MQTT_PKT_PINGRESP &&
              hdr.remaining_length == 0,
          "PINGRESP parses as a header-only packet");

    /* A packet cut short is incomplete, never malformed. */
    static const uint8_t cut[] = {0x30, 0x0E, 0x00, 0x08, 'e', 'o'};
    CHECK(eos_mqtt_parse_header(cut, sizeof(cut), &hdr)
              == EOS_MQTT_INCOMPLETE,
          "A truncated packet is incomplete");
}

int main(void)
{
    printf("================================================\n");
    printf("  EoS MQTT Codec Tests -- byte-exact fixtures\n");
    printf("================================================\n");

    test_remaining_length();
    test_connect();
    test_connack();
    test_publish();
    test_publish_malformed();
    test_subscribe_family();
    test_ping_and_disconnect();

    printf("\n================================================\n");
    if (fail_count == 0)
        printf("  ALL %d MQTT CODEC CHECKS PASSED\n", pass);
    else
        printf("  %d PASSED, %d FAILED\n", pass, fail_count);
    printf("================================================\n");

    return fail_count > 0 ? 1 : 0;
}
