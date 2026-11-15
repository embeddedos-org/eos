// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_config_headers.c
 * @brief Both config headers are usable in one translation unit (issue #260).
 *
 * eos/config.h (EosConfig, the build-tool half) and eos/eos_config.h (the
 * product/feature macros of the RTOS half) shared the include guard
 * EOS_CONFIG_H. Whichever was included first suppressed the other, with no
 * diagnostic: in one order EosConfig was an unknown type; in the other the
 * EOS_ENABLE_* macros stayed undefined and every `#if EOS_ENABLE_x` read 0.
 *
 * This file is the first translation unit in the tree that needs both. It
 * includes them in the order that used to lose the feature macros silently,
 * and fails to COMPILE if either header's contents are missing. A build
 * that passes EOS_ENABLE_CAMERA and EOS_ENABLE_PCIE_DRV with -D would hide
 * the second case; no configuration in the tree does, and eos_include_guards
 * catches the shared guard itself either way.
 */

#include <stdio.h>
#include "eos/config.h"
#include "eos/eos_config.h"

/* Evidence that eos_config.h's body was read: two feature macros that only
 * its default block (or a product profile it includes) ever defines. The
 * flags the root CMakeLists.txt passes with -D for the test build
 * (EOS_ENABLE_MULTICORE and six more) would be defined either way and prove
 * nothing here. */
#if !defined(EOS_ENABLE_CAMERA) || !defined(EOS_ENABLE_PCIE_DRV)
#error "eos/eos_config.h was suppressed: its feature macros are not defined"
#endif

int main(void) {
    /* sizeof needs the complete type: this line compiles only if the body
     * of eos/config.h was read. The test is the compilation; running it
     * only reports that it happened. */
    printf("  %-58s PASS (sizeof(EosConfig) = %lu)\n",
           "config_headers_coexist", (unsigned long)sizeof(EosConfig));
    return 0;
}
