/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 EoS Project */

/**
 * @file atomics.S
 * @brief ARM Cortex-M atomic operations using LDREX/STREX
 */

    .syntax unified
    .cpu    cortex-m4
    .thumb

/* int32_t eos_atomic_add_hw(volatile int32_t *ptr, int32_t val) */
    .global eos_atomic_add_hw
    .type   eos_atomic_add_hw, %function
eos_atomic_add_hw:
    dmb
1:  ldrex   r2, [r0]
    add     r3, r2, r1
    strex   r12, r3, [r0]
    cmp     r12, #0
    bne     1b
    dmb
    mov     r0, r2
    bx      lr
    .size eos_atomic_add_hw, . - eos_atomic_add_hw

/* int32_t eos_atomic_sub_hw(volatile int32_t *ptr, int32_t val) */
    .global eos_atomic_sub_hw
    .type   eos_atomic_sub_hw, %function
eos_atomic_sub_hw:
    dmb
1:  ldrex   r2, [r0]
    sub     r3, r2, r1
    strex   r12, r3, [r0]
    cmp     r12, #0
    bne     1b
    dmb
    mov     r0, r2
    bx      lr
    .size eos_atomic_sub_hw, . - eos_atomic_sub_hw

/* int32_t eos_atomic_cas_hw(volatile int32_t *ptr, int32_t expected, int32_t desired) */
    .global eos_atomic_cas_hw
    .type   eos_atomic_cas_hw, %function
eos_atomic_cas_hw:
    dmb
1:  ldrex   r3, [r0]
    cmp     r3, r1
    bne     2f
    strex   r12, r2, [r0]
    cmp     r12, #0
    bne     1b
2:  dmb
    mov     r0, r3
    bx      lr
    .size eos_atomic_cas_hw, . - eos_atomic_cas_hw

/* int32_t eos_atomic_load_hw(volatile const int32_t *ptr) */
    .global eos_atomic_load_hw
    .type   eos_atomic_load_hw, %function
eos_atomic_load_hw:
    dmb
    ldr     r0, [r0]
    dmb
    bx      lr
    .size eos_atomic_load_hw, . - eos_atomic_load_hw

/* void eos_atomic_store_hw(volatile int32_t *ptr, int32_t val) */
    .global eos_atomic_store_hw
    .type   eos_atomic_store_hw, %function
eos_atomic_store_hw:
    dmb
    str     r1, [r0]
    dmb
    bx      lr
    .size eos_atomic_store_hw, . - eos_atomic_store_hw

    .end
