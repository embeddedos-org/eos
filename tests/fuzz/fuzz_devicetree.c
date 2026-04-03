// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file fuzz_devicetree.c
 * @brief libFuzzer harness for device tree parser
 */

#include <stdint.h>
#include <stddef.h>

/* Forward-declare the DTB parse function */
extern int eos_dtb_parse(const void *dtb, size_t len);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Feed arbitrary data to the DTB parser — should never crash */
    eos_dtb_parse(data, size);
    return 0;
}
