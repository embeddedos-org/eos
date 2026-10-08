/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 EmbeddedOS Project
 *
 * @file test_pkg_anchor_usable.c
 * @brief The trust-anchor decision, without the host-only package manager.
 *
 * #98 removed eos_pkg's hard-coded all-zero trust anchor: the anchor now
 * arrives from the platform (eos_pkg_set_trust_anchor() or the
 * EOS_PKG_TRUST_ANCHOR_HEX build value), a non-prime-order key is refused
 * at set time, and with no anchor configured verification refuses rather
 * than appearing to succeed.
 *
 * #133 observed that the suite proving this (test_pkg_trust_anchor) only
 * builds on the host, because it links the whole host-only eos_eapp
 * library. The decision itself lives in eos_pkg_anchor.c, which needs
 * only eos_crypto and libc -- so this suite links just that TU and is
 * registered unconditionally, next to test_crypto_ed25519_loworder.
 * The full package-signing integration stays host-only, where it belongs:
 * it needs real package files.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eos_pkg.h"
#include <ed25519.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    static void name(void); \
    static void run_##name(void) { \
        printf("  %-56s ", #name); \
        fflush(stdout); \
        tests_run++; \
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

/* A genuinely usable Ed25519 public key: derived from a fixed seed and
 * cross-validated against OpenSSL (see test_pkg_trust_anchor.c). */
static const uint8_t VALID_KEY[32] = {
 0xc9,0x9a,0xff,0x66,0x7c,0x75,0x78,0x05,0x6b,0xd7,0xd4,0x59,0x0d,0xf9,0x2b,0xf2,
 0xae,0x10,0x8a,0x6a,0x7f,0x33,0xdd,0x52,0x38,0x4a,0x83,0xbf,0x32,0x5f,0x02,0x69};

/* A second usable key, derived at test time from a different seed with the
 * same ed25519_create_keypair() the signing tests use -- hard-coding a
 * second vector by hand would risk a key that is not a prime-order point. */
static const uint8_t SEED_2[32] = {
 0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
 0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f};
static uint8_t VALID_KEY_2[32];

/* The all-zero encoding decodes to a low-order curve point (#98). */
static const uint8_t ZERO_KEY[32] = {0};

/* ---- the decision -------------------------------------------------------- */

TEST(anchor_starts_unset)
{
    ASSERT(eos_pkg_trust_anchor() == NULL);
}

TEST(all_zero_anchor_is_refused)
{
    ASSERT(eos_pkg_set_trust_anchor(ZERO_KEY) == -1);
    ASSERT(eos_pkg_trust_anchor() == NULL);
}

TEST(valid_anchor_is_installed)
{
    ASSERT(eos_pkg_set_trust_anchor(VALID_KEY) == 0);
    const uint8_t *anchor = eos_pkg_trust_anchor();
    ASSERT(anchor != NULL);
    ASSERT(memcmp(anchor, VALID_KEY, 32) == 0);
}

TEST(second_set_replaces)
{
    ASSERT(eos_pkg_set_trust_anchor(VALID_KEY_2) == 0);
    const uint8_t *anchor = eos_pkg_trust_anchor();
    ASSERT(anchor != NULL);
    ASSERT(memcmp(anchor, VALID_KEY_2, 32) == 0);
}

TEST(refused_key_does_not_clear_a_good_anchor)
{
    /* A provisioning mistake must not silently disarm verification. */
    ASSERT(eos_pkg_set_trust_anchor(ZERO_KEY) == -1);
    const uint8_t *anchor = eos_pkg_trust_anchor();
    ASSERT(anchor != NULL);
    ASSERT(memcmp(anchor, VALID_KEY_2, 32) == 0);
}

TEST(null_clears_the_anchor)
{
    ASSERT(eos_pkg_set_trust_anchor(NULL) == 0);
    ASSERT(eos_pkg_trust_anchor() == NULL);
}

int main(void)
{
    uint8_t priv[64];

    /* Derive the second key before any test runs. */
    ed25519_create_keypair(VALID_KEY_2, priv, SEED_2);
    memset(priv, 0, sizeof(priv));

    printf("test_pkg_anchor_usable:\n");
    run_anchor_starts_unset();
    run_all_zero_anchor_is_refused();
    run_valid_anchor_is_installed();
    run_second_set_replaces();
    run_refused_key_does_not_clear_a_good_anchor();
    run_null_clears_the_anchor();
    printf("  %d/%d passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
