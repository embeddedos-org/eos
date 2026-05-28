OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY {
    /* QEMU virt ARM: RAM starts at 0x40000000 */
    RAM     (rwx) : ORIGIN = 0x40000000, LENGTH = 0x20000000  /* 512 MB */
    /* STM32MP1-like peripheral map (simulated) */
    PERIPH  (rw)  : ORIGIN = 0x08000000, LENGTH = 0x01000000
}

_stack_size = 0x10000;

SECTIONS {
    . = ORIGIN(RAM);

    .text : {
        KEEP(*(.text.boot))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > RAM

    .data : {
        _data_start = .;
        *(.data*)
        . = ALIGN(4);
        _data_end = .;
    } > RAM

    .bss (NOLOAD) : {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
    } > RAM

    .stack (NOLOAD) : {
        . = ALIGN(8);
        _stack_bottom = .;
        . += _stack_size;
        _stack_top = .;
    } > RAM

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.ARM.exidx*) }
}
