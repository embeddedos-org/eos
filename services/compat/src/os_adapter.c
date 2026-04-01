// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file os_adapter.c
 * @brief OS adapter registry and convenience wrappers
 */

#include "eos/os_adapter.h"
#include <string.h>
#include <stdlib.h>

static const eos_os_adapter_t *g_adapters[EOS_MAX_OS_ADAPTERS];
static int g_adapter_count = 0;
static const eos_os_adapter_t *g_active = NULL;

extern const eos_os_adapter_t eos_native_adapter;

void eos_os_adapter_init(void)
{
    eos_os_adapter_register(&eos_native_adapter);
}

int eos_os_adapter_register(const eos_os_adapter_t *adapter)
{
    if (!adapter || g_adapter_count >= EOS_MAX_OS_ADAPTERS)
        return -1;
    g_adapters[g_adapter_count++] = adapter;
    if (!g_active)
        g_active = adapter;
    return 0;
}

int eos_os_adapter_set_active(const char *name)
{
    if (!name) return -1;
    for (int i = 0; i < g_adapter_count; i++) {
        if (g_adapters[i]->name && strcmp(g_adapters[i]->name, name) == 0) {
            g_active = g_adapters[i];
            return 0;
        }
    }
    return -1;
}

const eos_os_adapter_t *eos_os_adapter_get_active(void)
{
    return g_active;
}

/* ---- Convenience wrappers ---- */

int eos_osa_task_create(const char *name, eos_osa_task_func_t entry, void *arg,
                        uint8_t priority, uint32_t stack_size)
{
    if (!g_active || !g_active->task_create) return -1;
    return g_active->task_create(name, entry, arg, priority, stack_size);
}

int eos_osa_task_delete(uint8_t handle)
{
    if (!g_active || !g_active->task_delete) return -1;
    return g_active->task_delete(handle);
}

int eos_osa_task_suspend(uint8_t handle)
{
    if (!g_active || !g_active->task_suspend) return -1;
    return g_active->task_suspend(handle);
}

int eos_osa_task_resume(uint8_t handle)
{
    if (!g_active || !g_active->task_resume) return -1;
    return g_active->task_resume(handle);
}

void eos_osa_task_yield(void)
{
    if (g_active && g_active->task_yield) g_active->task_yield();
}

void eos_osa_task_delay_ms(uint32_t ms)
{
    if (g_active && g_active->task_delay_ms) g_active->task_delay_ms(ms);
}

int eos_osa_mutex_create(uint8_t *out)
{
    if (!g_active || !g_active->mutex_create) return -1;
    return g_active->mutex_create(out);
}

int eos_osa_mutex_lock(uint8_t handle, uint32_t timeout_ms)
{
    if (!g_active || !g_active->mutex_lock) return -1;
    return g_active->mutex_lock(handle, timeout_ms);
}

int eos_osa_mutex_unlock(uint8_t handle)
{
    if (!g_active || !g_active->mutex_unlock) return -1;
    return g_active->mutex_unlock(handle);
}

int eos_osa_mutex_delete(uint8_t handle)
{
    if (!g_active || !g_active->mutex_delete) return -1;
    return g_active->mutex_delete(handle);
}

int eos_osa_sem_create(uint8_t *out, uint32_t initial, uint32_t max)
{
    if (!g_active || !g_active->sem_create) return -1;
    return g_active->sem_create(out, initial, max);
}

int eos_osa_sem_wait(uint8_t handle, uint32_t timeout_ms)
{
    if (!g_active || !g_active->sem_wait) return -1;
    return g_active->sem_wait(handle, timeout_ms);
}

int eos_osa_sem_post(uint8_t handle)
{
    if (!g_active || !g_active->sem_post) return -1;
    return g_active->sem_post(handle);
}

int eos_osa_sem_delete(uint8_t handle)
{
    if (!g_active || !g_active->sem_delete) return -1;
    return g_active->sem_delete(handle);
}

int eos_osa_queue_create(uint8_t *out, size_t item_size, uint32_t capacity)
{
    if (!g_active || !g_active->queue_create) return -1;
    return g_active->queue_create(out, item_size, capacity);
}

int eos_osa_queue_send(uint8_t handle, const void *item, uint32_t timeout_ms)
{
    if (!g_active || !g_active->queue_send) return -1;
    return g_active->queue_send(handle, item, timeout_ms);
}

int eos_osa_queue_receive(uint8_t handle, void *item, uint32_t timeout_ms)
{
    if (!g_active || !g_active->queue_receive) return -1;
    return g_active->queue_receive(handle, item, timeout_ms);
}

int eos_osa_queue_delete(uint8_t handle)
{
    if (!g_active || !g_active->queue_delete) return -1;
    return g_active->queue_delete(handle);
}

int eos_osa_timer_create(uint8_t *out, const char *name, uint32_t period_ms,
                         bool auto_reload, eos_osa_timer_cb_t cb, void *ctx)
{
    if (!g_active || !g_active->timer_create) return -1;
    return g_active->timer_create(out, name, period_ms, auto_reload, cb, ctx);
}

int eos_osa_timer_start(uint8_t handle)
{
    if (!g_active || !g_active->timer_start) return -1;
    return g_active->timer_start(handle);
}

int eos_osa_timer_stop(uint8_t handle)
{
    if (!g_active || !g_active->timer_stop) return -1;
    return g_active->timer_stop(handle);
}

int eos_osa_timer_delete(uint8_t handle)
{
    if (!g_active || !g_active->timer_delete) return -1;
    return g_active->timer_delete(handle);
}

uint32_t eos_osa_get_tick_ms(void)
{
    if (g_active && g_active->get_tick_ms) return g_active->get_tick_ms();
    return 0;
}

void eos_osa_delay_ms(uint32_t ms)
{
    if (g_active && g_active->delay_ms) g_active->delay_ms(ms);
}

void eos_osa_irq_disable(void)
{
    if (g_active && g_active->irq_disable) g_active->irq_disable();
}

void eos_osa_irq_enable(void)
{
    if (g_active && g_active->irq_enable) g_active->irq_enable();
}

void *eos_osa_mem_alloc(size_t size)
{
    if (g_active && g_active->mem_alloc) return g_active->mem_alloc(size);
    return malloc(size);
}

void eos_osa_mem_free(void *ptr)
{
    if (g_active && g_active->mem_free) { g_active->mem_free(ptr); return; }
    free(ptr);
}
