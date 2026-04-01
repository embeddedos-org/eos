// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file vxworks_int.c
 * @brief VxWorks Interrupt API implementation over EoS HAL
 *
 * Uses a static trampoline table to bridge VxWorks ISR signatures
 * (void routine(int parameter)) to EoS parameterless ISR handlers.
 */

#include "eos/vxworks_int.h"
#include "eos/kernel.h"
#include "eos/hal.h"

/* ----------------------------------------------------------------
 * Configuration
 * ---------------------------------------------------------------- */

#ifndef EOS_VXWORKS_MAX_VECTORS
#define EOS_VXWORKS_MAX_VECTORS 32
#endif

/* Default IRQ priority assigned when connecting an ISR */
#ifndef EOS_VXWORKS_DEFAULT_IRQ_PRIO
#define EOS_VXWORKS_DEFAULT_IRQ_PRIO  8
#endif

/* ----------------------------------------------------------------
 * Interrupt nesting counter for intLock / intUnlock
 * ---------------------------------------------------------------- */

static volatile int s_int_lock_depth = 0;

/* ----------------------------------------------------------------
 * Trampoline table: maps vector → {routine, parameter}
 *
 * EoS eos_irq_register() expects a void(*handler)(void) with no
 * parameters.  We generate a fixed set of trampoline functions, one
 * per vector slot, that look up the registered routine and invoke
 * it with the stored parameter.
 * ---------------------------------------------------------------- */

typedef struct {
    VOIDFUNCPTR routine;
    int         parameter;
    bool        connected;
} vxw_isr_entry_t;

static vxw_isr_entry_t s_isr_table[EOS_VXWORKS_MAX_VECTORS];

/*
 * Macro to define a trampoline function for a given vector index.
 * Each trampoline reads s_isr_table[N] and calls the routine.
 */
#define VXW_TRAMPOLINE(N)                                       \
    static void vxw_isr_trampoline_##N(void) {                  \
        if (s_isr_table[N].routine != NULL) {                   \
            s_isr_table[N].routine(s_isr_table[N].parameter);   \
        }                                                        \
    }

/* Generate 32 trampolines */
VXW_TRAMPOLINE(0)   VXW_TRAMPOLINE(1)   VXW_TRAMPOLINE(2)   VXW_TRAMPOLINE(3)
VXW_TRAMPOLINE(4)   VXW_TRAMPOLINE(5)   VXW_TRAMPOLINE(6)   VXW_TRAMPOLINE(7)
VXW_TRAMPOLINE(8)   VXW_TRAMPOLINE(9)   VXW_TRAMPOLINE(10)  VXW_TRAMPOLINE(11)
VXW_TRAMPOLINE(12)  VXW_TRAMPOLINE(13)  VXW_TRAMPOLINE(14)  VXW_TRAMPOLINE(15)
VXW_TRAMPOLINE(16)  VXW_TRAMPOLINE(17)  VXW_TRAMPOLINE(18)  VXW_TRAMPOLINE(19)
VXW_TRAMPOLINE(20)  VXW_TRAMPOLINE(21)  VXW_TRAMPOLINE(22)  VXW_TRAMPOLINE(23)
VXW_TRAMPOLINE(24)  VXW_TRAMPOLINE(25)  VXW_TRAMPOLINE(26)  VXW_TRAMPOLINE(27)
VXW_TRAMPOLINE(28)  VXW_TRAMPOLINE(29)  VXW_TRAMPOLINE(30)  VXW_TRAMPOLINE(31)

/* Lookup table mapping vector index → trampoline function pointer */
typedef void (*eos_isr_handler_t)(void);

static eos_isr_handler_t s_trampoline_lut[EOS_VXWORKS_MAX_VECTORS] = {
    vxw_isr_trampoline_0,  vxw_isr_trampoline_1,
    vxw_isr_trampoline_2,  vxw_isr_trampoline_3,
    vxw_isr_trampoline_4,  vxw_isr_trampoline_5,
    vxw_isr_trampoline_6,  vxw_isr_trampoline_7,
    vxw_isr_trampoline_8,  vxw_isr_trampoline_9,
    vxw_isr_trampoline_10, vxw_isr_trampoline_11,
    vxw_isr_trampoline_12, vxw_isr_trampoline_13,
    vxw_isr_trampoline_14, vxw_isr_trampoline_15,
    vxw_isr_trampoline_16, vxw_isr_trampoline_17,
    vxw_isr_trampoline_18, vxw_isr_trampoline_19,
    vxw_isr_trampoline_20, vxw_isr_trampoline_21,
    vxw_isr_trampoline_22, vxw_isr_trampoline_23,
    vxw_isr_trampoline_24, vxw_isr_trampoline_25,
    vxw_isr_trampoline_26, vxw_isr_trampoline_27,
    vxw_isr_trampoline_28, vxw_isr_trampoline_29,
    vxw_isr_trampoline_30, vxw_isr_trampoline_31,
};

/* ----------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------- */

STATUS intConnect(int vector, VOIDFUNCPTR routine, int parameter)
{
    if (vector < 0 || vector >= EOS_VXWORKS_MAX_VECTORS) return ERROR;
    if (routine == NULL) return ERROR;

    s_isr_table[vector].routine   = routine;
    s_isr_table[vector].parameter = parameter;
    s_isr_table[vector].connected = true;

    int rc = eos_irq_register((uint16_t)vector,
                              s_trampoline_lut[vector],
                              EOS_VXWORKS_DEFAULT_IRQ_PRIO);
    return (rc == 0) ? OK : ERROR;
}

int intLock(void)
{
    eos_irq_disable();
    int key = s_int_lock_depth;
    s_int_lock_depth++;
    return key;
}

void intUnlock(int lockKey)
{
    if (lockKey >= 0 && s_int_lock_depth > 0) {
        s_int_lock_depth--;
    }
    if (s_int_lock_depth == 0) {
        eos_irq_enable();
    }
}
