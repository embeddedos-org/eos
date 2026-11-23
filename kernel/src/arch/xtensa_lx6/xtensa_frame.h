// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file xtensa_frame.h
 * @brief Saved-context layout shared by vectors.S and port.c (Xtensa LX6)
 *
 * The port is built with the CALL0 ABI (-mabi=call0), so there are no register
 * windows to spill: a task's whole register state is a0..a15 plus the few
 * special registers below. One layout serves both a preempted task (written by
 * the level-1 interrupt handler) and a task that has never run (written by
 * eos_port_init_stack()), so a single restore path starts either.
 *
 * a1 is not stored. A frame sits at the bottom of the task's stack, and the
 * task's a1 is the frame address plus XT_FRAME_SIZE.
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
/* 84..95: padding. The CALL0 ABI keeps a1 16-byte aligned. */
#define XT_FRAME_SIZE 96

/* PS fields (Xtensa ISA, "Processor Status Special Register") */
#define XT_PS_INTLEVEL_MASK 0x0000000F
#define XT_PS_EXCM          0x00000010
#define XT_PS_UM            0x00000020

/*
 * PS of a task as stored in its frame. UM routes the task's level-1
 * interrupts to the UserExceptionVector. EXCM is set because the restore path
 * writes PS before RFE: with EXCM clear, the write would unmask interrupts in
 * the middle of reloading a0..a15. RFE clears EXCM as it returns, so the task
 * itself runs with EXCM clear and INTLEVEL 0.
 */
#define XT_PS_TASK          (XT_PS_UM | XT_PS_EXCM)

/*
 * PS while the level-1 handler runs C: INTLEVEL 3 (XCHAL_EXCM_LEVEL) masks
 * every level-1 line, and EXCM is clear because the core suppresses
 * zero-overhead loop-back while EXCM is set (Xtensa ISA, Loop Option), which
 * would make every compiled `loop` run once.
 */
#define XT_PS_DISPATCH      3

/* EXCCAUSE value for "level-1 interrupt" */
#define XT_EXCCAUSE_LEVEL1_INT 4

/* CPU interrupt lines on the ESP32 (core-isa.h: XCHAL_TIMER0_INTERRUPT,
 * XCHAL_INT7_TYPE == XTHAL_INTTYPE_SOFTWARE; both are level 1). */
#define XT_INT_TICK   6    /* CCOMPARE0 */
#define XT_INT_YIELD  7    /* software interrupt: the PendSV equivalent */

/* XCHAL_INTLEVEL1_MASK for the ESP32: the CPU lines that are level 1. */
#define XT_INTLEVEL1_MASK 0x000637FF

/* Codes passed to eos_xtensa_fatal() by the vectors. */
#define XT_FATAL_KERNEL_EXC   1   /* exception taken with PS.UM clear */
#define XT_FATAL_USER_EXC     2   /* task exception that is not an interrupt */
#define XT_FATAL_DOUBLE_EXC   3
#define XT_FATAL_WINDOW       4   /* window vector: impossible with CALL0 */
#define XT_FATAL_HIGH_INT     5   /* level 2-5, debug or NMI: none enabled */
#define XT_FATAL_MAIN_RETURN  6
#define XT_FATAL_TASK_RETURN  7   /* a task function returned (task_returned) */

#endif /* EOS_XTENSA_FRAME_H */
