/**
 * @file test_crypto.c
 * @brief Unit tests for EoS crypto services (SHA-256, AES, CRC)
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

TEST(test_sha256_empty) {
    char hex[EOS_SHA256_HEX_SIZE];
    eos_sha256_data("", 0, hex);
    ASSERT(strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0);
}

TEST(test_sha256_abc) {
    char hex[EOS_SHA256_HEX_SIZE];
    eos_sha256_data("abc", 3, hex);
    ASSERT(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);
}

TEST(test_sha256_long) {
    const char *msg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    char hex[EOS_SHA256_HEX_SIZE];
    eos_sha256_data(msg, strlen(msg), hex);
    ASSERT(strcmp(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1") == 0);
}

TEST(test_sha256_incremental) {
    EosSha256 ctx;
    uint8_t d1[EOS_SHA256_DIGEST_SIZE], d2[EOS_SHA256_DIGEST_SIZE];
    char h1[EOS_SHA256_HEX_SIZE], h2[EOS_SHA256_HEX_SIZE];

    eos_sha256_init(&ctx);
    eos_sha256_update(&ctx, "a", 1);
    eos_sha256_update(&ctx, "bc", 2);
    eos_sha256_final(&ctx, d1);
    eos_sha256_hex(d1, h1);

    eos_sha256_data("abc", 3, h2);
    ASSERT(strcmp(h1, h2) == 0);
}

TEST(test_crc32_basic) {
    const char *data = "123456789";
    uint32_t crc = eos_crc32(0, data, 9);
    ASSERT(crc == 0xCBF43926);
}

TEST(test_crc32_empty) {
    uint32_t crc = eos_crc32(0, "", 0);
    ASSERT(crc == 0);
}

TEST(test_crc32_incremental) {
    uint32_t full = eos_crc32(0, "Hello, World!", 13);
    uint32_t inc = eos_crc32(0, "Hello, ", 7);
    inc = eos_crc32(inc, "World!", 6);
    ASSERT(full == inc);
}

TEST(test_aes128_encrypt_decrypt) {
    uint8_t key[16] = {0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
                       0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
    uint8_t plain[16] = {0x32,0x43,0xf6,0xa8,0x88,0x5a,0x30,0x8d,
                         0x31,0x31,0x98,0xa2,0xe0,0x37,0x07,0x34};
    uint8_t enc[16], dec[16];

    EosAesCtx ctx;
    eos_aes_init(&ctx, key, 128);
    eos_aes_encrypt_block(&ctx, plain, enc);

    /* Encrypted should differ from plaintext */
    ASSERT(memcmp(enc, plain, 16) != 0);

    /* Decrypt should recover plaintext */
    eos_aes_decrypt_block(&ctx, enc, dec);
    ASSERT(memcmp(dec, plain, 16) == 0);
}

TEST(test_aes256_encrypt_decrypt) {
    uint8_t key[32] = {0};
    uint8_t plain[16] = "Hello AES-256!!";
    uint8_t enc[16], dec[16];

    for (int i = 0; i < 32; i++) key[i] = (uint8_t)i;

    EosAesCtx ctx;
    eos_aes_init(&ctx, key, 256);
    eos_aes_encrypt_block(&ctx, plain, enc);
    ASSERT(memcmp(enc, plain, 16) != 0);

    eos_aes_decrypt_block(&ctx, enc, dec);
    ASSERT(memcmp(dec, plain, 16) == 0);
}

TEST(test_aes_cbc_roundtrip) {
    uint8_t key[16] = {0};
    uint8_t iv[16] = {0};
    uint8_t plain[32] = "This is a 32-byte test message!";
    uint8_t enc[32], dec[32];

    for (int i = 0; i < 16; i++) { key[i] = (uint8_t)(i + 1); iv[i] = (uint8_t)(i + 0x10); }

    EosAesCtx ctx;
    eos_aes_init(&ctx, key, 128);
    eos_aes_cbc_encrypt(&ctx, iv, plain, enc, 32);
    ASSERT(memcmp(enc, plain, 32) != 0);

    eos_aes_cbc_decrypt(&ctx, iv, enc, dec, 32);
    ASSERT(memcmp(dec, plain, 32) == 0);
}

TEST(test_rsa_sign_verify_stub) {
    EosRsaKey key;
    memset(&key, 0, sizeof(key));
    key.key_bits = 2048;
    key.has_private = 1;

    uint8_t hash[32] = {0};
    for (int i = 0; i < 32; i++) hash[i] = (uint8_t)i;

    uint8_t sig[512];
    size_t sig_len;
    ASSERT(eos_rsa_sign_sha256(&key, hash, sig, &sig_len) == 0);
    ASSERT(sig_len == 256);
    ASSERT(eos_rsa_verify_sha256(&key, hash, sig, sig_len) == 0);

    /* Corrupt signature should fail */
    sig[sig_len - 1] ^= 0xFF;
    ASSERT(eos_rsa_verify_sha256(&key, hash, sig, sig_len) != 0);
}

int main(void) {
    printf("=== EoS: Crypto Services Unit Tests ===\n\n");
    run_test_sha256_empty();
    run_test_sha256_abc();
    run_test_sha256_long();
    run_test_sha256_incremental();
    run_test_crc32_basic();
    run_test_crc32_empty();
    run_test_crc32_incremental();
    run_test_aes128_encrypt_decrypt();
    run_test_aes256_encrypt_decrypt();
    run_test_aes_cbc_roundtrip();
    run_test_rsa_sign_verify_stub();
    tests_run = 11;
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
