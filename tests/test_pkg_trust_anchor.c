/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 EoS Project
 *
 * @file test_pkg_trust_anchor.c
 * @brief eos_pkg must have a real trust anchor, or refuse to verify.
 *
 * #99 taught services/crypto to reject low-order public keys, which closed the
 * half of #98 where eos_pkg accepted every package. It left the other half:
 * the anchor itself was still
 *
 *     static const uint8_t eos_pkg_public_key[32] = {0};
 *
 * so eos_pkg went from accepting everything to rejecting everything, and said
 * "signature verification failed" while doing it -- blaming the package for a
 * key that was never provisioned. A verifier that rejects a correctly signed
 * package is not a working signature check either.
 *
 * The packages below are signed with RFC 8032 section 7.1 vectors, so they are
 * genuinely signed by a real key and a test that only ever rejects is caught.
 *
 * Nothing compiled services/pkg/eos_pkg.c before this change -- it was in no
 * CMakeLists -- which is why none of this was reachable by a test.
 */

#include <stdio.h>
#ifdef _WIN32
#include <eos/eos_windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "eos_pkg.h"
#include <eos/crypto.h>

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

/* ---- RFC 8032 section 7.1 ------------------------------------------------ */

/* Test 2: a 1-byte message, so the package payload is the signed message. */
static const uint8_t T2_PUB[32] = {
 0x3d,0x40,0x17,0xc3,0xe8,0x43,0x89,0x5a,0x92,0xb7,0x0a,0xa7,0x4d,0x1b,0x7e,0xbc,
 0x9c,0x98,0x2c,0xcf,0x2e,0xc4,0x96,0x8c,0xc0,0xcd,0x55,0xf1,0x2a,0xf4,0x66,0x0c};
static const uint8_t T2_SIG[64] = {
 0x92,0xa0,0x09,0xa9,0xf0,0xd4,0xca,0xb8,0x72,0x0e,0x82,0x0b,0x5f,0x64,0x25,0x40,
 0xa2,0xb2,0x7b,0x54,0x16,0x50,0x3f,0x8f,0xb3,0x76,0x22,0x23,0xeb,0xdb,0x69,0xda,
 0x08,0x5a,0xc1,0xe4,0x3e,0x15,0x99,0x6e,0x45,0x8f,0x36,0x13,0xd0,0xf1,0x1d,0x8c,
 0x38,0x7b,0x2e,0xae,0xb4,0x30,0x2a,0xee,0xb0,0x0d,0x29,0x16,0x12,0xbb,0x0c,0x00};
static const uint8_t T2_MSG[1] = { 0x72 };

/* Test 1's key: a real, prime-order key that did not sign the package above. */
static const uint8_t T1_PUB[32] = {
 0xd7,0x5a,0x98,0x01,0x82,0xb1,0x0a,0xb7,0xd5,0x4b,0xfe,0xd3,0xc9,0x64,0x07,0x3a,
 0x0e,0xe1,0x72,0xf3,0xda,0xa6,0x23,0x25,0xaf,0x02,0x1a,0x68,0xf7,0x07,0x51,0x1a};

/* The eight low-order encodings. None can authenticate anything, so none is a
 * usable anchor. The first is the value this file used to ship. */
static const uint8_t LOW_ORDER[8][32] = {
    /* y = 0, order 4 -- the encoding eos_pkg carried as its trust anchor */
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

/* ---- package construction ------------------------------------------------ */

#define EAPP_PATH "test_trust_anchor.eapp"

/* Write a structurally valid .eapp. The header hash is computed over the
 * payload actually written, so the SHA-256 check always passes and every
 * failure below is the signature check, never integrity. */
static void write_eapp(const char *path,
                       const uint8_t *payload, uint32_t payload_len,
                       const uint8_t sig[64])
{
    eapp_header_t h;
    FILE *f;
    EosSha256 c;

    memset(&h, 0, sizeof(h));
    h.magic = EAPP_MAGIC;
    h.version = EAPP_VERSION;
    snprintf(h.name, sizeof(h.name), "%s", "trust-anchor-test");
    snprintf(h.package_id, sizeof(h.package_id), "%s", "org.eos.test.anchor");
    h.ver_major = 1;
    h.arch_count = 1;
    h.binary_offset = (uint32_t)sizeof(h);
    h.binary_size = payload_len;
    memcpy(h.signature, sig, EAPP_SIGNATURE_LEN);

    eos_sha256_init(&c);
    eos_sha256_update(&c, payload, payload_len);
    eos_sha256_final(&c, h.hash);

    /* POSIX creates these fixtures with 0600 so other users cannot rewrite a
     * signed package between creation and verification. Windows has no
     * equivalent portable CRT mode, so its branch uses the build directory's
     * inherited ACL; this test validates package verification, not filesystem
     * permission isolation. */
#ifdef _WIN32
    f = fopen(path, "wb");
#else
    {
        int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
        ASSERT(fd >= 0);
        f = fdopen(fd, "wb");
        if (!f) close(fd);
    }
#endif
    ASSERT(f != NULL);
    ASSERT(fwrite(&h, sizeof(h), 1, f) == 1);
    ASSERT(fwrite(payload, 1, payload_len, f) == payload_len);
    fclose(f);
}

static void write_genuine_eapp(void)
{
    write_eapp(EAPP_PATH, T2_MSG, (uint32_t)sizeof(T2_MSG), T2_SIG);
}

/* ---- tests --------------------------------------------------------------- */

TEST(test_no_anchor_refuses_a_genuine_package)
{
    /* The state this file shipped in, stated as a test: a package signed by a
     * real key, refused. Before the fix this printed "signature verification
     * failed" and the caller had no way to tell that the anchor, not the
     * package, was the problem. */
    ASSERT(eos_pkg_set_trust_anchor(NULL) == 0);
    ASSERT(eos_pkg_trust_anchor() == NULL);

    write_genuine_eapp();
    ASSERT(eos_pkg_verify(EAPP_PATH) != 0);
}

TEST(test_low_order_keys_are_refused_as_anchors)
{
    /* Refused when configured, not when a package arrives. All eight, because
     * the all-zero encoding this file used is only one of them and a fix aimed
     * at that single value would leave the class open. */
    size_t i;
    for (i = 0; i < 8; i++) {
        ASSERT(eos_pkg_set_trust_anchor(LOW_ORDER[i]) != 0);
        ASSERT(eos_pkg_trust_anchor() == NULL);
    }
}

TEST(test_a_real_key_is_accepted_as_an_anchor)
{
    const uint8_t *held;

    ASSERT(eos_pkg_set_trust_anchor(NULL) == 0);
    ASSERT(eos_pkg_set_trust_anchor(T2_PUB) == 0);

    held = eos_pkg_trust_anchor();
    ASSERT(held != NULL);
    ASSERT(memcmp(held, T2_PUB, EAPP_PUBKEY_LEN) == 0);
}

TEST(test_a_genuine_package_verifies_under_its_own_key)
{
    /* Without this the tests above are satisfied by a verifier that rejects
     * everything, which is the bug in its other form. */
    ASSERT(eos_pkg_set_trust_anchor(T2_PUB) == 0);
    write_genuine_eapp();
    ASSERT(eos_pkg_verify(EAPP_PATH) == 0);
}

TEST(test_another_real_key_does_not_verify_the_package)
{
    ASSERT(eos_pkg_set_trust_anchor(T1_PUB) == 0);
    write_genuine_eapp();
    ASSERT(eos_pkg_verify(EAPP_PATH) != 0);
}

TEST(test_a_tampered_payload_is_rejected_by_the_signature)
{
    /* The header hash is recomputed over the tampered payload, so the SHA-256
     * comparison passes and only the signature can reject this. That is the
     * point: the hash travels with the package, so an attacker who replaces
     * the payload replaces the hash too. */
    static const uint8_t tampered[1] = { 0x73 };

    ASSERT(eos_pkg_set_trust_anchor(T2_PUB) == 0);
    write_eapp(EAPP_PATH, tampered, (uint32_t)sizeof(tampered), T2_SIG);
    ASSERT(eos_pkg_verify(EAPP_PATH) != 0);
}

TEST(test_removing_the_anchor_restores_refusal)
{
    ASSERT(eos_pkg_set_trust_anchor(T2_PUB) == 0);
    write_genuine_eapp();
    ASSERT(eos_pkg_verify(EAPP_PATH) == 0);

    ASSERT(eos_pkg_set_trust_anchor(NULL) == 0);
    ASSERT(eos_pkg_trust_anchor() == NULL);
    ASSERT(eos_pkg_verify(EAPP_PATH) != 0);
}

TEST(test_a_rejected_anchor_does_not_replace_the_one_in_force)
{
    /* A failed provisioning attempt must not disarm a working installation. */
    ASSERT(eos_pkg_set_trust_anchor(T2_PUB) == 0);
    ASSERT(eos_pkg_set_trust_anchor(LOW_ORDER[0]) != 0);

    ASSERT(eos_pkg_trust_anchor() != NULL);
    ASSERT(memcmp(eos_pkg_trust_anchor(), T2_PUB, EAPP_PUBKEY_LEN) == 0);

    write_genuine_eapp();
    ASSERT(eos_pkg_verify(EAPP_PATH) == 0);
}

int main(void)
{
    printf("eos_pkg trust anchor\n");

    run_test_no_anchor_refuses_a_genuine_package();
    run_test_low_order_keys_are_refused_as_anchors();
    run_test_a_real_key_is_accepted_as_an_anchor();
    run_test_a_genuine_package_verifies_under_its_own_key();
    run_test_another_real_key_does_not_verify_the_package();
    run_test_a_tampered_payload_is_rejected_by_the_signature();
    run_test_removing_the_anchor_restores_refusal();
    run_test_a_rejected_anchor_does_not_replace_the_one_in_force();

    remove(EAPP_PATH);

    tests_run = 8;
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
