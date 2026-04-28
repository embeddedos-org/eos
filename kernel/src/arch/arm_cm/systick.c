// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file systick.c
 * @brief ARM Cortex-M SysTick handler for EoS kernel
 *
 * Increments the system tick counter, checks for tasks to wake,
 * processes software timers, and triggers PendSV for rescheduling.
 */

#include "eos/kernel.h"
#include "eos/arch.h"

/* ICSR register for pending PendSV */
#define ICSR        (*(volatile uint32_t *)0xE000ED04)
#define PENDSVSET   (1U << 28)

/* Global tick counter (shared with task.c) */
extern volatile uint32_t g_tick;

/* Scheduler needs-reschedule flag */
extern volatile uint8_t g_needs_reschedule;

/* Software timer tick processing (implemented in task.c) */
extern void eos_swtimer_tick(uint32_t current_tick);

/* Task wake check (implemented in task.c) */
extern void eos_task_wake_check(uint32_t current_tick);

/**
 * @brief SysTick interrupt handler.
 *
 * Called every tick (typically 1 ms). Increments tick counter,
 * wakes blocked tasks whose wake_tick has elapsed, processes
 * software timers, and requests a context switch if needed.
 */
void SysTick_Handler(void)
{
    eos_kernel_tick();  /* Increments g_tick */

    /* Wake any tasks whose delay has expired */
    eos_task_wake_check(g_tick);

    /* Process software timers */
    eos_swtimer_tick(g_tick);

    /* Request context switch — PendSV will fire at tail-chain priority */
    ICSR = PENDSVSET;
}
