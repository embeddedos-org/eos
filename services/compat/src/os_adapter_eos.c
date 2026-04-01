// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file os_adapter_eos.c
 * @brief Native EoS OS adapter — maps directly to eos_* kernel API
 *
 * Default adapter that routes all OS operations through the native
 * EoS kernel primitives defined in eos/kernel.h and eos/hal.h.
 */

#include "eos/os_adapter.h"
#include "eos/kernel.h"
#include "eos/hal.h"

static int eos_native_task_create(const char *name, eos_osa_task_func_t entry,
                                   void *arg, uint8_t priority, uint32_t stack_size)
{
    return eos_task_create(name, (eos_task_func_t)entry, arg, priority, stack_size);
}

static int eos_native_task_delete(uint8_t h) { return eos_task_delete(h); }
static int eos_native_task_suspend(uint8_t h) { return eos_task_suspend(h); }
static int eos_native_task_resume(uint8_t h) { return eos_task_resume(h); }
static void eos_native_task_yield(void) { eos_task_yield(); }
static void eos_native_task_delay(uint32_t ms) { eos_task_delay_ms(ms); }
static uint8_t eos_native_task_current(void) { return eos_task_get_current(); }
static const char *eos_native_task_name(uint8_t h) { return eos_task_get_name(h); }

static int eos_native_mutex_create(uint8_t *out) { return eos_mutex_create(out); }
static int eos_native_mutex_lock(uint8_t h, uint32_t t) { return eos_mutex_lock(h, t); }
static int eos_native_mutex_unlock(uint8_t h) { return eos_mutex_unlock(h); }
static int eos_native_mutex_delete(uint8_t h) { return eos_mutex_delete(h); }

static int eos_native_sem_create(uint8_t *out, uint32_t init, uint32_t max)
{
    return eos_sem_create(out, init, max);
}
static int eos_native_sem_wait(uint8_t h, uint32_t t) { return eos_sem_wait(h, t); }
static int eos_native_sem_post(uint8_t h) { return eos_sem_post(h); }
static int eos_native_sem_delete(uint8_t h) { return eos_sem_delete(h); }
static uint32_t eos_native_sem_count(uint8_t h) { return eos_sem_get_count(h); }

static int eos_native_queue_create(uint8_t *out, size_t sz, uint32_t cap)
{
    return eos_queue_create(out, sz, cap);
}
static int eos_native_queue_send(uint8_t h, const void *item, uint32_t t)
{
    return eos_queue_send(h, item, t);
}
static int eos_native_queue_recv(uint8_t h, void *item, uint32_t t)
{
    return eos_queue_receive(h, item, t);
}
static int eos_native_queue_delete(uint8_t h) { return eos_queue_delete(h); }
static uint32_t eos_native_queue_count(uint8_t h) { return eos_queue_count(h); }
static bool eos_native_queue_full(uint8_t h) { return eos_queue_is_full(h); }
static bool eos_native_queue_empty(uint8_t h) { return eos_queue_is_empty(h); }

static int eos_native_timer_create(uint8_t *out, const char *name, uint32_t period,
                                    bool reload, eos_osa_timer_cb_t cb, void *ctx)
{
    return eos_swtimer_create(out, name, period, reload,
                              (eos_swtimer_callback_t)cb, ctx);
}
static int eos_native_timer_start(uint8_t h) { return eos_swtimer_start(h); }
static int eos_native_timer_stop(uint8_t h) { return eos_swtimer_stop(h); }
static int eos_native_timer_delete(uint8_t h) { return eos_swtimer_delete(h); }

static uint32_t eos_native_get_tick(void) { return eos_get_tick_ms(); }
static void eos_native_delay(uint32_t ms) { eos_delay_ms(ms); }
static void eos_native_irq_off(void) { eos_irq_disable(); }
static void eos_native_irq_on(void) { eos_irq_enable(); }

const eos_os_adapter_t eos_native_adapter = {
    .name            = "eos",
    .task_create     = eos_native_task_create,
    .task_delete     = eos_native_task_delete,
    .task_suspend    = eos_native_task_suspend,
    .task_resume     = eos_native_task_resume,
    .task_yield      = eos_native_task_yield,
    .task_delay_ms   = eos_native_task_delay,
    .task_get_current = eos_native_task_current,
    .task_get_name   = eos_native_task_name,
    .mutex_create    = eos_native_mutex_create,
    .mutex_lock      = eos_native_mutex_lock,
    .mutex_unlock    = eos_native_mutex_unlock,
    .mutex_delete    = eos_native_mutex_delete,
    .sem_create      = eos_native_sem_create,
    .sem_wait        = eos_native_sem_wait,
    .sem_post        = eos_native_sem_post,
    .sem_delete      = eos_native_sem_delete,
    .sem_get_count   = eos_native_sem_count,
    .queue_create    = eos_native_queue_create,
    .queue_send      = eos_native_queue_send,
    .queue_receive   = eos_native_queue_recv,
    .queue_delete    = eos_native_queue_delete,
    .queue_count     = eos_native_queue_count,
    .queue_is_full   = eos_native_queue_full,
    .queue_is_empty  = eos_native_queue_empty,
    .timer_create    = eos_native_timer_create,
    .timer_start     = eos_native_timer_start,
    .timer_stop      = eos_native_timer_stop,
    .timer_delete    = eos_native_timer_delete,
    .get_tick_ms     = eos_native_get_tick,
    .delay_ms        = eos_native_delay,
    .irq_disable     = eos_native_irq_off,
    .irq_enable      = eos_native_irq_on,
    .mem_alloc       = NULL,
    .mem_free        = NULL,
};
