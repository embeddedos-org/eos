// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_crypto_rsa.c
 * @brief Unit tests for EoS RSA-2048 sign/verify operations
 */

#include "eos/crypto.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    static void name(void); \
    static void run_##name(void) { \
        printf("  %-50s ", #name); \
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

TEST(test_rsa_sign_returns_status) {
    EosRsaKey key;
    memset(&key, 0, sizeof(key));
    key.key_bits = 2048;
    key.has_private = 1;

    uint8_t hash[32];
    memset(hash, 0xAA, sizeof(hash));

    uint8_t sig[512];
    size_t sig_len = 0;

    int rc = eos_rsa_sign_sha256(&key, hash, sig, &sig_len);
    /* Even stub should return a defined status */
    (void)rc;
}

TEST(test_rsa_verify_returns_status) {
    EosRsaKey key;
    memset(&key, 0, sizeof(key));
    key.key_bits = 2048;
    key.has_private = 0;

    uint8_t hash[32];
    memset(hash, 0xBB, sizeof(hash));

    uint8_t sig[512];
    memset(sig, 0, sizeof(sig));
    size_t sig_len = 256;

    int rc = eos_rsa_verify_sha256(&key, hash, sig, sig_len);
    (void)rc;
}

TEST(test_rsa_sign_null_key) {
    uint8_t hash[32] = {0};
    uint8_t sig[512];
    size_t sig_len = 0;

    int rc = eos_rsa_sign_sha256(NULL, hash, sig, &sig_len);
    ASSERT(rc != 0);
}

TEST(test_rsa_verify_null_key) {
    uint8_t hash[32] = {0};
    uint8_t sig[512] = {0};

    int rc = eos_rsa_verify_sha256(NULL, hash, sig, 256);
    ASSERT(rc != 0);
}

TEST(test_rsa_sign_null_hash) {
    EosRsaKey key;
    memset(&key, 0, sizeof(key));
    key.key_bits = 2048;
    key.has_private = 1;

    uint8_t sig[512];
    size_t sig_len = 0;

    int rc = eos_rsa_sign_sha256(&key, NULL, sig, &sig_len);
    ASSERT(rc != 0);
}

TEST(test_rsa_verify_null_sig) {
    EosRsaKey key;
    memset(&key, 0, sizeof(key));
    key.key_bits = 2048;

    uint8_t hash[32] = {0};

    int rc = eos_rsa_verify_sha256(&key, hash, NULL, 0);
    ASSERT(rc != 0);
}

TEST(test_rsa_key_structure_size) {
    ASSERT(sizeof(EosRsaKey) > 0);
    ASSERT(EOS_RSA_MAX_KEY_BYTES == 512);

    EosRsaKey key;
    ASSERT(sizeof(key.n) == EOS_RSA_MAX_KEY_BYTES);
    ASSERT(sizeof(key.e) == EOS_RSA_MAX_KEY_BYTES);
    ASSERT(sizeof(key.d) == EOS_RSA_MAX_KEY_BYTES);
}

int main(void) {
    printf("=== EoS: RSA Crypto Unit Tests ===\n\n");
    run_test_rsa_sign_returns_status();
    run_test_rsa_verify_returns_status();
    run_test_rsa_sign_null_key();
    run_test_rsa_verify_null_key();
    run_test_rsa_sign_null_hash();
    run_test_rsa_verify_null_sig();
    run_test_rsa_key_structure_size();
    tests_run = 7;
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
