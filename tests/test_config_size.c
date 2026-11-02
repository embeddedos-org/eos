// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_config_size.c
 * @brief Size-budget guard for EosConfig (issue #148).
 *
 * sizeof(EosConfig) was measured at 3,953,236 bytes on 2026-10-13
 * (host, LP64) — five statics of it are ~19 MB of .bss, and no MCU
 * target in the §25.1 set can hold even one. The redesign in #148
 * (arena / string-table storage for the big array members) will
 * shrink it over several slices; this test is the ratchet that keeps
 * it from growing in the meantime and proves each shrink.
 *
 * RATCHET RULE: EOS_CONFIG_SIZE_CEILING may only move DOWN in future
 * commits. Raising it to make a red build green defeats the guard —
 * shrink the struct instead (see docs/eosconfig-size-inventory.md).
 */

#include <stdio.h>
#include "eos/config.h"

/* Measured 1,251,936 after the #148 RTOS dead-array removal
 * (EosRtosConfig's never-read options array + option_count, -81,936 B
 * from the Step 2a size of 1,333,872), rounded up slightly. Previous
 * ceiling 1,340,000 guarded the post-Step-2a layout. */
#define EOS_CONFIG_SIZE_CEILING 1260000u

int main(void) {
    size_t size = sizeof(EosConfig);
    printf("sizeof(EosConfig) = %zu bytes (ceiling %u)\n", size,
           EOS_CONFIG_SIZE_CEILING);
    if (size > EOS_CONFIG_SIZE_CEILING) {
        printf("FAIL: EosConfig grew past its size budget — see #148 and "
               "docs/eosconfig-size-inventory.md\n");
        return 1;
    }
    printf("PASS: EosConfig is within its size budget\n");
    return 0;
}
