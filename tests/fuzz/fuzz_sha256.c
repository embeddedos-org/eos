// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file fuzz_sha256.c
 * @brief libFuzzer harness for SHA-256
 */

#include "eos/crypto.h"
#include <stdint.h>
#include <stddef.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    EosSha256 ctx;
    uint8_t digest[EOS_SHA256_DIGEST_SIZE];
    char hex[EOS_SHA256_HEX_SIZE];

    eos_sha256_init(&ctx);
    eos_sha256_update(&ctx, data, size);
    eos_sha256_final(&ctx, digest);
    eos_sha256_hex(digest, hex);

    /* Also test one-shot API */
    eos_sha256_data(data, size, hex);

    return 0;
}
