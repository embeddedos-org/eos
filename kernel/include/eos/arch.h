// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file arch.h
 * @brief Architecture abstraction layer for EoS kernel
 *
 * Each architecture implements these functions in its port.c.
 */

#ifndef EOS_ARCH_H
#define EOS_ARCH_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the stack frame for a new task.
 * @param stack_top  Pointer to the top of the task's stack.
 * @param entry      Task entry function.
 * @param arg        Argument passed to the task entry.
 * @return Initial stack pointer for the task.
 */
uint32_t *eos_port_init_stack(uint32_t *stack_top, void (*entry)(void *), void *arg);

/**
 * @brief Start the first task (called once at kernel start).
 *
 * Sets up the SysTick timer, configures PendSV/SVCall priorities,
 * and restores the first task's context.
 */
void eos_port_start_first_task(void);

/**
 * @brief Trigger a context switch (yield).
 *
 * On ARM Cortex-M this sets the PendSV pending bit.
 */
void eos_port_yield(void);

/**
 * @brief Enter a critical section (disable interrupts).
 * @return Previous interrupt state (for nested critical sections).
 */
uint32_t eos_port_enter_critical(void);

/**
 * @brief Exit a critical section (restore interrupts).
 * @param state  Previous interrupt state from eos_port_enter_critical().
 */
void eos_port_exit_critical(uint32_t state);

/* ============================================================
 * IRQ Management (architecture-specific, low-level)
 *
 * NOTE: These are the arch-level IRQ functions. They differ from
 * the HAL-level eos_irq_* functions in hal.h which provide a
 * higher-level platform-abstracted interface.
 * ============================================================ */

typedef void (*eos_irq_handler_t)(void);

int  eos_arch_irq_register(uint32_t irq_num, eos_irq_handler_t handler);
void eos_arch_irq_enable(uint32_t irq_num);
void eos_arch_irq_disable(uint32_t irq_num);
void eos_arch_irq_set_priority(uint32_t irq_num, uint8_t priority);

#ifdef __cplusplus
}
#endif

#endif /* EOS_ARCH_H */
