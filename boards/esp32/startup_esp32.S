// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/*
 * startup_esp32.S -- reset entry for EoS on the ESP32 (PRO CPU)
 *
 * The ESP32 boot ROM reads the image esptool's elf2image wrote at flash
 * offset 0x1000, copies its segments into IRAM and DRAM, and jumps to the ELF
 * entry point, _start, on the PRO CPU. The APP CPU stays held in reset.
 *
 * EoS uses the windowed ABI, as the ROM does. This starts from a clean window
 * state (one live window, at base 0) on its own stack, with window exceptions
 * enabled, before running any compiled code.
 */

#include "xtensa_frame.h"

    .extern _eos_vector_table
    .extern _eos_stack_top
    .extern _eos_bss_start
    .extern _eos_bss_end
    .extern eos_esp32_early_init
    .extern main

    .section .text.startup, "ax"
    .literal_position
    .align  4
    .global _start
    .type   _start, @function
_start:
    /* INTLEVEL 15 (everything masked); EXCM, UM and WOE clear while the
     * window registers are rewritten. */
    movi    a0, XT_PS_INTLEVEL_MASK
    wsr.ps  a0
    rsync

    /* One window, at base 0. Changing WINDOWBASE remaps a0..a15, so nothing
     * live is kept in a register across it. */
    movi    a0, 1
    wsr.windowstart a0
    movi    a0, 0
    wsr.windowbase a0
    rsync

    movi    a1, _eos_stack_top
    movi    a0, _eos_vector_table
    wsr.vecbase a0
    movi    a0, 0
    wsr.intenable a0
    /* Coprocessors (the FPU) stay disabled: the port does not save their
     * state, so any use faults instead of corrupting another task. */
    wsr.cpenable a0
    /* Window exceptions on; still everything masked. _start is the
     * outermost frame: a0 = 0 ends a debugger's backtrace here. */
    movi    a0, XT_PS_WOE | XT_PS_INTLEVEL_MASK
    wsr.ps  a0
    rsync
    movi    a0, 0

    /* Watchdogs first: the ROM armed two of them, and zeroing .bss takes
     * time. eos_esp32_early_init() uses no .bss. */
    call4   eos_esp32_early_init

    movi    a2, _eos_bss_start
    movi    a3, _eos_bss_end
    movi    a4, 0
1:
    bgeu    a2, a3, 2f
    s32i    a4, a2, 0
    addi    a2, a2, 4
    j       1b
2:
    call4   main

    movi    a2, XT_FATAL_MAIN_RETURN
    j       _xt_fatal_entry
    .size   _start, . - _start
