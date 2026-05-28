/* EoS RPi4 Linker Script
 * Target: Raspberry Pi 4 (BCM2711, Cortex-A72)
 * QEMU:   raspi3b machine (closest QEMU target)
 * Load:   0x80000 (QEMU raspi3b kernel load address)
 */

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

MEMORY {
    /* BCM2711 memory layout */
    SRAM        (rwx) : ORIGIN = 0x00000000, LENGTH = 0x00080000  /* 512 KB SRAM */
    RAM         (rwx) : ORIGIN = 0x00080000, LENGTH = 0x3F780000  /* ~1 GB LPDDR4 */
    PERIPH      (rw)  : ORIGIN = 0xFE000000, LENGTH = 0x01800000  /* BCM2711 peripherals */
    UART0       (rw)  : ORIGIN = 0xFE201000, LENGTH = 0x00001000  /* PL011 UART0 */
    GPIO        (rw)  : ORIGIN = 0xFE200000, LENGTH = 0x00001000  /* GPIO */
    MBOX        (rw)  : ORIGIN = 0xFF800000, LENGTH = 0x00001000  /* VideoCore mailbox */
    GIC_DIST    (rw)  : ORIGIN = 0xFF841000, LENGTH = 0x00001000  /* GIC-400 distributor */
    GIC_CPU     (rw)  : ORIGIN = 0xFF842000, LENGTH = 0x00002000  /* GIC-400 CPU interface */
}

_stack_size = 0x20000;  /* 128 KB stack */

SECTIONS {
    . = ORIGIN(RAM);  /* Load at 0x80000 */

    .text : {
        KEEP(*(.text.boot))
        *(.text*)
        *(.rodata*)
        . = ALIGN(8);
    } > RAM

    .data : {
        _data_start = .;
        *(.data*)
        . = ALIGN(8);
        _data_end = .;
    } > RAM

    .bss (NOLOAD) : {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        _bss_end = .;
    } > RAM

    .stack (NOLOAD) : {
        . = ALIGN(16);
        _stack_bottom = .;
        . += _stack_size;
        _stack_top = .;
    } > RAM

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
