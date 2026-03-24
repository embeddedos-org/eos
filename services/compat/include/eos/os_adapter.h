// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file os_adapter.h
 * @brief EoS Pluggable OS Adapter — abstract OS operations
 *
 * Defines eos_os_adapter_t: a single monolithic struct of function pointers
 * that abstracts all OS primitives (tasks, sync, IPC, timers, memory).
 *
 * Adapters can be registered for: native EoS, FreeRTOS, Zephyr, Linux,
 * VxWorks, or any custom OS. The active adapter routes all OS calls.
 *
 * Pattern follows eos_hal_backend_t from eos/hal/include/eos/hal.h.
 *
 * Gated by EOS_ENABLE_COMPAT.
 */

#ifndef EOS_OS_ADAPTER_H
#define EOS_OS_ADAPTER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EOS_MAX_OS_ADAPTERS
#define EOS_MAX_OS_ADAPTERS  4
#endif

/* ============================================================
 * OS Adapter — single monolithic function pointer struct
 * ============================================================ */

typedef void (*eos_osa_task_func_t)(void *arg);
typedef void (*eos_osa_timer_cb_t)(uint8_t handle, void *ctx);

typedef struct eos_os_adapter {
    const char *name;

    /* ---- Task Management ---- */
    int  (*task_create)(const char *name, eos_osa_task_func_t entry, void *arg,
                        uint8_t priority, uint32_t stack_size);
    int  (*task_delete)(uint8_t handle);
    int  (*task_suspend)(uint8_t handle);
    int  (*task_resume)(uint8_t handle);
    void (*task_yield)(void);
    void (*task_delay_ms)(uint32_t ms);
    uint8_t (*task_get_current)(void);
    const char *(*task_get_name)(uint8_t handle);

    /* ---- Mutex ---- */
    int (*mutex_create)(uint8_t *out);
    int (*mutex_lock)(uint8_t handle, uint32_t timeout_ms);
    int (*mutex_unlock)(uint8_t handle);
    int (*mutex_delete)(uint8_t handle);

    /* ---- Semaphore ---- */
    int (*sem_create)(uint8_t *out, uint32_t initial, uint32_t max);
    int (*sem_wait)(uint8_t handle, uint32_t timeout_ms);
    int (*sem_post)(uint8_t handle);
    int (*sem_delete)(uint8_t handle);
    uint32_t (*sem_get_count)(uint8_t handle);

    /* ---- Message Queue ---- */
    int (*queue_create)(uint8_t *out, size_t item_size, uint32_t capacity);
    int (*queue_send)(uint8_t handle, const void *item, uint32_t timeout_ms);
    int (*queue_receive)(uint8_t handle, void *item, uint32_t timeout_ms);
    int (*queue_delete)(uint8_t handle);
    uint32_t (*queue_count)(uint8_t handle);
    bool (*queue_is_full)(uint8_t handle);
    bool (*queue_is_empty)(uint8_t handle);

    /* ---- Software Timer ---- */
    int (*timer_create)(uint8_t *out, const char *name, uint32_t period_ms,
                        bool auto_reload, eos_osa_timer_cb_t callback, void *ctx);
    int (*timer_start)(uint8_t handle);
    int (*timer_stop)(uint8_t handle);
    int (*timer_delete)(uint8_t handle);

    /* ---- System ---- */
    uint32_t (*get_tick_ms)(void);
    void (*delay_ms)(uint32_t ms);
    void (*irq_disable)(void);
    void (*irq_enable)(void);

    /* ---- Memory (optional — NULL = use malloc/free) ---- */
    void *(*mem_alloc)(size_t size);
    void  (*mem_free)(void *ptr);

} eos_os_adapter_t;

/* ============================================================
 * Adapter Registry
 * ============================================================ */

/**
 * Register an OS adapter. Up to EOS_MAX_OS_ADAPTERS can be registered.
 * The first adapter registered becomes the active adapter automatically.
 * @return 0 on success, -1 if registry full.
 */
int eos_os_adapter_register(const eos_os_adapter_t *adapter);

/**
 * Set the active adapter by name.
 * @return 0 on success, -1 if not found.
 */
int eos_os_adapter_set_active(const char *name);

/**
 * Get the currently active adapter.
 * @return Pointer to active adapter, or NULL if none registered.
 */
const eos_os_adapter_t *eos_os_adapter_get_active(void);

/**
 * Initialize the OS adapter subsystem and register the default
 * native EoS adapter.
 */
void eos_os_adapter_init(void);

/* ============================================================
 * Convenience Wrappers (dereference active adapter)
 * ============================================================ */

int  eos_osa_task_create(const char *name, eos_osa_task_func_t entry, void *arg,
                         uint8_t priority, uint32_t stack_size);
int  eos_osa_task_delete(uint8_t handle);
int  eos_osa_task_suspend(uint8_t handle);
int  eos_osa_task_resume(uint8_t handle);
void eos_osa_task_yield(void);
void eos_osa_task_delay_ms(uint32_t ms);

int  eos_osa_mutex_create(uint8_t *out);
int  eos_osa_mutex_lock(uint8_t handle, uint32_t timeout_ms);
int  eos_osa_mutex_unlock(uint8_t handle);
int  eos_osa_mutex_delete(uint8_t handle);

int  eos_osa_sem_create(uint8_t *out, uint32_t initial, uint32_t max);
int  eos_osa_sem_wait(uint8_t handle, uint32_t timeout_ms);
int  eos_osa_sem_post(uint8_t handle);
int  eos_osa_sem_delete(uint8_t handle);

int  eos_osa_queue_create(uint8_t *out, size_t item_size, uint32_t capacity);
int  eos_osa_queue_send(uint8_t handle, const void *item, uint32_t timeout_ms);
int  eos_osa_queue_receive(uint8_t handle, void *item, uint32_t timeout_ms);
int  eos_osa_queue_delete(uint8_t handle);

int  eos_osa_timer_create(uint8_t *out, const char *name, uint32_t period_ms,
                          bool auto_reload, eos_osa_timer_cb_t cb, void *ctx);
int  eos_osa_timer_start(uint8_t handle);
int  eos_osa_timer_stop(uint8_t handle);
int  eos_osa_timer_delete(uint8_t handle);

uint32_t eos_osa_get_tick_ms(void);
void eos_osa_delay_ms(uint32_t ms);
void eos_osa_irq_disable(void);
void eos_osa_irq_enable(void);

void *eos_osa_mem_alloc(size_t size);
void  eos_osa_mem_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* EOS_OS_ADAPTER_H */
