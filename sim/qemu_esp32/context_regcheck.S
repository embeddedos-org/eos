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
 * that loses any of these registers, or the loop state, is caught. Windowed
 * ABI: the function owns a0..a15 of its window; a0 (the return address) is
 * kept on the stack while it serves as the loop counter.
 */

    .text
    .literal_position
    .align  4
    .global eos_test_regcheck
    .type   eos_test_regcheck, @function
eos_test_regcheck:
    entry   a1, 48
    s32i    a0,  a1, 0
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
    retw
    .size   eos_test_regcheck, . - eos_test_regcheck

/*
 * uint32_t eos_test_win4(uint32_t n, uint32_t x)
 * uint32_t eos_test_win12(uint32_t n, uint32_t x)
 *
 * Recursions that call themselves with CALL4 and CALL12; GCC itself emits only
 * CALL8, so without these the 4- and 12-register window handlers would never
 * run. Each level keeps values live across the call in the registers that
 * call size preserves (a2..a3 for CALL4, a2..a11 for CALL12) and folds them
 * into the result, so a frame the handlers save or reload wrongly changes it.
 * context_test.c computes the expected values in C:
 *   win4(0, x)  = x;  win4(n, x)  = (win4(n-1, x+n) ^ x) + n
 *   win12(0, x) = x;  win12(n, x) = fold(win12(n-1, x^n), x) + n, where fold
 *   alternately adds and XORs x+4 .. x+11.
 */
    .global eos_test_win4
    .type   eos_test_win4, @function
    .align  4
eos_test_win4:
    entry   a1, 32
    bnez    a2, 1f
    mov     a2, a3
    retw
1:
    addi    a6, a2, -1
    add     a7, a3, a2
    call4   eos_test_win4
    xor     a6, a6, a3
    add     a2, a6, a2
    retw
    .size   eos_test_win4, . - eos_test_win4

    .global eos_test_win12
    .type   eos_test_win12, @function
    .align  4
eos_test_win12:
    entry   a1, 64
    bnez    a2, 1f
    mov     a2, a3
    retw
1:
    addi    a4,  a3, 4
    addi    a5,  a3, 5
    addi    a6,  a3, 6
    addi    a7,  a3, 7
    addi    a8,  a3, 8
    addi    a9,  a3, 9
    addi    a10, a3, 10
    addi    a11, a3, 11
    addi    a14, a2, -1
    xor     a15, a3, a2
    call12  eos_test_win12
    add     a14, a14, a4
    xor     a14, a14, a5
    add     a14, a14, a6
    xor     a14, a14, a7
    add     a14, a14, a8
    xor     a14, a14, a9
    add     a14, a14, a10
    xor     a14, a14, a11
    add     a2, a14, a2
    retw
    .size   eos_test_win12, . - eos_test_win12
