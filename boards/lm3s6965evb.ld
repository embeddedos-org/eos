/* QEMU LM3S6965EVB Linker Script — Flash at 0x00000000 */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 256K
    RAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 64K
}
ENTRY(Reset_Handler)
_estack = ORIGIN(RAM) + LENGTH(RAM);
_Min_Heap_Size = 0x2000;
_Min_Stack_Size = 0x1000;

SECTIONS
{
    .isr_vector : { . = ALIGN(4); KEEP(*(.isr_vector)); . = ALIGN(4); } >FLASH
    .text : { . = ALIGN(4); *(.text) *(.text*) *(.glue_7) *(.glue_7t) KEEP(*(.init)) KEEP(*(.fini)) . = ALIGN(4); _etext = .; } >FLASH
    .rodata : { . = ALIGN(4); *(.rodata) *(.rodata*) . = ALIGN(4); } >FLASH
    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
    .ARM : { __exidx_start = .; *(.ARM.exidx*); __exidx_end = .; } >FLASH
    _sidata = LOADADDR(.data);
    .data : { . = ALIGN(4); _sdata = .; *(.data) *(.data*) . = ALIGN(4); _edata = .; } >RAM AT> FLASH
    .bss : { . = ALIGN(4); _sbss = .; __bss_start__ = _sbss; *(.bss) *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; __bss_end__ = _ebss; } >RAM
    ._heap : { . = ALIGN(8); __heap_start = .; . = . + _Min_Heap_Size; __heap_end = .; } >RAM
    ._stack : { . = ALIGN(8); . = . + _Min_Stack_Size; } >RAM
}
