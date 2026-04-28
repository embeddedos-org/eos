// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file port.c
 * @brief ARM Cortex-M port layer for EoS kernel
 *
 * Implements stack initialization, PendSV yield, and SysTick setup.
 */

#include "eos/arch.h"
#include "eos/kernel.h"
#include <string.h>

/* Cortex-M system registers */
#define ICSR    (*(volatile uint32_t *)0xE000ED04)
#define SHPR3   (*(volatile uint32_t *)0xE000ED20)
#define SYST_CSR (*(volatile uint32_t *)0xE000E010)
#define SYST_RVR (*(volatile uint32_t *)0xE000E014)
#define SYST_CVR (*(volatile uint32_t *)0xE000E018)

#define PENDSVSET_BIT   (1U << 28)
#define SYSTICK_ENABLE  (1U << 0)
#define SYSTICK_TICKINT (1U << 1)
#define SYSTICK_CLKSRC  (1U << 2)

/* EXC_RETURN value for thread mode using PSP with no FPU */
#define EXC_RETURN_PSP  0xFFFFFFFDU

/* Configurable: System clock and tick rate */
#ifndef EOS_SYS_CLOCK_HZ
#define EOS_SYS_CLOCK_HZ   168000000U  /* Default: 168 MHz (STM32F4) */
#endif

#ifndef EOS_TICK_RATE_HZ
#define EOS_TICK_RATE_HZ    1000U       /* 1 ms tick */
#endif

/* Stack canary magic value */
#define EOS_STACK_CANARY    0xDEADBEEFU

/* External references to scheduler globals */
extern uint32_t **g_current_sp;
extern uint32_t **g_next_sp;

uint32_t *eos_port_init_stack(uint32_t *stack_top, void (*entry)(void *), void *arg)
{
    /* ARM Cortex-M exception stack frame (hardware-saved, pushed last):
     *   xPSR, PC, LR, R12, R3, R2, R1, R0
     * Software-saved (pushed first by PendSV):
     *   LR (EXC_RETURN), R11, R10, R9, R8, R7, R6, R5, R4
     */
    uint32_t *sp = stack_top;

    /* Ensure 8-byte alignment (AAPCS requirement) */
    sp = (uint32_t *)((uint32_t)sp & ~7U);

    /* Hardware-saved frame (exception entry) */
    *(--sp) = 0x01000000U;          /* xPSR — Thumb bit set */
    *(--sp) = (uint32_t)entry;      /* PC — task entry point */
    *(--sp) = 0xFFFFFFFEU;          /* LR — invalid (task should never return) */
    *(--sp) = 0;                    /* R12 */
    *(--sp) = 0;                    /* R3 */
    *(--sp) = 0;                    /* R2 */
    *(--sp) = 0;                    /* R1 */
    *(--sp) = (uint32_t)arg;        /* R0 — task argument */

    /* Software-saved registers */
    *(--sp) = EXC_RETURN_PSP;       /* LR (EXC_RETURN) */
    *(--sp) = 0;                    /* R11 */
    *(--sp) = 0;                    /* R10 */
    *(--sp) = 0;                    /* R9 */
    *(--sp) = 0;                    /* R8 */
    *(--sp) = 0;                    /* R7 */
    *(--sp) = 0;                    /* R6 */
    *(--sp) = 0;                    /* R5 */
    *(--sp) = 0;                    /* R4 */

    return sp;
}

void eos_port_start_first_task(void)
{
    /* Set PendSV and SysTick to lowest priority */
    SHPR3 |= (0xFFU << 16);  /* PendSV priority = 0xFF (lowest) */
    SHPR3 |= (0xFFU << 24);  /* SysTick priority = 0xFF (lowest) */

    /* Configure SysTick */
    SYST_RVR = (EOS_SYS_CLOCK_HZ / EOS_TICK_RATE_HZ) - 1;
    SYST_CVR = 0;
    SYST_CSR = SYSTICK_ENABLE | SYSTICK_TICKINT | SYSTICK_CLKSRC;

    /* Trigger SVC to start the first task */
    __asm volatile (
        "cpsie i    \n"
        "svc   0    \n"
    );
}

void eos_port_yield(void)
{
    /* Set PendSV pending bit to trigger context switch */
    ICSR = PENDSVSET_BIT;
    __asm volatile ("dsb");
    __asm volatile ("isb");
}

uint32_t eos_port_enter_critical(void)
{
    uint32_t primask;
    __asm volatile (
        "mrs %0, primask \n"
        "cpsid i         \n"
        : "=r" (primask)
    );
    return primask;
}

void eos_port_exit_critical(uint32_t state)
{
    __asm volatile (
        "msr primask, %0 \n"
        :
        : "r" (state)
    );
}
