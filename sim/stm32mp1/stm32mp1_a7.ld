/* EoS STM32MP1 Linker Script — Cortex-A7
 * Target: STM32MP157 (Cortex-A7 + Cortex-M4)
 * QEMU:   virt-a15 machine (ARM Cortex-A class)
 * DDR:    0xC0000000 (512 MB on STM32MP157)
 */

OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY {
    /* STM32MP1 memory map */
    SYSRAM      (rwx) : ORIGIN = 0x2FFC0000, LENGTH = 0x00040000  /* 256 KB SYSRAM */
    DDR         (rwx) : ORIGIN = 0xC0000000, LENGTH = 0x20000000  /* 512 MB DDR3 */
    RETRAM      (rw)  : ORIGIN = 0x38000000, LENGTH = 0x00010000  /* 64 KB RETRAM */
    MCU_SRAM    (rw)  : ORIGIN = 0x10000000, LENGTH = 0x00060000  /* 384 KB MCU SRAM */
    /* STM32MP1 peripheral base */
    UART4       (rw)  : ORIGIN = 0x40010000, LENGTH = 0x00001000  /* UART4 */
    RCC         (rw)  : ORIGIN = 0x50000000, LENGTH = 0x00001000  /* RCC */
    GPIOA       (rw)  : ORIGIN = 0x50002000, LENGTH = 0x00000400  /* GPIOA */
    IWDG1       (rw)  : ORIGIN = 0x5C003000, LENGTH = 0x00000400  /* IWDG1 */
    OTA_REGION  (rw)  : ORIGIN = 0xD0000000, LENGTH = 0x00100000  /* OTA scratch in DDR */
}

_stack_size = 0x10000;  /* 64 KB stack */

SECTIONS {
    . = ORIGIN(DDR);

    .text : {
        KEEP(*(.text.boot))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > DDR

    .data : {
        _data_start = .;
        *(.data*)
        . = ALIGN(4);
        _data_end = .;
    } > DDR

    .bss (NOLOAD) : {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
    } > DDR

    .stack (NOLOAD) : {
        . = ALIGN(8);
        _stack_bottom = .;
        . += _stack_size;
        _stack_top = .;
    } > DDR

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.ARM.exidx*) }
}
