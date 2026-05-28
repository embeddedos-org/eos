/* EoS QEMU virt ARM64 linker script
   Memory map matches board_qemu_arm64.h:
   FLASH: 0x00000000 - 0x03FFFFFF (64 MB)
   RAM:   0x40000000 - 0x5FFFFFFF (512 MB)
*/
ENTRY(eos_kernel_entry)
MEMORY {
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 64M
    RAM   (rwx) : ORIGIN = 0x40000000, LENGTH = 512M
}
PHDRS {
    text  PT_LOAD FLAGS(5);
    data  PT_LOAD FLAGS(6);
}
SECTIONS {
    .text : {
        KEEP(*(.vectors))
        *(.text.eos_kernel_entry)
        *(.text*)
        *(.rodata*)
    } > FLASH :text
    .data : {
        _data_start = .;
        *(.data*)
        _data_end = .;
    } > RAM AT > FLASH :data
    .bss (NOLOAD) : {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    } > RAM :data
    .stack (NOLOAD) : {
        . = ALIGN(16);
        . += 0x10000;
        _stack_top = .;
    } > RAM :data
    _heap_start = _bss_end;
    _heap_end   = ORIGIN(RAM) + LENGTH(RAM) - 0x10000;
}
