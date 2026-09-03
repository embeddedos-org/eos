/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 EoS Project
 *
 * @file test_crypto_ed25519_loworder.c
 * @brief Ed25519 must reject public keys outside the prime-order subgroup.
 *
 * Decoding a point is not enough. Ed25519 has eight low-order points, and for
 * any of them every term of the verification equation collapses to the identity
 * regardless of the message — so a signature of all zeros verifies against any
 * content at all. That is not a weakened signature; it is no signature.
 *
 * This was not hypothetical here. eos_pkg.c shipped
 *
 *     static const uint8_t eos_pkg_public_key[32] = {0};
 *
 * and the all-zero encoding is one of the eight, so package installation
 * accepted every file handed to it. See #98.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ed25519_verify(const unsigned char *signature, const unsigned char *message,
                   size_t message_len, const unsigned char *public_key);

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    static void name(void); \
    static void run_##name(void) { \
        printf("  %-52s ", #name); \
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
} while(0)

/* The eight low-order point encodings. */
static const unsigned char LOW_ORDER[8][32] = {
    /* y = 0, order 4 — the encoding eos_pkg used as its trust anchor */
    {0},
    /* the identity, order 1 */
    {1},
    /* order 8 */
    {0x26,0xe8,0x95,0x8f,0xc2,0xb2,0x27,0xb0,0x45,0xc3,0xf4,0x89,0xf2,0xef,0x98,0xf0,
     0xd5,0xdf,0xac,0x05,0xd3,0xc6,0x33,0x39,0xb1,0x38,0x02,0x88,0x6d,0x53,0xfc,0x05},
    /* order 8 */
    {0xc7,0x17,0x6a,0x70,0x3d,0x4d,0xd8,0x4f,0xba,0x3c,0x0b,0x76,0x0d,0x10,0x67,0x0f,
     0x2a,0x20,0x53,0xfa,0x2c,0x39,0xcc,0xc6,0x4e,0xc7,0xfd,0x77,0x92,0xac,0x03,0x7a},
    /* p - 1 */
    {0xec,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
     0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f},
    /* p, which reduces to y = 0 */
    {0xed,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
     0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f},
    /* p + 1, which reduces to the identity */
    {0xee,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
     0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f},
    /* non-canonical, above p */
    {0xd9,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
     0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
};

/* RFC 8032 section 7.1, Test 1: empty message. A real key and a real signature,
 * so a check that rejects low-order keys by rejecting everything is caught. */
static const unsigned char RFC8032_PUB[32] = {
    0xd7,0x5a,0x98,0x01,0x82,0xb1,0x0a,0xb7,0xd5,0x4b,0xfe,0xd3,0xc9,0x64,0x07,0x3a,
    0x0e,0xe1,0x72,0xf3,0xda,0xa6,0x23,0x25,0xaf,0x02,0x1a,0x68,0xf7,0x07,0x51,0x1a};
static const unsigned char RFC8032_SIG[64] = {
    0xe5,0x56,0x43,0x00,0xc3,0x60,0xac,0x72,0x90,0x86,0xe2,0xcc,0x80,0x6e,0x82,0x8a,
    0x84,0x87,0x7f,0x1e,0xb8,0xe5,0xd9,0x74,0xd8,0x73,0xe0,0x65,0x22,0x49,0x01,0x55,
    0x5f,0xb8,0x82,0x15,0x90,0xa3,0x3b,0xac,0xc6,0x1e,0x39,0x70,0x1c,0xf9,0xb4,0x6b,
    0xd2,0x5b,0xf5,0xf0,0x59,0x5b,0xbe,0x24,0x65,0x51,0x41,0x43,0x8e,0x7a,0x10,0x0b};

static const unsigned char MSG[] = "untrusted firmware";
#define MSG_LEN (sizeof(MSG) - 1)

/* The all-zero key is an order-4 point, so with S = 0 the equation reduces to
 * [k](-A), and whether that lands on R depends on k mod 4 — that is, on the
 * message. The forgery therefore succeeds for roughly one message in four, not
 * for all of them, and an attacker simply varies padding until one takes.
 *
 * That subtlety matters for this test. An earlier version asserted on a single
 * message and passed against the *unfixed* code, because the message it happened
 * to pick was one of the three-in-four that do not verify. It looked like a
 * regression test and was not one. These are messages verified to be accepted by
 * the unfixed implementation. */
static const char *const FORGEABLE_MESSAGES[] = {
    "malicious package payload",
    "AB",
};

TEST(test_all_zero_key_is_rejected)
{
    /* The exact key eos_pkg shipped. An all-zero signature alongside it needs
     * no key material and no knowledge of the payload. */
    unsigned char key[32] = {0};
    unsigned char sig[64] = {0};
    size_t i;

    for (i = 0; i < sizeof(FORGEABLE_MESSAGES) / sizeof(FORGEABLE_MESSAGES[0]); i++) {
        const unsigned char *m = (const unsigned char *)FORGEABLE_MESSAGES[i];
        ASSERT(ed25519_verify(sig, m, strlen(FORGEABLE_MESSAGES[i]), key) != 1);
    }
}

TEST(test_identity_key_is_rejected)
{
    /* The identity has order 1, which divides L, so a subgroup test alone
     * admits it. It must be rejected explicitly. */
    unsigned char key[32] = {1};
    unsigned char sig[64] = {1};
    ASSERT(ed25519_verify(sig, MSG, MSG_LEN, key) != 1);
}

TEST(test_every_low_order_key_is_rejected)
{
    /* Each low-order key against each low-order point as R: 64 combinations,
     * none of which may verify. */
    for (int k = 0; k < 8; k++) {
        for (int r = 0; r < 8; r++) {
            unsigned char sig[64];
            memset(sig, 0, sizeof(sig));
            memcpy(sig, LOW_ORDER[r], 32);
            ASSERT(ed25519_verify(sig, MSG, MSG_LEN, LOW_ORDER[k]) != 1);
        }
    }
}

TEST(test_rfc8032_vector_still_verifies)
{
    /* Rejecting every key would also pass the tests above. This is what stops
     * the fix from being a denial of service on legitimate signatures. */
    ASSERT(ed25519_verify(RFC8032_SIG, (const unsigned char *)"", 0,
                          RFC8032_PUB) == 1);
}

TEST(test_rfc8032_vector_rejects_a_tampered_message)
{
    const unsigned char tampered[1] = { 'x' };
    ASSERT(ed25519_verify(RFC8032_SIG, tampered, 1, RFC8032_PUB) != 1);
}

int main(void)
{
    printf("=== EoS: Ed25519 low-order key rejection ===\n\n");
    run_test_all_zero_key_is_rejected();
    run_test_identity_key_is_rejected();
    run_test_every_low_order_key_is_rejected();
    run_test_rfc8032_vector_still_verifies();
    run_test_rfc8032_vector_rejects_a_tampered_message();
    tests_run = 5;
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
