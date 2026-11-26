// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/*
 * vectors.S -- Xtensa LX6 exception vectors and context switch for EoS
 *
 * Windowed ABI. The ESP32 has 64 address registers, seen 16 at a time through
 * a window that CALL4/8/12 rotate by 4, 8 or 12 and RETW rotates back. When a
 * call would reuse registers an older frame still holds, the core takes a
 * window overflow exception and the handler below stores that frame's
 * registers in the save areas the ABI reserves on the stack; a return into a
 * frame no longer in registers takes an underflow exception that reloads it.
 *
 * Only level-1 interrupts are used: CPU line 6 (CCOMPARE0, the tick) and line
 * 7 (the software interrupt eos_port_yield() raises). Tasks run with PS.UM
 * set, so both arrive at the UserExceptionVector. The handler:
 *
 *   1. saves the interrupted window's a0..a15 and special registers in a frame
 *      XT_BSA_SIZE bytes below the task's stack pointer (xtensa_frame.h);
 *   2. spills every older window to its save area on the task's stack, so
 *      nothing of the task is left only in the register file;
 *   3. runs the C dispatcher on the interrupt stack, which may switch tasks;
 *   4. resets the window state and reloads whichever task g_current_sp names.
 *      Its spilled callers come back through underflow exceptions as it
 *      returns into them.
 *
 * An alloca exception (MOVSP while the caller's window is live) takes the same
 * path without step 3: once the windows are spilled, the retried MOVSP
 * succeeds. Every other vector means something the port does not expect and
 * reports a fatal error instead of returning into a state nothing saved.
 */

#include "xtensa_frame.h"

    .extern eos_xtensa_irq_dispatch
    .extern eos_xtensa_fatal
    .extern _eos_isr_stack_top

/*
 * Per-CPU state. Single-CPU: one saved-SP slot (g_current_sp) and one
 * interrupt stack. SMP (EOS_SMP_CORES > 1, the ESP32's two CPUs): the slot
 * is g_cpu_sp[cpu] and CPU 1 has its own interrupt stack; the CPU number is
 * bit 13 of PRID (0xCDCD on the PRO CPU, 0xABAB on the APP CPU).
 *   cur_sp_slot reg, tmp : reg = address of this CPU's saved-SP slot
 *   isr_stack   reg, tmp : reg = top of this CPU's interrupt stack
 */
#if defined(EOS_SMP_CORES) && EOS_SMP_CORES > 1
    .extern g_cpu_sp
    .extern _eos_isr_stack1_top
    .macro  cur_sp_slot reg, tmp
    rsr.prid \tmp
    extui   \tmp, \tmp, 13, 1
    movi    \reg, g_cpu_sp
    addx4   \reg, \tmp, \reg
    .endm
    .macro  isr_stack reg, tmp
    rsr.prid \tmp
    extui   \tmp, \tmp, 13, 1
    movi    \reg, _eos_isr_stack_top
    beqz    \tmp, .Lisr_stack_\@
    movi    \reg, _eos_isr_stack1_top
.Lisr_stack_\@:
    .endm
#else
    .extern g_current_sp
    .macro  cur_sp_slot reg, tmp
    movi    \reg, g_current_sp
    .endm
    .macro  isr_stack reg, tmp
    movi    \reg, _eos_isr_stack_top
    .endm
#endif

/*
 * Spill every live window except the current one. Each AND reads a12..a15,
 * the registers three windows ahead; if an older frame still holds them the
 * core takes an overflow exception for it, oldest frame first. Five reads,
 * 3+3+3+3+4 windows apart, cover the 15 windows that are not the current one
 * and leave WINDOWBASE where it started. Needs PS.WOE set, PS.EXCM clear and
 * level-1 interrupts masked, and a1 holding the current function's stack
 * pointer: the spill locates the caller's save area from it. (The same
 * sequence as Zephyr's SPILL_ALL_WINDOWS for 64 address registers.)
 */
    .macro  spill_all_windows
    and     a12, a12, a12
    rotw    3
    and     a12, a12, a12
    rotw    3
    and     a12, a12, a12
    rotw    3
    and     a12, a12, a12
    rotw    3
    and     a12, a12, a12
    rotw    4
    .endm

/* WINDOWSTART = 1 << WINDOWBASE: the current window is the only live one.
 * Clobbers a2, a3. */
    .macro  reset_windowstart
    rsr.windowbase a2
    movi    a3, 1
    ssl     a2
    sll     a3, a3
    wsr.windowstart a3
    rsync
    .endm

/* ------------------------------------------------------------------------
 * Vector table. VECBASE must be 1 KB aligned; the linker script places
 * .vectors at the start of IRAM. Offsets are XCHAL_*_VECOFS for the ESP32.
 *
 * The six window handlers are the standard sequences from the Xtensa ISA
 * reference (Windowed Register Option). In each, a0..a3 (a0..a7, a0..a11)
 * are the registers of frame j being saved or restored, and a5 (a9, a13) is
 * the stack pointer of its callee j+1. Frame j's a0..a3 live in the 16 bytes
 * below j+1's stack pointer; for a CALL8/CALL12 frame the extra registers
 * live at the top of j's own frame, located through j's caller's stack
 * pointer, which j's own save area holds at offset -12.
 * ---------------------------------------------------------------------- */
    .section .vectors, "ax"
    .align  1024
    .global _eos_vector_table
_eos_vector_table:

    .org    0x000       /* WindowOverflow4 */
    s32e    a0, a5, -16
    s32e    a1, a5, -12
    s32e    a2, a5, -8
    s32e    a3, a5, -4
    rfwo

    .org    0x040       /* WindowUnderflow4 */
    l32e    a0, a5, -16
    l32e    a1, a5, -12
    l32e    a2, a5, -8
    l32e    a3, a5, -4
    rfwu

    .org    0x080       /* WindowOverflow8 */
    s32e    a0, a9, -16
    l32e    a0, a1, -12
    s32e    a1, a9, -12
    s32e    a2, a9, -8
    s32e    a3, a9, -4
    s32e    a4, a0, -32
    s32e    a5, a0, -28
    s32e    a6, a0, -24
    s32e    a7, a0, -20
    rfwo

    .org    0x0C0       /* WindowUnderflow8 */
    l32e    a1, a9, -12
    l32e    a0, a9, -16
    l32e    a7, a1, -12
    l32e    a2, a9, -8
    l32e    a4, a7, -32
    l32e    a3, a9, -4
    l32e    a5, a7, -28
    l32e    a6, a7, -24
    l32e    a7, a7, -20
    rfwu

    .org    0x100       /* WindowOverflow12 */
    s32e    a0, a13, -16
    l32e    a0, a1, -12
    s32e    a1, a13, -12
    s32e    a2, a13, -8
    s32e    a3, a13, -4
    s32e    a4, a0, -48
    s32e    a5, a0, -44
    s32e    a6, a0, -40
    s32e    a7, a0, -36
    s32e    a8, a0, -32
    s32e    a9, a0, -28
    s32e    a10, a0, -24
    s32e    a11, a0, -20
    rfwo

    .org    0x140       /* WindowUnderflow12 */
    l32e    a1, a13, -12
    l32e    a0, a13, -16
    l32e    a11, a1, -12
    l32e    a2, a13, -8
    l32e    a4, a11, -48
    l32e    a8, a11, -32
    l32e    a3, a13, -4
    l32e    a5, a11, -44
    l32e    a6, a11, -40
    l32e    a7, a11, -36
    l32e    a9, a11, -28
    l32e    a10, a11, -24
    l32e    a11, a11, -20
    rfwu

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
 * User exception entry. On arrival: PS.EXCM is set (level-1 interrupts are
 * masked and no window exception can be taken), a1 is the task's stack
 * pointer, a0 is in EXCSAVE1.
 * ---------------------------------------------------------------------- */
    .text
    .literal_position
    .align  4
_xt_user_exception:
    addi    a1, a1, -XT_STACK_FRAME
    s32i    a2, a1, XT_F_A2
    s32i    a3, a1, XT_F_A3
    rsr.exccause a2
    s32i    a2, a1, XT_F_EXCCAUSE
    movi    a3, XT_EXCCAUSE_LEVEL1_INT
    beq     a2, a3, 1f
    movi    a3, XT_EXCCAUSE_ALLOCA
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

    /* Spill the task's older windows with a1 back at the task's stack
     * pointer (the spill finds the caller's save area through it). The
     * current window's registers are already in the frame. EXCM must be
     * clear for the overflow exceptions to be taken; INTLEVEL 3 keeps
     * level-1 interrupts masked, and with UM clear anything else that faults
     * here is reported through the KernelExceptionVector. EPC1 and PS are
     * already in the frame; the restore path rewrites both. */
    addi    a1, a1, XT_STACK_FRAME
    movi    a2, XT_PS_DISPATCH
    wsr.ps  a2
    rsync
    spill_all_windows
    addi    a1, a1, -XT_STACK_FRAME

    /* An alloca exception only needed the spill: resume the same task at the
     * MOVSP, which now succeeds. */
    l32i    a2, a1, XT_F_EXCCAUSE
    movi    a3, XT_EXCCAUSE_ALLOCA
    beq     a2, a3, _xt_restore_frame

    /* Record the outgoing task's stack pointer. g_current_sp is NULL until
     * eos_kernel_start(), and no interrupt is enabled before then; a2 keeps
     * this frame (CALL4 preserves the caller's a0..a3) so the NULL case
     * still returns to it. */
    mov     a2, a1
    cur_sp_slot a3, a4
    l32i    a3, a3, 0
    beqz    a3, 2f
    s32i    a1, a3, 0
2:
    isr_stack a1, a4
    call4   eos_xtensa_irq_dispatch

    /* Resume whichever task g_current_sp now names. */
    mov     a1, a2
    cur_sp_slot a3, a4
    l32i    a3, a3, 0
    beqz    a3, _xt_restore_frame
    l32i    a1, a3, 0
    /* fall through */

/* ------------------------------------------------------------------------
 * Restore the frame at a1 and return into it with RFE. Masks everything and
 * sets EXCM first, so no interrupt and no window exception can intervene,
 * and makes the current window the only live one: the restored task's older
 * frames are all on its stack.
 * ---------------------------------------------------------------------- */
    .global _xt_restore_frame
_xt_restore_frame:
    movi    a2, XT_PS_EXCM | XT_PS_INTLEVEL_MASK
    wsr.ps  a2
    rsync
    reset_windowstart
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
    addi    a1, a1, XT_STACK_FRAME
    rfe

/* ------------------------------------------------------------------------
 * void eos_xtensa_start_first_task(void)
 * Called by eos_port_start_first_task() once the tick is armed. The caller's
 * windows are abandoned (the boot stack is not used again); the restore path
 * resets the window state and RFE starts the first task.
 * ---------------------------------------------------------------------- */
    .global eos_xtensa_start_first_task
    .type   eos_xtensa_start_first_task, @function
    .align  4
eos_xtensa_start_first_task:
    entry   a1, 16
    cur_sp_slot a2, a3
    l32i    a2, a2, 0
    l32i    a1, a2, 0
    j       _xt_restore_frame
    .size   eos_xtensa_start_first_task, . - eos_xtensa_start_first_task

/* ------------------------------------------------------------------------
 * Where a task function returns to. eos_port_init_stack() makes it the
 * return address of the task's entry function, which RETW then jumps to with
 * the window rotated back to the task's initial frame. Reports
 * XT_FATAL_TASK_RETURN, which a test can tell from any other fault.
 * ---------------------------------------------------------------------- */
    .global eos_xtensa_task_returned
    .align  4
eos_xtensa_task_returned:
    movi    a2, XT_FATAL_TASK_RETURN
    j       _xt_fatal_entry

/* ------------------------------------------------------------------------
 * Fatal path. a2 holds an XT_FATAL_* code; every other register may be the
 * faulting context's, including a1, and the window state may be anything.
 * Mask everything, make the current window the only live one, switch to the
 * interrupt stack and call eos_xtensa_fatal(code, exccause, epc1, excvaddr),
 * which reports and never returns.
 * ---------------------------------------------------------------------- */
    .global _xt_fatal_entry
    .align  4
_xt_fatal_entry:
    movi    a3, XT_PS_EXCM | XT_PS_INTLEVEL_MASK
    wsr.ps  a3
    rsync
    mov     a6, a2
    reset_windowstart
    isr_stack a1, a3
    rsr.exccause a7
    rsr.epc1 a8
    rsr.excvaddr a9
    /* Window exceptions on, EXCM clear so the reporter's loops run (see the
     * spill above), everything still masked. */
    movi    a2, XT_PS_WOE | XT_PS_INTLEVEL_MASK
    wsr.ps  a2
    rsync
    call4   eos_xtensa_fatal
3:
    j       3b
