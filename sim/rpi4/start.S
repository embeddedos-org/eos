// SPDX-License-Identifier: MIT
// EoS RPi4 Boot Stub — BCM2711 Cortex-A72
// Raspberry Pi 4 bare-metal entry point
// QEMU raspi3b machine (closest available QEMU target to RPi4)
// Loaded at 0x80000 by QEMU raspi3b

.section ".text.boot"
.global _start

_start:
    // ── ARM64 Image header (required by QEMU raspi3b) ─────────────────────
    // Offset 0: branch to real entry
    b       _real_entry
    .quad   0               // image load offset (0 = load at text base)
    .quad   0               // image size (filled by objcopy)
    .quad   0               // flags
    .quad   0               // reserved
    .quad   0               // reserved
    .quad   0               // reserved
    .quad   0x644d5241      // magic "ARM\x64"
    .long   0               // reserved (PE header offset)

_real_entry:
    // ── CPU0 only — park other cores ─────────────────────────────────────
    mrs     x0, mpidr_el1
    and     x0, x0, #0xFF
    cbnz    x0, _park_core

    // ── Enable FP/NEON (CPACR_EL1.FPEN = 0b11) ──────────────────────────
    mov     x0, #(3 << 20)
    msr     cpacr_el1, x0
    isb

    // ── Set up stack (grows down from 0x80000) ───────────────────────────
    ldr     x0, =_stack_top
    mov     sp, x0

    // ── Clear BSS ────────────────────────────────────────────────────────
    ldr     x0, =_bss_start
    ldr     x1, =_bss_end
    mov     x2, #0
_bss_loop:
    cmp     x0, x1
    b.ge    _bss_done
    str     x2, [x0], #8
    b       _bss_loop
_bss_done:

    // ── Jump to C kernel entry ────────────────────────────────────────────
    bl      eos_kernel_entry_rpi4

_park_core:
    wfi
    b       _park_core
