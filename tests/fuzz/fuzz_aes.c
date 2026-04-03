// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file fuzz_aes.c
 * @brief libFuzzer harness for AES encrypt/decrypt round-trip
 */

#include "eos/crypto.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 16 + 16) return 0; /* Need at least key + one block */

    const uint8_t *key = data;
    const uint8_t *plaintext = data + 16;
    size_t pt_len = size - 16;

    /* Round to block size */
    pt_len = (pt_len / EOS_AES_BLOCK_SIZE) * EOS_AES_BLOCK_SIZE;
    if (pt_len == 0) return 0;

    EosAesCtx ctx;
    eos_aes_init(&ctx, key, 128);

    /* ECB single-block round-trip */
    uint8_t encrypted[16], decrypted[16];
    eos_aes_encrypt_block(&ctx, plaintext, encrypted);
    eos_aes_decrypt_block(&ctx, encrypted, decrypted);

    /* Verify round-trip */
    if (memcmp(plaintext, decrypted, 16) != 0) {
        __builtin_trap();
    }

    return 0;
}
