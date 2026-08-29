// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file fuzz_devicetree.c
 * @brief libFuzzer harness for the flattened device tree parser
 *
 * A DTB is untrusted input: it arrives from a prior boot stage or a flash
 * region and every offset and length inside it is attacker-influenced. The
 * parser must reject anything malformed without reading outside the blob.
 *
 * Build and run:
 *   cmake -B build -DEOS_BUILD_TESTS=ON -DEOS_ENABLE_FUZZING=ON \
 *         -DCMAKE_C_COMPILER=clang
 *   ./build/tests/fuzz/fuzz_devicetree -max_total_time=60
 */

#include <stddef.h>
#include <stdint.h>

#include "devicetree.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* The parser takes a uint32_t length; skip inputs it could not describe. */
    if (size > UINT32_MAX) return 0;

    /* libFuzzer's own buffer ends exactly at `size` and is redzoned by ASan,
     * so a one-byte over-read of the blob is reported rather than tolerated.
     * The output tree is static only to keep it off the (limited) stack —
     * eos_dt_parse() memsets it before touching anything. */
    static EosDeviceTree dt;
    (void)eos_dt_parse(&dt, data, (uint32_t)size);
    return 0;
}
