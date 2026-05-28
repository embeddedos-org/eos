/* EoS Generated Linker Script
 * Board:  EoS Reference Board v1.0 rev 1.0
 * CPU:    cortex-a57 (aarch64)
 * Tool:   ebuild cad_pipeline.py
 * DO NOT EDIT — regenerate from EoS Reference Board v1.0.kicad_pcb
 */

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

MEMORY {
    FLASH_NOR            (rx)   : ORIGIN = 0x00000000, LENGTH = 0x04000000
    SRAM_BOOT            (rwx)  : ORIGIN = 0x20000000, LENGTH = 0x00010000
    PERIPH_BASE          (rw)   : ORIGIN = 0x08000000, LENGTH = 0x02000000
    UART0_PL011          (rw)   : ORIGIN = 0x09000000, LENGTH = 0x00001000
    WDT_SP805            (rw)   : ORIGIN = 0x10000000, LENGTH = 0x00001000
    GIC_DIST             (rw)   : ORIGIN = 0x08000000, LENGTH = 0x00010000
    GIC_CPU              (rw)   : ORIGIN = 0x08010000, LENGTH = 0x00010000
    RAM_LPDDR4           (rwx)  : ORIGIN = 0x40000000, LENGTH = 0x20000000
    HEAP_REGION          (rw)   : ORIGIN = 0x48000000, LENGTH = 0x02000000
    OTA_SCRATCH          (rw)   : ORIGIN = 0x50000000, LENGTH = 0x00010000
    FRAMEBUFFER          (rw)   : ORIGIN = 0x3c000000, LENGTH = 0x00800000
}

/* Stack size: 0x10000 bytes */
_stack_size = 0x10000;

SECTIONS {
    . = ORIGIN(RAM_LPDDR4) + 0x00080000;  /* kernel load offset */

    .text : {
        KEEP(*(.text.boot))
        *(.text*)
        *(.rodata*)
        . = ALIGN(8);
    } > RAM_LPDDR4

    .data : {
        _data_start = .;
        *(.data*)
        . = ALIGN(8);
        _data_end = .;
    } > RAM_LPDDR4

    .bss (NOLOAD) : {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        _bss_end = .;
    } > RAM_LPDDR4

    .stack (NOLOAD) : {
        . = ALIGN(16);
        _stack_bottom = .;
        . += _stack_size;
        _stack_top = .;
    } > RAM_LPDDR4

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
