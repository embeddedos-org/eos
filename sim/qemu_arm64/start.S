.section .text.start, "ax"
.global _start
.type _start, %function
_start:
    // ARM64 Linux Image header (64 bytes) — required for QEMU -kernel
    // Magic: ARM64 kernel magic at offset 56 = 0x644d5241 "ARM\x64"
    // Without this header, QEMU doesn't know it's an ARM64 image
    .quad 0x14000010           // code0: branch over header (b +64)
    .quad 0                    // code1
    .quad 0                    // text_offset (0 = load at 0x80000 from RAM base)
    .quad 0                    // image_size
    .quad 0x000a               // flags: little-endian
    .quad 0                    // res2
    .quad 0                    // res3
    .quad 0x644d5241           // magic: ARM\x64

real_start:
    // Set up stack pointer
    ldr x0, =_stack_top
    mov sp, x0
    // Clear BSS
    ldr x1, =_bss_start
    ldr x2, =_bss_end
    mov x0, #0
1:  cmp x1, x2
    b.ge 2f
    str x0, [x1], #8
    b 1b
2:  // Enable FP/NEON (CPACR_EL1.FPEN = 0b11 -- trap-free at EL1 and EL0)
    mov x0, #(3 << 20)
    msr cpacr_el1, x0
    isb
    // Jump to C entry
    bl eos_kernel_entry
3:  wfi
    b 3b
