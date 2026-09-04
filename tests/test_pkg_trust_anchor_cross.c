/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 EoS Project
 *
 * @file test_pkg_trust_anchor_cross.c
 * @brief Cross-target test for eos_pkg trust anchor validation.
 *
 * This test covers the same logic as test_pkg_trust_anchor.c but is designed
 * to run on cross-compile targets (ARM Cortex-M4, ARM64, etc.) by not linking
 * against eos_eapp, which is host-only. Instead, it tests the anchor validation
 * logic directly through the crypto API that underlies it.
 *
 * Issue #133: test_pkg_trust_anchor.c is host-only due to its eos_eapp
 * dependency, so the anchor validation logic is untested on the cross-target
 * configurations that actually ship. This file provides that coverage.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <ed25519.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    static void name(void); \
    static void run_##name(void) { \
        printf("  %-56s ", #name); \
        fflush(stdout); \
        name(); \
        tests_passed++; \
        printf("[PASS]\n"); \
    } \
    static void name(void)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while (0)

/* ---- RFC 8032 section 7.1 test vectors ----------------------------------- */

/* Test 2: a real, prime-order Ed25519 public key */
static uint8_t t2_pub[32] = {
  0x3d,0x40,0x17,0xc3,0xe8,0x43,0x89,0x5a,0x92,0xb7,0x0a,0xa7,0x4d,0x1b,0x7e,0xbc,
  0x9c,0x98,0x2c,0xcf,0x2e,0xc4,0x96,0x8c,0xc0,0xcd,0x55,0xf1,0x2a,0xf4,0x66,0x0c};

/* Test 1's key: another real, prime-order key */
static uint8_t t1_pub[32] = {
  0xd7,0x5a,0x98,0x01,0x82,0xb1,0x0a,0xb7,0xd5,0x4b,0xfe,0xd3,0xc9,0x64,0x07,0x3a,
  0x0e,0xe1,0x72,0xf3,0xda,0xa6,0x23,0x25,0xaf,0x02,0x1a,0x68,0xf7,0x07,0x51,0x1a};

/* The eight low-order encodings. None can authenticate anything, so none is a
 * usable anchor. The first is the value eos_pkg used to ship as its trust
 * anchor before #98 fixed it. */
static uint8_t low_order_0[32] = {0}; /* y = 0, order 4 */
static uint8_t low_order_1[32] = {1}; /* the identity, order 1 */
static uint8_t low_order_2[32] = { /* order 8 */
    0x26,0xe8,0x95,0x8f,0xc2,0xb2,0x27,0xb0,0x45,0xc3,0xf4,0x89,0xf2,0xef,0x98,0xf0,
    0xd5,0xdf,0xac,0x05,0xd3,0xc6,0x33,0x39,0xb1,0x38,0x02,0x88,0x6d,0x53,0xfc,0x05};
static uint8_t low_order_3[32] = { /* order 8 */
    0xc7,0x17,0x6a,0x70,0x3d,0x4d,0xd8,0x4f,0xba,0x3c,0x0b,0x76,0x0d,0x10,0x67,0x0f,
    0x2a,0x20,0x53,0xfa,0x2c,0x39,0xcc,0xc6,0x4e,0xc7,0xfd,0x77,0x92,0xac,0x03,0x7a};
static uint8_t low_order_4[32] = { /* p - 1 */
    0xec,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f};
static uint8_t low_order_5[32] = { /* p, which reduces to y = 0 */
    0xed,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f};
static uint8_t low_order_6[32] = { /* p + 1, which reduces to the identity */
    0xee,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f};
static uint8_t low_order_7[32] = { /* non-canonical, above p */
    0xd9,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};

static uint8_t* low_order_keys[] = {
    low_order_0, low_order_1, low_order_2, low_order_3,
    low_order_4, low_order_5, low_order_6, low_order_7
};

/* ---- tests --------------------------------------------------------------- */

TEST(test_real_keys_are_usable_as_anchors)
{
    /* Both RFC 8032 test vectors are prime-order keys and should be accepted. */
    ASSERT(ed25519_public_key_is_usable(t2_pub) == 1);
    ASSERT(ed25519_public_key_is_usable(t1_pub) == 1);
}

TEST(test_low_order_keys_are_not_usable_as_anchors)
{
    /* All eight low-order encodings must be rejected. The all-zero key is the
     * one eos_pkg shipped with before #98, but a fix aimed only at that value
     * would leave the other seven open. */
    size_t i;
    for (i = 0; i < 8; i++) {
        ASSERT(ed25519_public_key_is_usable(low_order_keys[i]) == 0);
    }
}

TEST(test_null_pointer_is_not_usable)
{
    /* The API must handle NULL gracefully. */
    ASSERT(ed25519_public_key_is_usable(NULL) == 0);
}

TEST(test_invalid_encoding_is_not_usable)
{
    /* A key that does not decode to a valid curve point must be rejected.
     * Use a value that is definitely not a valid Ed25519 point encoding. */
    uint8_t invalid[32];
    memset(invalid, 0xAA, sizeof(invalid));
    ASSERT(ed25519_public_key_is_usable(invalid) == 0);
}

int main(void)
{
    printf("eos_pkg trust anchor validation (cross-target)\n");

    run_test_real_keys_are_usable_as_anchors();
    run_test_low_order_keys_are_not_usable_as_anchors();
    run_test_null_pointer_is_not_usable();
    run_test_invalid_encoding_is_not_usable();

    tests_run = 4;
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
