// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_crypto_ecc.c
 * @brief Unit tests for EoS ECDSA P-256 sign/verify operations
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

TEST(test_ecc_sign_returns_status) {
    EosEccKey key;
    memset(&key, 0, sizeof(key));
    key.curve = 256; /* P-256 */

    uint8_t hash[32];
    memset(hash, 0xCC, sizeof(hash));

    uint8_t sig[72];
    size_t sig_len = 0;

    int rc = eos_ecc_sign(&key, hash, 32, sig, &sig_len);
    (void)rc;
}

TEST(test_ecc_verify_returns_status) {
    EosEccKey key;
    memset(&key, 0, sizeof(key));
    key.curve = 256;

    uint8_t hash[32];
    memset(hash, 0xDD, sizeof(hash));

    uint8_t sig[72] = {0};

    int rc = eos_ecc_verify(&key, hash, 32, sig, 64);
    (void)rc;
}

TEST(test_ecc_sign_null_key) {
    uint8_t hash[32] = {0};
    uint8_t sig[72];
    size_t sig_len = 0;

    int rc = eos_ecc_sign(NULL, hash, 32, sig, &sig_len);
    ASSERT(rc != 0);
}

TEST(test_ecc_verify_null_key) {
    uint8_t hash[32] = {0};
    uint8_t sig[72] = {0};

    int rc = eos_ecc_verify(NULL, hash, 32, sig, 64);
    ASSERT(rc != 0);
}

TEST(test_ecc_sign_null_hash) {
    EosEccKey key;
    memset(&key, 0, sizeof(key));
    key.curve = 256;

    uint8_t sig[72];
    size_t sig_len = 0;

    int rc = eos_ecc_sign(&key, NULL, 0, sig, &sig_len);
    ASSERT(rc != 0);
}

TEST(test_ecc_verify_null_sig) {
    EosEccKey key;
    memset(&key, 0, sizeof(key));
    key.curve = 256;

    uint8_t hash[32] = {0};

    int rc = eos_ecc_verify(&key, hash, 32, NULL, 0);
    ASSERT(rc != 0);
}

TEST(test_ecc_key_structure) {
    EosEccKey key;
    ASSERT(sizeof(key.pub) == 64);
    ASSERT(sizeof(key.priv) == 32);
}

int main(void) {
    printf("=== EoS: ECC Crypto Unit Tests ===\n\n");
    run_test_ecc_sign_returns_status();
    run_test_ecc_verify_returns_status();
    run_test_ecc_sign_null_key();
    run_test_ecc_verify_null_key();
    run_test_ecc_sign_null_hash();
    run_test_ecc_verify_null_sig();
    run_test_ecc_key_structure();
    tests_run = 7;
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
