ENTRY(_start)
MEMORY {
    RAM (rwx) : ORIGIN = 0x40080000, LENGTH = 128M
}
SECTIONS {
    . = 0x40080000;
    .text : {
        KEEP(*(.text.start))
        *(.text.eos_kernel_entry)
        *(.text*)
        *(.rodata*)
    } > RAM
    . = ALIGN(8);
    .data : {
        _data_start = .;
        *(.data*)
        _data_end = .;
    } > RAM
    . = ALIGN(8);
    .bss (NOLOAD) : {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    } > RAM
    . = ALIGN(16);
    . += 0x10000;
    _stack_top = .;
}
