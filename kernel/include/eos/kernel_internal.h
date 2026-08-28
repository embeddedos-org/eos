// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file kernel_internal.h
 * @brief Internal kernel API shared between task.c, sync.c, and ipc.c
 *
 * NOT part of the public API. Only included by kernel source files.
 */

#ifndef EOS_KERNEL_INTERNAL_H
#define EOS_KERNEL_INTERNAL_H

#include "eos/kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Global tick counter — incremented by SysTick or platform timer */
extern volatile uint32_t g_tick;

/* Task array — accessible to sync.c and ipc.c for priority inheritance */
extern eos_task_t g_tasks[EOS_MAX_TASKS];

/**
 * @brief True once @p now has reached or passed @p deadline.
 *
 * Compares the signed difference rather than the two values directly, so
 * the result stays correct when the tick counter wraps past UINT32_MAX.
 * An absolute `now >= deadline` test fires immediately as soon as the
 * deadline wraps, waking the task roughly 2^32 ticks early.
 *
 * Valid while the deadline is less than 2^31 ticks ahead — about 24.9 days
 * at 1 ms per tick.
 */
#define EOS_TICK_REACHED(now, deadline) ((int32_t)((uint32_t)(now) - (uint32_t)(deadline)) >= 0)

/**
 * @brief Get a task's current priority.
 */
uint8_t eos_task_get_priority_internal(eos_task_handle_t h);

/**
 * @brief Set a task's priority (used for priority inheritance).
 */
void eos_task_set_priority_internal(eos_task_handle_t h, uint8_t prio);

/**
 * @brief Block a task with a timeout.
 *
 * Sets the task state to BLOCKED and wake_tick to g_tick + timeout_ms,
 * marking the deadline live via wake_armed. If timeout_ms is
 * EOS_WAIT_FOREVER, wake_armed stays 0 and the task never auto-wakes.
 */
void eos_task_block_with_timeout(eos_task_handle_t h, uint32_t timeout_ms);

/**
 * @brief Unblock a task (set state to READY, disarm the wake deadline).
 */
void eos_task_unblock(eos_task_handle_t h);

#ifdef __cplusplus
}
#endif

#endif /* EOS_KERNEL_INTERNAL_H */
