// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/*
 * uint32_t eos_test_regcheck(uint32_t seed, uint32_t rounds)
 *
 * Fills a0 and a2..a15 and SAR with values derived from seed, spins in a
 * 2000-iteration zero-overhead loop, then checks that every one of them still
 * holds its value and that the loop ran exactly 2000 times. Repeats `rounds`
 * times. Returns 0, or the first failure: 1 = loop count (LBEG/LEND/LCOUNT),
 * 2 = SAR, 3 = an address register.
 *
 * Run by context_test.c in tasks that the tick preempts, so a context switch
 * that loses any of these registers, or the loop state, is caught. CALL0 ABI:
 * a12..a15 and a0 are saved and restored around the check.
 */

    .text
    .literal_position
    .align  4
    .global eos_test_regcheck
    .type   eos_test_regcheck, @function
eos_test_regcheck:
    addi    a1, a1, -32
    s32i    a0,  a1, 0
    s32i    a12, a1, 4
    s32i    a13, a1, 8
    s32i    a14, a1, 12
    s32i    a15, a1, 16
    s32i    a3,  a1, 20         /* rounds left */
    s32i    a2,  a1, 24         /* seed */
1:
    l32i    a2, a1, 24
    ssr     a2                  /* SAR = seed & 31 */
    addi    a4,  a2, 4
    addi    a5,  a2, 5
    addi    a6,  a2, 6
    addi    a7,  a2, 7
    addi    a8,  a2, 8
    addi    a9,  a2, 9
    addi    a10, a2, 10
    addi    a11, a2, 11
    addi    a12, a2, 12
    addi    a13, a2, 13
    addi    a14, a2, 14
    addi    a15, a2, 15
    movi    a0, 0
    movi    a3, 2000
    loop    a3, 2f
    addi    a0, a0, 1
2:
    movi    a3, 2000
    bne     a0, a3, .Lerr_loop
    rsr.sar a3
    extui   a0, a2, 0, 5
    bne     a3, a0, .Lerr_sar
    addi    a3, a2, 4
    bne     a4, a3, .Lerr_reg
    addi    a3, a2, 5
    bne     a5, a3, .Lerr_reg
    addi    a3, a2, 6
    bne     a6, a3, .Lerr_reg
    addi    a3, a2, 7
    bne     a7, a3, .Lerr_reg
    addi    a3, a2, 8
    bne     a8, a3, .Lerr_reg
    addi    a3, a2, 9
    bne     a9, a3, .Lerr_reg
    addi    a3, a2, 10
    bne     a10, a3, .Lerr_reg
    addi    a3, a2, 11
    bne     a11, a3, .Lerr_reg
    addi    a3, a2, 12
    bne     a12, a3, .Lerr_reg
    addi    a3, a2, 13
    bne     a13, a3, .Lerr_reg
    addi    a3, a2, 14
    bne     a14, a3, .Lerr_reg
    addi    a3, a2, 15
    bne     a15, a3, .Lerr_reg
    l32i    a3, a1, 24
    bne     a2, a3, .Lerr_reg
    l32i    a3, a1, 20
    addi    a3, a3, -1
    s32i    a3, a1, 20
    bnez    a3, 1b
    movi    a2, 0
    j       .Ldone
.Lerr_loop:
    movi    a2, 1
    j       .Ldone
.Lerr_sar:
    movi    a2, 2
    j       .Ldone
.Lerr_reg:
    movi    a2, 3
.Ldone:
    l32i    a0,  a1, 0
    l32i    a12, a1, 4
    l32i    a13, a1, 8
    l32i    a14, a1, 12
    l32i    a15, a1, 16
    addi    a1, a1, 32
    ret
    .size   eos_test_regcheck, . - eos_test_regcheck
