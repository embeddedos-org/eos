// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file xtensa_frame.h
 * @brief Saved-context layout shared by vectors.S and port.c (Xtensa LX6)
 *
 * The port uses the windowed ABI (GCC's default for the ESP32). When a task is
 * interrupted, vectors.S saves the 16 registers of the interrupted function's
 * window (a0..a15) in this frame and spills every older window -- the task's
 * callers -- to the save areas the ABI reserves for them on the task's own
 * stack. What is left in the register file is dead, so a context is this
 * frame plus the task's stack. One layout serves both a preempted task and a
 * task that has never run (written by eos_port_init_stack()), so a single
 * restore path starts either.
 *
 * The frame sits XT_BSA_SIZE bytes below the interrupted function's stack
 * pointer: those 16 bytes are the base save area where the spill stores the
 * caller's a0..a3, and where the function's return reloads them from. The
 * task's a1 is therefore the frame address plus XT_STACK_FRAME, and a1 is
 * not stored.
 *
 * Included from assembly, so only #defines.
 */

#ifndef EOS_XTENSA_FRAME_H
#define EOS_XTENSA_FRAME_H

#define XT_F_PC       0    /* EPC1: where the task resumes */
#define XT_F_PS       4    /* PS, always with EXCM set (see XT_PS_TASK) */
#define XT_F_A0       8
#define XT_F_A2      12
#define XT_F_A3      16
#define XT_F_A4      20
#define XT_F_A5      24
#define XT_F_A6      28
#define XT_F_A7      32
#define XT_F_A8      36
#define XT_F_A9      40
#define XT_F_A10     44
#define XT_F_A11     48
#define XT_F_A12     52
#define XT_F_A13     56
#define XT_F_A14     60
#define XT_F_A15     64
#define XT_F_SAR     68
#define XT_F_LBEG    72    /* zero-overhead loop state (XCHAL_HAVE_LOOPS) */
#define XT_F_LEND    76
#define XT_F_LCOUNT  80
#define XT_F_EXCCAUSE 84   /* why the frame was saved (vectors.S uses it) */
/* 88..95: padding to keep a1 16-byte aligned, as the ABI requires. */
#define XT_FRAME_SIZE 96
/* Base save area of the interrupted function's caller, kept free above the
 * frame (see the file comment). */
#define XT_BSA_SIZE   16
#define XT_STACK_FRAME (XT_FRAME_SIZE + XT_BSA_SIZE)

/* PS fields (Xtensa ISA, "Processor Status Special Register") */
#define XT_PS_INTLEVEL_MASK 0x0000000F
#define XT_PS_EXCM          0x00000010
#define XT_PS_UM            0x00000020
#define XT_PS_CALLINC_SHIFT 16
#define XT_PS_WOE           0x00040000

/*
 * PS of a task as stored in its frame. UM routes the task's level-1
 * interrupts to the UserExceptionVector and WOE enables window exceptions.
 * EXCM is set because the restore path writes PS before RFE: with EXCM clear,
 * the write would unmask interrupts in the middle of reloading a0..a15. RFE
 * clears EXCM as it returns, so the task itself runs with EXCM clear and
 * INTLEVEL 0.
 */
#define XT_PS_TASK          (XT_PS_UM | XT_PS_EXCM | XT_PS_WOE)

/*
 * PS while vectors.S spills windows and runs C: INTLEVEL 3 (XCHAL_EXCM_LEVEL)
 * masks every level-1 line; WOE is set and EXCM clear so that window
 * exceptions are taken (with EXCM set one would be a double exception), and
 * because the core suppresses zero-overhead loop-back while EXCM is set
 * (Xtensa ISA, Loop Option), which would make every compiled `loop` run once.
 */
#define XT_PS_DISPATCH      (XT_PS_WOE | 3)

/* EXCCAUSE values (Xtensa ISA, Table 4-64) */
#define XT_EXCCAUSE_ALLOCA     5   /* MOVSP while the caller's window is live */
#define XT_EXCCAUSE_LEVEL1_INT 4

/* CPU interrupt lines on the ESP32 (core-isa.h: XCHAL_TIMER0_INTERRUPT,
 * XCHAL_INT7_TYPE == XTHAL_INTTYPE_SOFTWARE; both are level 1). */
#define XT_INT_TICK   6    /* CCOMPARE0 */
#define XT_INT_YIELD  7    /* software interrupt: the PendSV equivalent */

/* XCHAL_INTLEVEL1_MASK for the ESP32: the CPU lines that are level 1. */
#define XT_INTLEVEL1_MASK 0x000637FF

/* Codes passed to eos_xtensa_fatal() by the vectors. Code 4 was the window
 * vectors under the CALL0 port, which now handle window exceptions. */
#define XT_FATAL_KERNEL_EXC   1   /* exception taken with PS.UM clear */
#define XT_FATAL_USER_EXC     2   /* task exception that is not an interrupt */
#define XT_FATAL_DOUBLE_EXC   3
#define XT_FATAL_HIGH_INT     5   /* level 2-5, debug or NMI: none enabled */
#define XT_FATAL_MAIN_RETURN  6
#define XT_FATAL_TASK_RETURN  7   /* a task function returned */

#endif /* EOS_XTENSA_FRAME_H */
