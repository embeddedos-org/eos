// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file fuzz_ota_header.c
 * @brief libFuzzer harness for OTA firmware header parser
 */

#include <stdint.h>
#include <stddef.h>

/* Forward-declare OTA header parse */
extern int eos_ota_parse_header(const void *data, size_t len);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    eos_ota_parse_header(data, size);
    return 0;
}
