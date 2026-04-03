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
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
    }
    return ~crc;
}

uint32_t eos_crc32_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    uint32_t crc = 0;
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
        crc = eos_crc32(crc, buf, n);
    fclose(fp);
    return crc;
}

/* ECMA-182 CRC-64 with polynomial 0x42F0E1EBA9EA3693 */
uint64_t eos_crc64(uint64_t crc, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint64_t)p[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xC96C5795D7870F42ULL & (-(crc & 1)));
    }
    return ~crc;
}
