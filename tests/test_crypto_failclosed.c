// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_crypto_failclosed.c
 * @brief Guards that the unimplemented signature routines refuse to run.
 *
 * services/crypto/src/rsa_ecc_sha512.c contains stubs, not signature
 * verification. eos_rsa_verify_sha256() compared only the trailing 32 bytes of
 * the signature against the hash, and eos_ecc_verify() returned success for any
 * 64-byte input; neither read the key. That was reachable from
 * eos_secureboot_verify_image(), so a 256-byte file whose last 32 bytes were an
 * image's own SHA-256 — producible with no private key — made secure boot
 * report VERIFIED for an arbitrary image.
 *
 * They now refuse unless a build defines EOS_ALLOW_STUB_CRYPTO. These tests
 * assert that refusal, so the bypass cannot come back unnoticed. If real
 * verification lands, replace them with known-answer tests against published
 * vectors.
 */

#include "eos/crypto.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, what)                                                    \
    do {                                                                     \
        if (cond) {                                                          \
            printf("  [PASS] %s\n", (what));                                 \
        } else {                                                             \
            printf("  [FAIL] %s (%s:%d)\n", (what), __FILE__, __LINE__);     \
            failures++;                                                      \
        }                                                                    \
    } while (0)

/* The exact forgery that used to pass: a signature whose trailing 32 bytes are
 * the hash, built without any private key. */
static void test_trailing_hash_forgery_is_rejected(void)
{
    EosRsaKey key;
    uint8_t hash[32];
    uint8_t sig[256];

    memset(&key, 0, sizeof(key));
    key.key_bits = 2048;
    memset(hash, 0x5A, sizeof(hash));

    memset(sig, 0, sizeof(sig));
    memcpy(sig + sizeof(sig) - 32, hash, 32);   /* the whole "forgery" */

    CHECK(eos_rsa_verify_sha256(&key, hash, sig, sizeof(sig)) != 0,
          "RSA verify rejects a signature ending in the hash");
}

static void test_rsa_verify_never_succeeds(void)
{
    EosRsaKey key;
    uint8_t hash[32];
    uint8_t sig[256];

    memset(&key, 0, sizeof(key));
    key.key_bits = 2048;
    memset(hash, 0xBB, sizeof(hash));
    memset(sig, 0xCC, sizeof(sig));

    CHECK(eos_rsa_verify_sha256(&key, hash, sig, sizeof(sig)) != 0,
          "RSA verify rejects an arbitrary signature");
    CHECK(eos_rsa_verify_sha256(NULL, hash, sig, sizeof(sig)) != 0,
          "RSA verify rejects a NULL key");
}

static void test_ecc_verify_never_succeeds(void)
{
    EosEccKey key;
    uint8_t hash[32];
    uint8_t sig[64];

    memset(&key, 0, sizeof(key));
    memset(hash, 0x11, sizeof(hash));
    memset(sig, 0x22, sizeof(sig));

    /* This is the input the old stub accepted outright. */
    CHECK(eos_ecc_verify(&key, hash, sizeof(hash), sig, sizeof(sig)) != 0,
          "ECC verify rejects any 64-byte signature");
    CHECK(eos_ecc_verify(NULL, hash, sizeof(hash), sig, sizeof(sig)) != 0,
          "ECC verify rejects a NULL key");
}

static void test_signing_refuses_to_mint_fake_signatures(void)
{
    EosRsaKey rsa;
    EosEccKey ecc;
    uint8_t hash[32];
    uint8_t sig[512];
    size_t sig_len = 0;

    memset(&rsa, 0, sizeof(rsa));
    rsa.key_bits = 2048;
    rsa.has_private = 1;
    memset(&ecc, 0, sizeof(ecc));
    memset(hash, 0x77, sizeof(hash));
    memset(sig, 0, sizeof(sig));

    /* A stub that mints a plausible-looking signature is as dangerous as one
     * that accepts them: it lets a signing pipeline appear to work. */
    CHECK(eos_rsa_sign_sha256(&rsa, hash, sig, &sig_len) != 0,
          "RSA sign refuses to produce a fake signature");
    CHECK(eos_ecc_sign(&ecc, hash, sizeof(hash), sig, &sig_len) != 0,
          "ECC sign refuses to produce a fake signature");
}

int main(void)
{
    printf("=== crypto fail-closed guards ===\n");
    test_trailing_hash_forgery_is_rejected();
    test_rsa_verify_never_succeeds();
    test_ecc_verify_never_succeeds();
    test_signing_refuses_to_mint_fake_signatures();

    printf("%s (%d failures)\n", failures ? "FAILURES" : "ALL PASS", failures);
    return failures == 0 ? 0 : 1;
}
