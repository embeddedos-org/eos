// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/crypto.h"
#include <string.h>
#include <stdio.h>

/* IEEE 802.3 CRC-32 with standard polynomial 0xEDB88320 */
uint32_t eos_crc32(uint32_t crc, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
    }
    return ~crc;
}

int eos_crc32_file_ex(const char *path, uint32_t *out) {
    if (!path || !out) return -1;

    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;

    uint32_t crc = 0;
    uint8_t buf[4096];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        crc = eos_crc32(crc, buf, n);
    }

    if (ferror(fp)) {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    *out = crc;
    return 0;
}

/* Kept for callers that predate eos_crc32_file_ex(). The 0 it returns on a
 * failed read is indistinguishable from the CRC of an empty file, so new code
 * should use eos_crc32_file_ex(). */
uint32_t eos_crc32_file(const char *path) {
    uint32_t crc = 0;
    return (eos_crc32_file_ex(path, &crc) == 0) ? crc : 0;
}

/* CRC-64/XZ: the ECMA-182 polynomial 0x42F0E1EBA9EA3693 in reflected form
 * (0xC96C5795D7870F42), with an all-ones init and a final complement. Its
 * check value for "123456789" is 0x995DC9BBDF1939FA.
 *
 * This is not CRC-64/ECMA-182 itself, which is MSB-first with a zero init and
 * no final xor and checks to 0x6C40DF5F0B497347. Only the comment was wrong;
 * the implementation is unchanged, since anything already holding a CRC-64
 * computed it this way. */
uint64_t eos_crc64(uint64_t crc, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint64_t)p[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xC96C5795D7870F42ULL : 0);
    }
    return ~crc;
}
