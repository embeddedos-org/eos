// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_crc.c
 * @brief Known-answer and error-path tests for the CRC-32 and CRC-64 helpers.
 *
 * test_crypto.c covers eos_crc32() over a buffer. Neither the file variant nor
 * eos_crc64() had any coverage, and it was the file variant that could not
 * report a failed read at all.
 *
 * Each case calls the function and inspects the out-parameter in separate
 * statements: C does not sequence the arguments of a call against each other,
 * so reading *out inside the same expression that fills it is undefined.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "eos/crypto.h"

#define TMP_FILE "test_crc_tmp.bin"
#define NO_FILE  "test_crc_no_such_file.bin"

static void write_file(const char *path, const void *data, size_t len) {
    FILE *fp = fopen(path, "wb");
    assert(fp != NULL);
    if (len > 0) {
        assert(fwrite(data, 1, len, fp) == len);
    }
    assert(fclose(fp) == 0);
}

static void test_crc32_check_value(void) {
    assert(eos_crc32(0, "123456789", 9) == 0xcbf43926u);
    printf("[PASS] crc32 check value\n");
}

static void test_crc64_check_value(void) {
    /* CRC-64/XZ. CRC-64/ECMA-182 would be 0x6c40df5f0b497347. */
    assert(eos_crc64(0, "123456789", 9) == 0x995dc9bbdf1939faULL);
    printf("[PASS] crc64 check value\n");
}

static void test_file_ex_missing(void) {
    uint32_t crc = 0xdeadbeefu;
    int r = eos_crc32_file_ex(NO_FILE, &crc);
    assert(r == -1);
    assert(crc == 0xdeadbeefu);   /* left untouched on failure */
    printf("[PASS] crc32_file_ex reports a missing file\n");
}

static void test_file_ex_null_args(void) {
    uint32_t crc = 0;
    assert(eos_crc32_file_ex(NULL, &crc) == -1);
    assert(eos_crc32_file_ex(TMP_FILE, NULL) == -1);
    printf("[PASS] crc32_file_ex rejects NULL arguments\n");
}

static void test_file_ex_empty(void) {
    /* The CRC of an empty file is 0 -- the same value eos_crc32_file()
     * returns when it cannot open the file at all. This collision is why the
     * status cannot live in the return value. */
    write_file(TMP_FILE, "", 0);
    uint32_t crc = 0xdeadbeefu;
    int r = eos_crc32_file_ex(TMP_FILE, &crc);
    assert(r == 0);
    assert(crc == 0u);
    assert(remove(TMP_FILE) == 0);
    printf("[PASS] crc32_file_ex succeeds on an empty file\n");
}

static void test_file_ex_content(void) {
    const char *msg = "123456789";
    write_file(TMP_FILE, msg, 9);
    uint32_t crc = 0;
    int r = eos_crc32_file_ex(TMP_FILE, &crc);
    assert(r == 0);
    assert(crc == eos_crc32(0, msg, 9));
    assert(crc == 0xcbf43926u);
    assert(remove(TMP_FILE) == 0);
    printf("[PASS] crc32_file_ex matches the buffer CRC\n");
}

static void test_file_ex_multi_block(void) {
    /* Larger than the 4096-byte read buffer, so the chaining across reads is
     * exercised rather than a single fread. */
    size_t len = 10000;
    uint8_t *big = malloc(len);
    assert(big != NULL);
    for (size_t i = 0; i < len; i++) big[i] = (uint8_t)(i * 7 + 3);

    write_file(TMP_FILE, big, len);
    uint32_t crc = 0;
    int r = eos_crc32_file_ex(TMP_FILE, &crc);
    assert(r == 0);
    assert(crc == eos_crc32(0, big, len));
    assert(remove(TMP_FILE) == 0);
    free(big);
    printf("[PASS] crc32_file_ex spans multiple read blocks\n");
}

static void test_file_legacy_unchanged(void) {
    /* The old entry point keeps the behaviour its existing callers rely on. */
    write_file(TMP_FILE, "123456789", 9);
    assert(eos_crc32_file(TMP_FILE) == 0xcbf43926u);
    assert(remove(TMP_FILE) == 0);
    assert(eos_crc32_file(NO_FILE) == 0u);
    printf("[PASS] crc32_file keeps its old behaviour\n");
}

int main(void) {
    printf("=== CRC tests ===\n");
    test_crc32_check_value();
    test_crc64_check_value();
    test_file_ex_missing();
    test_file_ex_null_args();
    test_file_ex_empty();
    test_file_ex_content();
    test_file_ex_multi_block();
    test_file_legacy_unchanged();
    printf("=== all CRC tests passed ===\n");
    return 0;
}
