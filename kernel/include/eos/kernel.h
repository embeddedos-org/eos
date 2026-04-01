// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file kernel.h
 * @brief EoS Lightweight RTOS Kernel API
 *
 * Provides task management, synchronization primitives (mutex, semaphore),
 * inter-process communication (message queues, mailboxes), and software timers
 * for resource-constrained embedded systems.
 */

#ifndef EOS_KERNEL_H
#define EOS_KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Kernel Configuration
 * ============================================================ */

#ifndef EOS_MAX_TASKS
#define EOS_MAX_TASKS       16
#endif

#ifndef EOS_MAX_MUTEXES
#define EOS_MAX_MUTEXES     8
#endif

#ifndef EOS_MAX_SEMAPHORES
#define EOS_MAX_SEMAPHORES  8
#endif

#ifndef EOS_MAX_QUEUES
#define EOS_MAX_QUEUES      8
#endif

#ifndef EOS_MAX_TIMERS
#define EOS_MAX_TIMERS      8
#endif

#define EOS_WAIT_FOREVER    0xFFFFFFFFU
#define EOS_NO_WAIT         0U

/* ============================================================
 * Kernel Return Codes
 * ============================================================ */

#define EOS_KERN_OK          0
#define EOS_KERN_ERR        -1
#define EOS_KERN_TIMEOUT    -2
#define EOS_KERN_FULL       -3
#define EOS_KERN_EMPTY      -4
#define EOS_KERN_INVALID    -5
#define EOS_KERN_NO_MEMORY  -6

/* ============================================================
 * Task Management
 * ============================================================ */

typedef enum {
    EOS_TASK_READY     = 0,
    EOS_TASK_RUNNING   = 1,
    EOS_TASK_BLOCKED   = 2,
    EOS_TASK_SUSPENDED = 3,
    EOS_TASK_DELETED   = 4,
} eos_task_state_t;

typedef void (*eos_task_func_t)(void *arg);

typedef struct {
    uint8_t id;
    const char *name;
    eos_task_state_t state;
    uint8_t priority;
    uint32_t stack_size;
    eos_task_func_t entry;
    void *arg;
    uint32_t *stack_base;
    uint32_t *stack_ptr;
    uint32_t wake_tick;
    uint32_t run_count;
} eos_task_t;

typedef uint8_t eos_task_handle_t;

int  eos_kernel_init(void);
void eos_kernel_start(void);
bool eos_kernel_is_running(void);

int eos_task_create(const char *name, eos_task_func_t entry, void *arg,
                     uint8_t priority, uint32_t stack_size);

int  eos_task_delete(eos_task_handle_t handle);
int  eos_task_suspend(eos_task_handle_t handle);
int  eos_task_resume(eos_task_handle_t handle);
void eos_task_yield(void);
void eos_task_delay_ms(uint32_t ms);
eos_task_handle_t eos_task_get_current(void);
eos_task_state_t  eos_task_get_state(eos_task_handle_t handle);
const char       *eos_task_get_name(eos_task_handle_t handle);

/* ============================================================
 * Mutex
 * ============================================================ */

typedef uint8_t eos_mutex_handle_t;

int eos_mutex_create(eos_mutex_handle_t *out);
int eos_mutex_lock(eos_mutex_handle_t handle, uint32_t timeout_ms);
int eos_mutex_unlock(eos_mutex_handle_t handle);
int eos_mutex_delete(eos_mutex_handle_t handle);

/* ============================================================
 * Semaphore
 * ============================================================ */

typedef uint8_t eos_sem_handle_t;

int eos_sem_create(eos_sem_handle_t *out, uint32_t initial, uint32_t max);
int eos_sem_wait(eos_sem_handle_t handle, uint32_t timeout_ms);
int eos_sem_post(eos_sem_handle_t handle);
int eos_sem_delete(eos_sem_handle_t handle);
uint32_t eos_sem_get_count(eos_sem_handle_t handle);

/* ============================================================
 * Message Queue
 * ============================================================ */

typedef uint8_t eos_queue_handle_t;

int eos_queue_create(eos_queue_handle_t *out, size_t item_size, uint32_t capacity);
int eos_queue_send(eos_queue_handle_t handle, const void *item, uint32_t timeout_ms);
int eos_queue_receive(eos_queue_handle_t handle, void *item, uint32_t timeout_ms);
int eos_queue_peek(eos_queue_handle_t handle, void *item);
int eos_queue_delete(eos_queue_handle_t handle);
uint32_t eos_queue_count(eos_queue_handle_t handle);
bool eos_queue_is_full(eos_queue_handle_t handle);
bool eos_queue_is_empty(eos_queue_handle_t handle);

/* ============================================================
 * Software Timer
 * ============================================================ */

typedef uint8_t eos_swtimer_handle_t;
typedef void (*eos_swtimer_callback_t)(eos_swtimer_handle_t handle, void *ctx);

int eos_swtimer_create(eos_swtimer_handle_t *out, const char *name,
                        uint32_t period_ms, bool auto_reload,
                        eos_swtimer_callback_t callback, void *ctx);
int eos_swtimer_start(eos_swtimer_handle_t handle);
int eos_swtimer_stop(eos_swtimer_handle_t handle);
int eos_swtimer_reset(eos_swtimer_handle_t handle);
int eos_swtimer_delete(eos_swtimer_handle_t handle);

/* ============================================================
 * Kernel Tick (called from ISR)
 * ============================================================ */

void eos_kernel_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_KERNEL_H */
