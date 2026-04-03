// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eos/crypto.h"

static void test_sha256_empty(void) {
    char hex[EOS_SHA256_HEX_SIZE];
    eos_sha256_data("", 0, hex);
    assert(strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0);
    printf("[PASS] sha256 empty\n");
}

static void test_sha256_abc(void) {
    char hex[EOS_SHA256_HEX_SIZE];
    eos_sha256_data("abc", 3, hex);
    assert(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);
    printf("[PASS] sha256 abc\n");
}

static void test_sha256_nist(void) {
    const char *msg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    char hex[EOS_SHA256_HEX_SIZE];
    eos_sha256_data(msg, strlen(msg), hex);
    assert(strcmp(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1") == 0);
    printf("[PASS] sha256 NIST 448-bit\n");
}

static void test_sha256_incremental(void) {
    EosSha256 ctx;
    eos_sha256_init(&ctx);
    eos_sha256_update(&ctx, "a", 1);
    eos_sha256_update(&ctx, "b", 1);
    eos_sha256_update(&ctx, "c", 1);
    uint8_t digest[EOS_SHA256_DIGEST_SIZE];
    eos_sha256_final(&ctx, digest);
    char hex[EOS_SHA256_HEX_SIZE];
    eos_sha256_hex(digest, hex);
    assert(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);
    printf("[PASS] sha256 incremental\n");
}

static void test_sha256_null(void) {
    eos_sha256_init(NULL);
    eos_sha256_final(NULL, NULL);
    printf("[PASS] sha256 null safety\n");
}

static void test_crc32(void) {
    uint32_t c = eos_crc32(0, "123456789", 9);
    assert(c == 0xCBF43926);
    printf("[PASS] crc32\n");
}

static void test_crc32_incremental(void) {
    uint32_t c = 0;
    c = eos_crc32(c, "1234", 4);
    c = eos_crc32(c, "56789", 5);
    assert(c == 0xCBF43926);
    printf("[PASS] crc32 incremental\n");
}

int main(void) {
    printf("=== EoS Crypto Tests ===\n");
    test_sha256_empty();
    test_sha256_abc();
    test_sha256_nist();
    test_sha256_incremental();
    test_sha256_null();
    test_crc32();
    test_crc32_incremental();
    printf("=== ALL CRYPTO TESTS PASSED (7/7) ===\n");
    return 0;
}