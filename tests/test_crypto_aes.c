// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_crypto_aes.c
 * @brief Unit tests for EoS AES-128/256 ECB and CBC encryption
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

/* NIST FIPS-197 Appendix B — AES-128 test vector */
static const uint8_t aes128_key[16] = {
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};
static const uint8_t aes128_plain[16] = {
    0x32, 0x43, 0xf6, 0xa8, 0x88, 0x5a, 0x30, 0x8d,
    0x31, 0x31, 0x98, 0xa2, 0xe0, 0x37, 0x07, 0x34
};
static const uint8_t aes128_cipher[16] = {
    0x39, 0x25, 0x84, 0x1d, 0x02, 0xdc, 0x09, 0xfb,
    0xdc, 0x11, 0x85, 0x97, 0x19, 0x6a, 0x0b, 0x32
};

/* AES-256 test vector (NIST SP 800-38A) */
static const uint8_t aes256_key[32] = {
    0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
    0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
    0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
    0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4
};
static const uint8_t aes256_plain[16] = {
    0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
    0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a
};
static const uint8_t aes256_cipher[16] = {
    0xf3, 0xee, 0xd1, 0xbd, 0xb5, 0xd2, 0xa0, 0x3c,
    0x06, 0x4b, 0x5a, 0x7e, 0x3d, 0xb1, 0x81, 0xf8
};

TEST(test_aes128_ecb_encrypt) {
    EosAesCtx ctx;
    uint8_t out[16];
    eos_aes_init(&ctx, aes128_key, 128);
    eos_aes_encrypt_block(&ctx, aes128_plain, out);
    ASSERT(memcmp(out, aes128_cipher, 16) == 0);
}

TEST(test_aes128_ecb_decrypt) {
    EosAesCtx ctx;
    uint8_t out[16];
    eos_aes_init(&ctx, aes128_key, 128);
    eos_aes_decrypt_block(&ctx, aes128_cipher, out);
    ASSERT(memcmp(out, aes128_plain, 16) == 0);
}

TEST(test_aes128_roundtrip) {
    EosAesCtx ctx;
    uint8_t encrypted[16], decrypted[16];
    const uint8_t data[16] = "Hello, AES-128!";

    eos_aes_init(&ctx, aes128_key, 128);
    eos_aes_encrypt_block(&ctx, data, encrypted);
    eos_aes_decrypt_block(&ctx, encrypted, decrypted);
    ASSERT(memcmp(data, decrypted, 16) == 0);
}

TEST(test_aes256_ecb_encrypt) {
    EosAesCtx ctx;
    uint8_t out[16];
    eos_aes_init(&ctx, aes256_key, 256);
    eos_aes_encrypt_block(&ctx, aes256_plain, out);
    ASSERT(memcmp(out, aes256_cipher, 16) == 0);
}

TEST(test_aes256_ecb_decrypt) {
    EosAesCtx ctx;
    uint8_t out[16];
    eos_aes_init(&ctx, aes256_key, 256);
    eos_aes_decrypt_block(&ctx, aes256_cipher, out);
    ASSERT(memcmp(out, aes256_plain, 16) == 0);
}

TEST(test_aes256_roundtrip) {
    EosAesCtx ctx;
    uint8_t encrypted[16], decrypted[16];
    const uint8_t data[16] = "Hello, AES-256!";

    eos_aes_init(&ctx, aes256_key, 256);
    eos_aes_encrypt_block(&ctx, data, encrypted);
    eos_aes_decrypt_block(&ctx, encrypted, decrypted);
    ASSERT(memcmp(data, decrypted, 16) == 0);
}

TEST(test_aes128_cbc_roundtrip) {
    EosAesCtx ctx;
    const uint8_t iv[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                            0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    uint8_t plaintext[32] = "AES-CBC mode test data!!";
    uint8_t ciphertext[32], recovered[32];

    eos_aes_init(&ctx, aes128_key, 128);
    eos_aes_cbc_encrypt(&ctx, iv, plaintext, ciphertext, 32);
    eos_aes_cbc_decrypt(&ctx, iv, ciphertext, recovered, 32);
    ASSERT(memcmp(plaintext, recovered, 32) == 0);
}

TEST(test_aes256_cbc_roundtrip) {
    EosAesCtx ctx;
    const uint8_t iv[16] = {0xf0, 0xe1, 0xd2, 0xc3, 0xb4, 0xa5, 0x96, 0x87,
                            0x78, 0x69, 0x5a, 0x4b, 0x3c, 0x2d, 0x1e, 0x0f};
    uint8_t plaintext[48];
    uint8_t ciphertext[48], recovered[48];

    memset(plaintext, 0xAB, sizeof(plaintext));
    eos_aes_init(&ctx, aes256_key, 256);
    eos_aes_cbc_encrypt(&ctx, iv, plaintext, ciphertext, 48);
    eos_aes_cbc_decrypt(&ctx, iv, ciphertext, recovered, 48);
    ASSERT(memcmp(plaintext, recovered, 48) == 0);
}

TEST(test_aes_null_safety) {
    eos_aes_init(NULL, aes128_key, 128);
    eos_aes_encrypt_block(NULL, NULL, NULL);
    eos_aes_decrypt_block(NULL, NULL, NULL);
    /* Should not crash */
}

TEST(test_aes_different_keys_produce_different_ciphertext) {
    EosAesCtx ctx1, ctx2;
    uint8_t key2[16] = {0};
    uint8_t out1[16], out2[16];
    const uint8_t data[16] = "Same plaintext!!";

    eos_aes_init(&ctx1, aes128_key, 128);
    eos_aes_init(&ctx2, key2, 128);
    eos_aes_encrypt_block(&ctx1, data, out1);
    eos_aes_encrypt_block(&ctx2, data, out2);
    ASSERT(memcmp(out1, out2, 16) != 0);
}

int main(void) {
    printf("=== EoS: AES Encryption Unit Tests ===\n\n");
    run_test_aes128_ecb_encrypt();
    run_test_aes128_ecb_decrypt();
    run_test_aes128_roundtrip();
    run_test_aes256_ecb_encrypt();
    run_test_aes256_ecb_decrypt();
    run_test_aes256_roundtrip();
    run_test_aes128_cbc_roundtrip();
    run_test_aes256_cbc_roundtrip();
    run_test_aes_null_safety();
    run_test_aes_different_keys_produce_different_ciphertext();
    tests_run = 10;
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
