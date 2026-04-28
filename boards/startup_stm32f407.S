/* SPDX-License-Identifier: MIT
 * STM32F407VG startup — vector table + Reset_Handler
 * Initializes .data, zeroes .bss, calls SystemInit + main.
 */

    .syntax unified
    .cpu cortex-m4
    .fpu fpv4-sp-d16
    .thumb

    .global g_pfnVectors
    .global Default_Handler
    .global Reset_Handler

/* Linker symbols */
    .word _sidata
    .word _sdata
    .word _edata
    .word _sbss
    .word _ebss

    .section .text.Reset_Handler
    .weak Reset_Handler
    .type Reset_Handler, %function
Reset_Handler:
    /* Set stack pointer */
    ldr sp, =_estack

    /* Copy .data from flash to RAM */
    ldr r0, =_sdata
    ldr r1, =_edata
    ldr r2, =_sidata
    movs r3, #0
    b .Ldata_check
.Ldata_copy:
    ldr r4, [r2, r3]
    str r4, [r0, r3]
    adds r3, r3, #4
.Ldata_check:
    adds r4, r0, r3
    cmp r4, r1
    bcc .Ldata_copy

    /* Zero .bss */
    ldr r0, =_sbss
    ldr r1, =_ebss
    movs r2, #0
    b .Lbss_check
.Lbss_zero:
    str r2, [r0]
    adds r0, r0, #4
.Lbss_check:
    cmp r0, r1
    bcc .Lbss_zero

    /* Enable FPU (CP10/CP11 full access) — skip on Cortex-M3 or when EOS_NO_FPU */
#ifndef EOS_NO_FPU
    ldr r0, =0xE000ED88
    ldr r1, [r0]
    orr r1, r1, #(0xF << 20)
    str r1, [r0]
    dsb
    isb
#endif

    /* Call main */
    bl main

    /* If main returns, loop forever */
.Lhang:
    b .Lhang
    .size Reset_Handler, .-Reset_Handler

/* Default handler for unused interrupts */
    .section .text.Default_Handler,"ax",%progbits
Default_Handler:
    b Default_Handler
    .size Default_Handler, .-Default_Handler

/* Vector table */
    .section .isr_vector,"a",%progbits
    .type g_pfnVectors, %object
    .size g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
    .word _estack               /* 0: Initial SP */
    .word Reset_Handler         /* 1: Reset */
    .word NMI_Handler           /* 2: NMI */
    .word HardFault_Handler     /* 3: HardFault */
    .word MemManage_Handler     /* 4: MemManage */
    .word BusFault_Handler      /* 5: BusFault */
    .word UsageFault_Handler    /* 6: UsageFault */
    .word 0                     /* 7-10: Reserved */
    .word 0
    .word 0
    .word 0
    .word SVC_Handler           /* 11: SVCall */
    .word DebugMon_Handler      /* 12: DebugMon */
    .word 0                     /* 13: Reserved */
    .word PendSV_Handler        /* 14: PendSV */
    .word SysTick_Handler       /* 15: SysTick */

/* Weak aliases — default to Default_Handler */
    .weak NMI_Handler
    .thumb_set NMI_Handler, Default_Handler
    .weak HardFault_Handler
    .thumb_set HardFault_Handler, Default_Handler
    .weak MemManage_Handler
    .thumb_set MemManage_Handler, Default_Handler
    .weak BusFault_Handler
    .thumb_set BusFault_Handler, Default_Handler
    .weak UsageFault_Handler
    .thumb_set UsageFault_Handler, Default_Handler
    .weak SVC_Handler
    .thumb_set SVC_Handler, Default_Handler
    .weak DebugMon_Handler
    .thumb_set DebugMon_Handler, Default_Handler
    .weak PendSV_Handler
    .thumb_set PendSV_Handler, Default_Handler
    .weak SysTick_Handler
    .thumb_set SysTick_Handler, Default_Handler
