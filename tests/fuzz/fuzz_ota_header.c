// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file fuzz_ota_header.c
 * @brief libFuzzer harness for the OTA update-ingest path
 *
 * This harness previously called
 *
 *     extern int eos_ota_parse_header(const void *data, size_t len);
 *
 * which does not exist anywhere in the tree, and was commented out of
 * tests/fuzz/CMakeLists.txt -- so it was dead twice over and could not have
 * compiled if it had been enabled.
 *
 * What is actually worth fuzzing here is the ingest path, because that is
 * where attacker-controlled bytes arrive: eos_ota_source_t carries a declared
 * size and expected digest that travel WITH the update, and
 * eos_ota_write_chunk() then streams the payload. The harness drives that
 * sequence and finishes with verify/apply so the hash comparison and the
 * authenticity gate are exercised too.
 */

#include "eos/ota.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Refuse everything. The point is to exercise apply()'s gate without letting
 * a fuzz-chosen image count as authentic. */
static int deny(const uint8_t digest[32], const uint8_t *sig, size_t sig_len,
                void *ctx)
{
    (void)digest; (void)sig; (void)sig_len; (void)ctx;
    return -1;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* 36 bytes of control block: 4 for the declared size, 32 for the digest
     * the update claims. Everything after is payload. */
    if (size < 36) return 0;

    eos_ota_source_t src;
    memset(&src, 0, sizeof(src));
    src.expected_size = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                        ((uint32_t)data[2] << 8)  |  (uint32_t)data[3];
    memcpy(src.expected_sha256, data + 4, 32);

    const uint8_t *payload = data + 36;
    size_t payload_len = size - 36;

    if (eos_ota_init() != 0) return 0;
    eos_ota_set_authenticator(deny, NULL);

    bool available = false;
    eos_ota_check_update(&src, &available);

    if (eos_ota_begin(&src) == 0) {
        /* Feed the payload in fuzz-chosen slices so chunk boundaries vary. */
        size_t off = 0;
        while (off < payload_len) {
            size_t chunk = (size_t)(payload[off] % 64u) + 1u;
            if (off + chunk > payload_len) chunk = payload_len - off;
            if (eos_ota_write_chunk(payload + off, chunk) != 0) break;
            off += chunk;
        }
        eos_ota_finish();
        eos_ota_verify();
        eos_ota_apply();        /* must never install: deny() refuses */
        eos_ota_rollback();
    }

    eos_ota_abort();
    eos_ota_deinit();
    return 0;
}
