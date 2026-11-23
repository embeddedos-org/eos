/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 EoS Project */

/*
 * EoS on the ESP32, loaded straight from flash by the boot ROM.
 *
 * The ROM copies loadable segments into RAM while it is still running, so
 * they may only go where it does not live. These two regions are the ones
 * ESP-IDF's own second-stage bootloader is linked into for the same reason
 * (components/bootloader/subproject/main/ld/esp32/bootloader.ld, v5.4):
 *
 *   iram_seg  0x40080400, 63 KB   code, vectors, literals
 *   dram_seg  0x3FFF0000, 24 KB   initialised data and read-only data
 *
 * .bss and the stacks are not loaded, only zeroed by _start once the ROM is
 * done, so they can use the 192 KB of DRAM below the ROM's stack
 * (0x3FFB0000-0x3FFE0000, ESP-IDF's dram0_0_seg origin).
 *
 * IRAM can only be read 32 bits at a time, so read-only data must not be
 * placed there: strings are read a byte at a time.
 */

ENTRY(_start)

MEMORY
{
    iram_seg (RX) : org = 0x40080400, len = 0xFC00
    dram_seg (RW) : org = 0x3FFF0000, len = 0x6000
    bss_seg  (RW) : org = 0x3FFB0000, len = 0x30000
}

SECTIONS
{
    .iram0.text :
    {
        _eos_vector_table_start = ABSOLUTE(.);
        KEEP(*(.vectors))
        . = ALIGN(4);
        /* L32R reaches backwards only, so literals precede the code. */
        *(.literal .literal.* .text.startup.literal)
        *(.text.startup)
        *(.text .text.*)
        . = ALIGN(4);
    } > iram_seg

    .dram0.data :
    {
        . = ALIGN(16);
        *(.rodata .rodata.*)
        *(.data .data.*)
        . = ALIGN(4);
    } > dram_seg

    .dram0.bss (NOLOAD) :
    {
        . = ALIGN(16);
        _eos_bss_start = ABSOLUTE(.);
        *(.bss .bss.* COMMON)
        . = ALIGN(16);
        _eos_bss_end = ABSOLUTE(.);
    } > bss_seg

    /* Boot stack: main() and the code before eos_kernel_start(). */
    .stack (NOLOAD) :
    {
        . = ALIGN(16);
        . += 4096;
        _eos_stack_top = ABSOLUTE(.);
    } > bss_seg

    /* Interrupt stack: vectors.S runs the dispatcher and the fatal report
     * here, so task stacks only carry one saved frame. */
    .isr_stack (NOLOAD) :
    {
        . = ALIGN(16);
        . += 2048;
        _eos_isr_stack_top = ABSOLUTE(.);
    } > bss_seg

    /DISCARD/ : { *(.comment) *(.note.*) *(.xtensa.info) *(.xt.prop*) *(.xt.lit*) }
}

/* Boot ROM functions (ESP-IDF v5.4, components/esp_rom/esp32/ld/esp32.rom.ld).
 * The ROM is built for the windowed ABI, as EoS now is. */
PROVIDE(rom_i2c_writeReg = 0x400041a4);

ASSERT((_eos_vector_table_start & 0x3FF) == 0, "VECBASE must be 1 KB aligned")
