/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 EoS Project
 *
 * @file test_pkg_trust_anchor_cross.c
 * @brief Cross-target test for eos_pkg's trust-anchor policy.
 *
 * test_pkg_trust_anchor.c exercises the same policy but links eos_eapp,
 * which compiles the rest of eos_pkg.c -- directory listing and process
 * spawning for install/remove/run -- and is host-only for that reason. This
 * file links only eos_pkg_trust_anchor (#133), the pure in-memory subset of
 * that file split out for exactly this: eos_pkg_set_trust_anchor() and
 * eos_pkg_trust_anchor() themselves, not just the crypto predicate they call
 * into, on a target this repository can actually cross-compile.
 *
 * Issue #133: the anchor-validation layer sits above services/crypto's
 * low-order rejection, and cross-compile targets had no suite that reached
 * it. This file provides that coverage; whether any cross-target CI job
 * currently builds and runs it is a separate question -- see the PR body.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "eos_pkg.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    static void name(void); \
    static void run_##name(void) { \
        printf("  %-56s ", #name); \
        fflush(stdout); \
        name(); \
        tests_passed++; \
        tests_run++; \
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
static const uint8_t t2_pub[EAPP_PUBKEY_LEN] = {
  0x3d,0x40,0x17,0xc3,0xe8,0x43,0x89,0x5a,0x92,0xb7,0x0a,0xa7,0x4d,0x1b,0x7e,0xbc,
  0x9c,0x98,0x2c,0xcf,0x2e,0xc4,0x96,0x8c,0xc0,0xcd,0x55,0xf1,0x2a,0xf4,0x66,0x0c};

/* Test 1's key: another real, prime-order key */
static const uint8_t t1_pub[EAPP_PUBKEY_LEN] = {
  0xd7,0x5a,0x98,0x01,0x82,0xb1,0x0a,0xb7,0xd5,0x4b,0xfe,0xd3,0xc9,0x64,0x07,0x3a,
  0x0e,0xe1,0x72,0xf3,0xda,0xa6,0x23,0x25,0xaf,0x02,0x1a,0x68,0xf7,0x07,0x51,0x1a};

/* The eight low-order encodings. None can authenticate anything, so none is a
 * usable anchor. The first is the value eos_pkg used to ship as its trust
 * anchor before #98 fixed it. Mirrors test_pkg_trust_anchor.c's LOW_ORDER
 * table exactly -- see finding 5 of the #137 review on keeping this table in
 * one place. */
static const uint8_t low_order[8][EAPP_PUBKEY_LEN] = {
    {0}, /* y = 0, order 4 */
    {1}, /* the identity, order 1 */
    {0x26,0xe8,0x95,0x8f,0xc2,0xb2,0x27,0xb0,0x45,0xc3,0xf4,0x89,0xf2,0xef,0x98,0xf0, /* order 8 */
     0xd5,0xdf,0xac,0x05,0xd3,0xc6,0x33,0x39,0xb1,0x38,0x02,0x88,0x6d,0x53,0xfc,0x05},
    {0xc7,0x17,0x6a,0x70,0x3d,0x4d,0xd8,0x4f,0xba,0x3c,0x0b,0x76,0x0d,0x10,0x67,0x0f, /* order 8 */
     0x2a,0x20,0x53,0xfa,0x2c,0x39,0xcc,0xc6,0x4e,0xc7,0xfd,0x77,0x92,0xac,0x03,0x7a},
    {0xec,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* p - 1 */
     0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f},
    {0xed,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* p, reduces to y = 0 */
     0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f},
    {0xee,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* p + 1, reduces to identity */
     0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f},
    {0xd9,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* non-canonical, above p */
     0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
};

/* ---- tests ----------------------------------------------------------------
 *
 * Every case calls eos_pkg_set_trust_anchor() / eos_pkg_trust_anchor()
 * directly -- the same two functions test_pkg_trust_anchor.c calls on the
 * host -- not ed25519_public_key_is_usable(). That predicate is exercised on
 * its own in test_crypto_ed25519_loworder.c. */

TEST(test_real_keys_are_accepted_as_anchors)
{
    const uint8_t *held;

    ASSERT(eos_pkg_set_trust_anchor(t2_pub) == 0);
    held = eos_pkg_trust_anchor();
    ASSERT(held != NULL);
    ASSERT(memcmp(held, t2_pub, EAPP_PUBKEY_LEN) == 0);

    ASSERT(eos_pkg_set_trust_anchor(t1_pub) == 0);
    held = eos_pkg_trust_anchor();
    ASSERT(held != NULL);
    ASSERT(memcmp(held, t1_pub, EAPP_PUBKEY_LEN) == 0);

    ASSERT(eos_pkg_set_trust_anchor(NULL) == 0);
}

TEST(test_low_order_keys_are_refused_as_anchors)
{
    /* Refused when configured, not left to surface later as a verification
     * failure that blames the package for a bad anchor. All eight, because a
     * fix aimed only at the all-zero encoding would leave the rest open. */
    size_t i;
    for (i = 0; i < 8; i++) {
        ASSERT(eos_pkg_set_trust_anchor(low_order[i]) != 0);
        ASSERT(eos_pkg_trust_anchor() == NULL);
    }
}

TEST(test_null_clears_the_anchor)
{
    ASSERT(eos_pkg_set_trust_anchor(t2_pub) == 0);
    ASSERT(eos_pkg_trust_anchor() != NULL);

    ASSERT(eos_pkg_set_trust_anchor(NULL) == 0);
    ASSERT(eos_pkg_trust_anchor() == NULL);
}

TEST(test_invalid_encoding_is_refused_as_anchor)
{
    /* 0xAA repeated is not a valid Ed25519 point encoding: point decode
     * solves for x from y via x^2 = u/v (u = y^2-1, v = dy^2+1), and for
     * this y neither +sqrt(u/v) nor -sqrt(u/v)*sqrt(-1) satisfies vx^2 = u,
     * so no x exists on the curve for it and decode fails closed. */
    uint8_t invalid[EAPP_PUBKEY_LEN];
    memset(invalid, 0xAA, sizeof(invalid));

    ASSERT(eos_pkg_set_trust_anchor(invalid) != 0);
    ASSERT(eos_pkg_trust_anchor() == NULL);
}

TEST(test_a_rejected_anchor_does_not_replace_the_one_in_force)
{
    /* A failed provisioning attempt must not disarm a working installation. */
    ASSERT(eos_pkg_set_trust_anchor(t2_pub) == 0);
    ASSERT(eos_pkg_set_trust_anchor(low_order[0]) != 0);

    ASSERT(eos_pkg_trust_anchor() != NULL);
    ASSERT(memcmp(eos_pkg_trust_anchor(), t2_pub, EAPP_PUBKEY_LEN) == 0);

    ASSERT(eos_pkg_set_trust_anchor(NULL) == 0);
}

int main(void)
{
    printf("eos_pkg trust anchor (cross-target)\n");

    run_test_real_keys_are_accepted_as_anchors();
    run_test_low_order_keys_are_refused_as_anchors();
    run_test_null_clears_the_anchor();
    run_test_invalid_encoding_is_refused_as_anchor();
    run_test_a_rejected_anchor_does_not_replace_the_one_in_force();

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
