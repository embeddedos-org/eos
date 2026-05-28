// SPDX-License-Identifier: MIT
// EoS STM32MP1 Boot Stub — Cortex-A7 (32-bit ARM)
// STM32MP157 — QEMU virt-a15 machine (closest ARM Cortex-A class)
// Loaded at 0xC0000000 (DDR base on STM32MP1)

.section ".text.boot"
.global _start

_start:
    // ── CPU0 only — park other cores ─────────────────────────────────────
    mrc     p15, 0, r0, c0, c0, 5   // MPIDR
    ands    r0, r0, #0xFF
    bne     _park_core

    // ── Enable VFP/NEON ──────────────────────────────────────────────────
    mrc     p15, 0, r0, c1, c0, 2   // CPACR
    orr     r0, r0, #(0xF << 20)    // Enable CP10 and CP11 (VFP/NEON)
    mcr     p15, 0, r0, c1, c0, 2
    isb
    vmrs    r0, fpexc
    orr     r0, r0, #(1 << 30)      // FPEXC.EN = 1
    vmsr    fpexc, r0

    // ── Set up stack ─────────────────────────────────────────────────────
    ldr     sp, =_stack_top

    // ── Clear BSS ────────────────────────────────────────────────────────
    ldr     r0, =_bss_start
    ldr     r1, =_bss_end
    mov     r2, #0
_bss_loop:
    cmp     r0, r1
    bge     _bss_done
    str     r2, [r0], #4
    b       _bss_loop
_bss_done:

    // ── Jump to C kernel entry ────────────────────────────────────────────
    bl      eos_kernel_entry_stm32mp1_a7

_park_core:
    wfi
    b       _park_core
