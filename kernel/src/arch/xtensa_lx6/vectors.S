// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/*
 * vectors.S -- Xtensa LX6 exception vectors and context switch for EoS
 *
 * Built with the CALL0 ABI and run with PS.WOE clear, so no window exception
 * is ever taken and a context is a0..a15 plus SAR and the loop registers (see
 * xtensa_frame.h).
 *
 * Only level-1 interrupts are used: CPU line 6 (CCOMPARE0, the tick) and line
 * 7 (the software interrupt eos_port_yield() raises). Tasks run with PS.UM
 * set, so both arrive at the UserExceptionVector. Every other vector means
 * something the port does not expect and reports a fatal error instead of
 * returning into a state nothing saved.
 *
 * The handler saves the interrupted task's frame on that task's stack, stores
 * the stack pointer through g_current_sp, and runs the C dispatcher on a
 * separate interrupt stack. A task stack therefore needs room for one frame
 * (XT_FRAME_SIZE) above its own use, not for the scheduler's call chain: the
 * idle task's stack is 256 bytes. The dispatcher may call eos_schedule(),
 * which points g_current_sp at the next task; the handler then restores
 * whichever frame g_current_sp names.
 */

#include "xtensa_frame.h"

    .extern g_current_sp
    .extern eos_xtensa_irq_dispatch
    .extern eos_xtensa_fatal
    .extern _eos_isr_stack_top

/* ------------------------------------------------------------------------
 * Vector table. VECBASE must be 1 KB aligned; the linker script places
 * .vectors at the start of IRAM. Offsets are XCHAL_*_VECOFS for the ESP32.
 * Each slot loads a fatal code into a2 and jumps; none of them returns.
 * ---------------------------------------------------------------------- */
    .section .vectors, "ax"
    .align  1024
    .global _eos_vector_table
_eos_vector_table:

    .org    0x000       /* WindowOverflow4 */
    movi    a2, XT_FATAL_WINDOW
    j       _xt_fatal_entry
    .org    0x040       /* WindowUnderflow4 */
    movi    a2, XT_FATAL_WINDOW
    j       _xt_fatal_entry
    .org    0x080       /* WindowOverflow8 */
    movi    a2, XT_FATAL_WINDOW
    j       _xt_fatal_entry
    .org    0x0C0       /* WindowUnderflow8 */
    movi    a2, XT_FATAL_WINDOW
    j       _xt_fatal_entry
    .org    0x100       /* WindowOverflow12 */
    movi    a2, XT_FATAL_WINDOW
    j       _xt_fatal_entry
    .org    0x140       /* WindowUnderflow12 */
    movi    a2, XT_FATAL_WINDOW
    j       _xt_fatal_entry
    .org    0x180       /* Level 2 interrupt */
    movi    a2, XT_FATAL_HIGH_INT
    j       _xt_fatal_entry
    .org    0x1C0       /* Level 3 interrupt */
    movi    a2, XT_FATAL_HIGH_INT
    j       _xt_fatal_entry
    .org    0x200       /* Level 4 interrupt */
    movi    a2, XT_FATAL_HIGH_INT
    j       _xt_fatal_entry
    .org    0x240       /* Level 5 interrupt */
    movi    a2, XT_FATAL_HIGH_INT
    j       _xt_fatal_entry
    .org    0x280       /* Debug (level 6) */
    movi    a2, XT_FATAL_HIGH_INT
    j       _xt_fatal_entry
    .org    0x2C0       /* NMI (level 7) */
    movi    a2, XT_FATAL_HIGH_INT
    j       _xt_fatal_entry
    .org    0x300       /* KernelExceptionVector: PS.UM clear */
    movi    a2, XT_FATAL_KERNEL_EXC
    j       _xt_fatal_entry
    .org    0x340       /* UserExceptionVector: tasks, PS.UM set */
    wsr.excsave1 a0
    j       _xt_user_exception
    .org    0x3C0       /* DoubleExceptionVector */
    movi    a2, XT_FATAL_DOUBLE_EXC
    j       _xt_fatal_entry
    .org    0x400

/* ------------------------------------------------------------------------
 * Level-1 interrupt entry. On arrival: PS.EXCM is set (level-1 interrupts are
 * masked), a1 is the task's stack pointer, a0 is in EXCSAVE1.
 * ---------------------------------------------------------------------- */
    .text
    .literal_position
    .align  4
_xt_user_exception:
    addi    a1, a1, -XT_FRAME_SIZE
    s32i    a2, a1, XT_F_A2
    s32i    a3, a1, XT_F_A3
    rsr.exccause a2
    movi    a3, XT_EXCCAUSE_LEVEL1_INT
    beq     a2, a3, 1f
    /* A task faulted (illegal instruction, load/store error, FP use with
     * CPENABLE clear, ...). Nothing can be resumed safely. */
    movi    a2, XT_FATAL_USER_EXC
    j       _xt_fatal_entry
1:
    s32i    a4,  a1, XT_F_A4
    s32i    a5,  a1, XT_F_A5
    s32i    a6,  a1, XT_F_A6
    s32i    a7,  a1, XT_F_A7
    s32i    a8,  a1, XT_F_A8
    s32i    a9,  a1, XT_F_A9
    s32i    a10, a1, XT_F_A10
    s32i    a11, a1, XT_F_A11
    s32i    a12, a1, XT_F_A12
    s32i    a13, a1, XT_F_A13
    s32i    a14, a1, XT_F_A14
    s32i    a15, a1, XT_F_A15
    rsr.excsave1 a2
    s32i    a2,  a1, XT_F_A0
    rsr.epc1 a2
    s32i    a2,  a1, XT_F_PC
    rsr.ps  a2
    s32i    a2,  a1, XT_F_PS
    rsr.sar a2
    s32i    a2,  a1, XT_F_SAR
    rsr.lbeg a2
    s32i    a2,  a1, XT_F_LBEG
    rsr.lend a2
    s32i    a2,  a1, XT_F_LEND
    rsr.lcount a2
    s32i    a2,  a1, XT_F_LCOUNT
    /* The interrupted code may have been inside a zero-overhead loop. Clear
     * LCOUNT so the handler's own code cannot be looped back at that LEND --
     * memcpy, say, can be both the interrupted function and one the
     * dispatcher calls. */
    movi    a2, 0
    wsr.lcount a2

    /* Record the outgoing task's stack pointer. g_current_sp is NULL until
     * eos_kernel_start(), and no interrupt is enabled before then; a12 keeps
     * this frame so the NULL case still returns to it. */
    mov     a12, a1
    movi    a2, g_current_sp
    l32i    a2, a2, 0
    beqz    a2, 2f
    s32i    a1, a2, 0
2:
    movi    a1, _eos_isr_stack_top
    /* Leave exception mode before running C. With PS.EXCM set the core
     * does not loop back at LEND, so every zero-overhead loop GCC emits
     * (find_next_task()'s scan, for one) would run a single iteration.
     * INTLEVEL = XCHAL_EXCM_LEVEL keeps level-1 interrupts masked, and with
     * UM clear any exception the dispatcher takes is reported through the
     * KernelExceptionVector. EPC1 and PS are already in the frame; the
     * restore path rewrites PS (EXCM set) before reloading it. */
    movi    a2, XT_PS_DISPATCH
    wsr.ps  a2
    rsync
    call0   eos_xtensa_irq_dispatch

    /* Resume whichever task g_current_sp now names. */
    mov     a1, a12
    movi    a2, g_current_sp
    l32i    a2, a2, 0
    beqz    a2, _xt_restore_frame
    l32i    a1, a2, 0
    /* fall through */

/* ------------------------------------------------------------------------
 * Restore the frame at a1 and return into it with RFE. Entered with PS.EXCM
 * set or with interrupts masked, so nothing can interrupt the reload.
 * ---------------------------------------------------------------------- */
    .global _xt_restore_frame
_xt_restore_frame:
    l32i    a2, a1, XT_F_SAR
    wsr.sar a2
    l32i    a2, a1, XT_F_LBEG
    wsr.lbeg a2
    l32i    a2, a1, XT_F_LEND
    wsr.lend a2
    l32i    a2, a1, XT_F_LCOUNT
    wsr.lcount a2
    l32i    a2, a1, XT_F_PC
    wsr.epc1 a2
    l32i    a2, a1, XT_F_PS
    wsr.ps  a2          /* EXCM set: still masked (XT_PS_TASK) */
    rsync
    l32i    a0,  a1, XT_F_A0
    l32i    a4,  a1, XT_F_A4
    l32i    a5,  a1, XT_F_A5
    l32i    a6,  a1, XT_F_A6
    l32i    a7,  a1, XT_F_A7
    l32i    a8,  a1, XT_F_A8
    l32i    a9,  a1, XT_F_A9
    l32i    a10, a1, XT_F_A10
    l32i    a11, a1, XT_F_A11
    l32i    a12, a1, XT_F_A12
    l32i    a13, a1, XT_F_A13
    l32i    a14, a1, XT_F_A14
    l32i    a15, a1, XT_F_A15
    l32i    a2,  a1, XT_F_A2
    l32i    a3,  a1, XT_F_A3
    addi    a1, a1, XT_FRAME_SIZE
    rfe

/* ------------------------------------------------------------------------
 * void eos_xtensa_start_first_task(void)
 * Called by eos_port_start_first_task() once the tick is armed. Masks
 * interrupts, then restores the first task's frame; RFE starts it.
 * ---------------------------------------------------------------------- */
    .global eos_xtensa_start_first_task
    .align  4
eos_xtensa_start_first_task:
    rsil    a2, XT_PS_INTLEVEL_MASK
    movi    a2, g_current_sp
    l32i    a2, a2, 0
    l32i    a1, a2, 0
    j       _xt_restore_frame

/* ------------------------------------------------------------------------
 * Fatal path. a2 holds an XT_FATAL_* code; every other register may be the
 * faulting context's, including a1, so switch to the interrupt stack before
 * calling C. eos_xtensa_fatal() reports and never returns.
 * ---------------------------------------------------------------------- */
    .global _xt_fatal_entry
    .align  4
_xt_fatal_entry:
    rsil    a3, XT_PS_INTLEVEL_MASK
    movi    a1, _eos_isr_stack_top
    rsr.exccause a3
    rsr.epc1 a4
    rsr.excvaddr a5
    /* Everything masked, and EXCM clear so the reporter's loops run (see
     * the dispatcher call above). */
    movi    a6, XT_PS_INTLEVEL_MASK
    wsr.ps  a6
    rsync
    call0   eos_xtensa_fatal
3:
    j       3b
