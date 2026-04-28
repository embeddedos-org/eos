/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 EoS Project */

/**
 * @file context_switch.S
 * @brief ARM Cortex-M PendSV context switch handler
 *
 * Saves/restores R4-R11 and LR (EXC_RETURN) manually.
 * R0-R3, R12, LR, PC, xPSR are saved by hardware on exception entry.
 */

    .syntax unified
    .cpu    cortex-m4
    .thumb

    .extern g_current_sp     /* uint32_t *g_current_sp — current task's SP slot */
    .extern g_next_sp        /* uint32_t *g_next_sp — next task's SP slot */
    .extern eos_schedule     /* void eos_schedule(void) — pick next task */

    .global PendSV_Handler
    .type   PendSV_Handler, %function

/**
 * PendSV_Handler — triggered by setting ICSR.PENDSVSET.
 *
 * On entry the hardware has already pushed {R0-R3, R12, LR, PC, xPSR}
 * onto the current task's PSP. We save {R4-R11, LR} manually, call the
 * scheduler, then restore the next task's context.
 */
PendSV_Handler:
    /* Disable interrupts during context switch */
    cpsid   i
    isb

    /* ---- Save current context ---- */
    mrs     r0, psp             /* R0 = current task's PSP */
    tst     lr, #0x10           /* Check if FPU context is active (bit 4 of EXC_RETURN) */
    it      eq
    vstmdbeq r0!, {s16-s31}     /* Save upper FPU regs if FPU was used */
    stmdb   r0!, {r4-r11, lr}   /* Push R4-R11 and LR (EXC_RETURN) */

    /* Store the updated SP into the current task's TCB */
    ldr     r1, =g_current_sp
    ldr     r1, [r1]            /* R1 = &(current_task->stack_ptr) */
    str     r0, [r1]            /* Save PSP */

    /* ---- Select next task ---- */
    push    {lr}
    bl      eos_schedule        /* Updates g_current_sp and g_next_sp */
    pop     {lr}

    /* ---- Restore next context ---- */
    ldr     r1, =g_next_sp
    ldr     r1, [r1]            /* R1 = &(next_task->stack_ptr) */
    ldr     r0, [r1]            /* R0 = next task's saved PSP */

    ldmia   r0!, {r4-r11, lr}   /* Pop R4-R11 and LR (EXC_RETURN) */
    tst     lr, #0x10
    it      eq
    vldmiaeq r0!, {s16-s31}     /* Restore upper FPU regs if needed */
    msr     psp, r0             /* Set PSP to next task's stack */

    /* Enable interrupts and return */
    cpsie   i
    isb
    bx      lr                  /* EXC_RETURN — hardware restores {R0-R3,R12,LR,PC,xPSR} */

    .size PendSV_Handler, . - PendSV_Handler

/**
 * SVC_Handler — used to start the very first task.
 */
    .global SVC_Handler
    .type   SVC_Handler, %function
SVC_Handler:
    ldr     r1, =g_next_sp
    ldr     r1, [r1]
    ldr     r0, [r1]

    ldmia   r0!, {r4-r11, lr}
    msr     psp, r0
    /* Switch to PSP for thread mode */
    mov     r0, #2
    msr     control, r0
    isb
    cpsie   i
    bx      lr

    .size SVC_Handler, . - SVC_Handler
    .end
