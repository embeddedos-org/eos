// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_crypto_sha512.c
 * @brief Unit tests for EoS SHA-512 hashing (NIST test vectors)
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

/* NIST SHA-512 test vectors */

TEST(test_sha512_empty) {
    EosSha512 ctx;
    uint8_t digest[EOS_SHA512_DIGEST_SIZE];
    char hex[EOS_SHA512_HEX_SIZE];

    eos_sha512_init(&ctx);
    eos_sha512_update(&ctx, "", 0);
    eos_sha512_final(&ctx, digest);
    eos_sha512_hex(digest, hex);

    ASSERT(strcmp(hex,
        "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce"
        "47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e") == 0);
}

TEST(test_sha512_abc) {
    EosSha512 ctx;
    uint8_t digest[EOS_SHA512_DIGEST_SIZE];
    char hex[EOS_SHA512_HEX_SIZE];

    eos_sha512_init(&ctx);
    eos_sha512_update(&ctx, "abc", 3);
    eos_sha512_final(&ctx, digest);
    eos_sha512_hex(digest, hex);

    ASSERT(strcmp(hex,
        "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
        "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f") == 0);
}

TEST(test_sha512_nist_448bit) {
    const char *msg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    EosSha512 ctx;
    uint8_t digest[EOS_SHA512_DIGEST_SIZE];
    char hex[EOS_SHA512_HEX_SIZE];

    eos_sha512_init(&ctx);
    eos_sha512_update(&ctx, msg, strlen(msg));
    eos_sha512_final(&ctx, digest);
    eos_sha512_hex(digest, hex);

    ASSERT(strcmp(hex,
        "204a8fc6dda82f0a0ced7beb8e08a41657c16ef468b228a8279be331a703c335"
        "96fd15c13b1b07f9aa1d3bea57789ca031ad85c7a71dd70354ec631238ca3445") == 0);
}

TEST(test_sha512_incremental) {
    EosSha512 ctx;
    uint8_t digest[EOS_SHA512_DIGEST_SIZE];
    char hex[EOS_SHA512_HEX_SIZE];

    eos_sha512_init(&ctx);
    eos_sha512_update(&ctx, "a", 1);
    eos_sha512_update(&ctx, "b", 1);
    eos_sha512_update(&ctx, "c", 1);
    eos_sha512_final(&ctx, digest);
    eos_sha512_hex(digest, hex);

    /* Should match SHA-512("abc") */
    ASSERT(strcmp(hex,
        "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
        "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f") == 0);
}

TEST(test_sha512_null_safety) {
    eos_sha512_init(NULL);
    eos_sha512_update(NULL, "data", 4);
    eos_sha512_final(NULL, NULL);
    eos_sha512_hex(NULL, NULL);
    /* Should not crash */
}

TEST(test_sha512_digest_size) {
    ASSERT(EOS_SHA512_DIGEST_SIZE == 64);
    ASSERT(EOS_SHA512_BLOCK_SIZE == 128);
    ASSERT(EOS_SHA512_HEX_SIZE == 129);
}

int main(void) {
    printf("=== EoS: SHA-512 Unit Tests ===\n\n");
    run_test_sha512_empty();
    run_test_sha512_abc();
    run_test_sha512_nist_448bit();
    run_test_sha512_incremental();
    run_test_sha512_null_safety();
    run_test_sha512_digest_size();
    tests_run = 6;
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
