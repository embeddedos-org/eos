// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file test_ed25519_canonical_s.c
 * @brief The [0, L) boundary of sc_is_canonical(), tested directly
 *
 * RFC 8032 5.1.7 requires rejecting a signature whose S is not in [0, L).
 * The shared contract corpus covers the far side of that band -- it carries
 * S + L vectors, which land in [L, 2^253) -- but it cannot reach the boundary
 * itself, because through ed25519_verify() a non-canonical S and a signature
 * that simply fails the group equation are the same observable: 0.
 *
 * That matters in one direction in particular. An off-by-one that refused a
 * valid S = L - 1 would satisfy every vector in the corpus and would reject
 * genuine signatures only in the field. This is the test that discriminates,
 * and it needs no signing key and no `cryptography` module.
 */

#include <stdio.h>
#include <string.h>

/* Include the translation unit to reach its static. The alternative --
 * compiling ed25519_verify.c a second time with a macro that makes the symbol
 * external -- gave the function a linkage that varied by build, which static
 * analysis reads as an unreachable static in the ordinary build. */
#include "../services/crypto/src/ed25519_verify.c"

static int failures = 0;

#define CHECK(cond) do {                                                      \
    if (!(cond)) {                                                            \
        fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
        failures++;                                                           \
    }                                                                         \
} while (0)

/* L = 2^252 + 27742317777372353535851937790883648493, little-endian. */
static const unsigned char L[32] = {
    0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
    0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10
};

/* Subtract one from a little-endian 32-byte scalar. */
static void dec(unsigned char v[32])
{
    int i;
    for (i = 0; i < 32; i++) {
        if (v[i]-- != 0) break;
    }
}

static void inc(unsigned char v[32])
{
    int i;
    for (i = 0; i < 32; i++) {
        if (++v[i] != 0) break;
    }
}

int main(void)
{
    unsigned char s[32];

    printf("=== Ed25519 canonical-S boundary ===\n");

    /* S = 0 is in range. */
    memset(s, 0, sizeof s);
    CHECK(sc_is_canonical(s) == 1);

    /* S = 1. */
    memset(s, 0, sizeof s);
    s[0] = 1;
    CHECK(sc_is_canonical(s) == 1);

    /* S = L - 1 is the largest value in range. This is the one an off-by-one
     * gets wrong, and the one no signature-level test can isolate. */
    memcpy(s, L, sizeof s);
    dec(s);
    CHECK(sc_is_canonical(s) == 1);

    /* S = L is out of range: RFC 8032 says [0, L), half-open. */
    memcpy(s, L, sizeof s);
    CHECK(sc_is_canonical(s) == 0);

    /* S = L + 1. */
    memcpy(s, L, sizeof s);
    inc(s);
    CHECK(sc_is_canonical(s) == 0);

    /* S = 2^252 is BELOW L (L = 2^252 + 2.77e37), so it is in range. Stated
     * because it is the value one reaches for when thinking of the boundary
     * as "2^252", and it is the wrong one. */
    memset(s, 0, sizeof s);
    s[31] = 0x10;
    CHECK(sc_is_canonical(s) == 1);

    /* S = 2^252 + 2^251, which IS in [L, 2^253). This is the band ref10's
     * `signature[63] & 224` test lets through -- 0x18 & 0xE0 == 0, so that
     * check sees nothing wrong -- and it is exactly the gap the corpus's
     * S + L vectors exercise from the other direction. */
    memset(s, 0, sizeof s);
    s[31] = 0x18;
    CHECK((s[31] & 224) == 0);          /* ref10's partial test passes it */
    CHECK(sc_is_canonical(s) == 0);     /* this one does not */

    /* All bits set: far out of range. */
    memset(s, 0xFF, sizeof s);
    CHECK(sc_is_canonical(s) == 0);

    if (failures) {
        fprintf(stderr, "\n%d check(s) failed\n", failures);
        return 1;
    }
    printf("[PASS] the [0, L) boundary holds on both sides\n");
    return 0;
}
