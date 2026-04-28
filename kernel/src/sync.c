// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file sync.c
 * @brief Mutex and semaphore with timeout blocking and priority inheritance
 */

#include "eos/kernel.h"
#include "eos/kernel_internal.h"
#include "eos/arch.h"
#include <string.h>

/* ============================================================
 * Mutex — with priority inheritance and timeout blocking
 * ============================================================ */

#define MTX_MAX_WAITERS 8

typedef struct {
    uint8_t in_use;
    uint8_t locked;
    uint8_t owner;
    uint8_t original_prio;
    uint8_t rec_count;
    uint8_t waiters[MTX_MAX_WAITERS];
    uint8_t waiter_count;
} mtx_t;

typedef struct {
    uint8_t in_use;
    int32_t count;
    int32_t max_count;
    uint8_t waiters[MTX_MAX_WAITERS];
    uint8_t waiter_count;
} sem_t;

static mtx_t g_mtx[EOS_MAX_MUTEXES];
static sem_t g_sem[EOS_MAX_SEMAPHORES];

int eos_mutex_create(eos_mutex_handle_t *out)
{
    if (!out) return EOS_KERN_INVALID;
    uint32_t crit = eos_port_enter_critical();
    for (int i = 0; i < EOS_MAX_MUTEXES; i++) {
        if (!g_mtx[i].in_use) {
            memset(&g_mtx[i], 0, sizeof(mtx_t));
            g_mtx[i].in_use = 1;
            g_mtx[i].owner = 0xFF;
            *out = (uint8_t)i;
            eos_port_exit_critical(crit);
            return EOS_KERN_OK;
        }
    }
    eos_port_exit_critical(crit);
    return EOS_KERN_NO_MEMORY;
}

int eos_mutex_lock(eos_mutex_handle_t h, uint32_t timeout_ms)
{
    if (h >= EOS_MAX_MUTEXES || !g_mtx[h].in_use) return EOS_KERN_INVALID;

    uint8_t caller = (uint8_t)eos_task_get_current();
    uint32_t crit = eos_port_enter_critical();

    /* Free — acquire immediately */
    if (!g_mtx[h].locked) {
        g_mtx[h].locked = 1;
        g_mtx[h].owner = caller;
        g_mtx[h].original_prio = eos_task_get_priority_internal(caller);
        g_mtx[h].rec_count = 1;
        eos_port_exit_critical(crit);
        return EOS_KERN_OK;
    }

    /* Recursive lock by owner */
    if (g_mtx[h].owner == caller) {
        g_mtx[h].rec_count++;
        eos_port_exit_critical(crit);
        return EOS_KERN_OK;
    }

    /* No-wait: fail immediately */
    if (timeout_ms == EOS_NO_WAIT) {
        eos_port_exit_critical(crit);
        return EOS_KERN_TIMEOUT;
    }

    /* Priority inheritance: boost owner to caller's priority if higher */
    uint8_t caller_prio = eos_task_get_priority_internal(caller);
    uint8_t owner_prio = eos_task_get_priority_internal(g_mtx[h].owner);
    if (caller_prio < owner_prio) {
        eos_task_set_priority_internal(g_mtx[h].owner, caller_prio);
    }

    /* Add caller to wait queue */
    if (g_mtx[h].waiter_count < MTX_MAX_WAITERS) {
        g_mtx[h].waiters[g_mtx[h].waiter_count++] = caller;
    }

    /* Block the calling task */
    eos_task_block_with_timeout(caller, timeout_ms);
    eos_port_exit_critical(crit);
    eos_port_yield();

    /* Resumed — check if we got the mutex */
    crit = eos_port_enter_critical();
    if (g_mtx[h].owner == caller) {
        eos_port_exit_critical(crit);
        return EOS_KERN_OK;
    }

    /* Timeout — remove from wait queue */
    for (int i = 0; i < g_mtx[h].waiter_count; i++) {
        if (g_mtx[h].waiters[i] == caller) {
            for (int j = i; j < g_mtx[h].waiter_count - 1; j++)
                g_mtx[h].waiters[j] = g_mtx[h].waiters[j + 1];
            g_mtx[h].waiter_count--;
            break;
        }
    }
    eos_port_exit_critical(crit);
    return EOS_KERN_TIMEOUT;
}

int eos_mutex_unlock(eos_mutex_handle_t h)
{
    if (h >= EOS_MAX_MUTEXES || !g_mtx[h].in_use || !g_mtx[h].locked)
        return EOS_KERN_INVALID;

    uint8_t caller = (uint8_t)eos_task_get_current();
    if (g_mtx[h].owner != caller) return EOS_KERN_INVALID;

    uint32_t crit = eos_port_enter_critical();

    if (--g_mtx[h].rec_count > 0) {
        eos_port_exit_critical(crit);
        return EOS_KERN_OK;
    }

    /* Restore owner's original priority */
    eos_task_set_priority_internal(caller, g_mtx[h].original_prio);

    /* Grant to highest-priority waiter */
    if (g_mtx[h].waiter_count > 0) {
        int best = 0;
        uint8_t best_prio = 255;
        for (int i = 0; i < g_mtx[h].waiter_count; i++) {
            uint8_t wp = eos_task_get_priority_internal(g_mtx[h].waiters[i]);
            if (wp < best_prio) { best_prio = wp; best = i; }
        }
        uint8_t next_owner = g_mtx[h].waiters[best];
        for (int j = best; j < g_mtx[h].waiter_count - 1; j++)
            g_mtx[h].waiters[j] = g_mtx[h].waiters[j + 1];
        g_mtx[h].waiter_count--;

        g_mtx[h].owner = next_owner;
        g_mtx[h].original_prio = eos_task_get_priority_internal(next_owner);
        g_mtx[h].rec_count = 1;
        eos_task_unblock(next_owner);
    } else {
        g_mtx[h].locked = 0;
        g_mtx[h].owner = 0xFF;
    }

    eos_port_exit_critical(crit);
    return EOS_KERN_OK;
}

int eos_mutex_delete(eos_mutex_handle_t h)
{
    if (h >= EOS_MAX_MUTEXES || !g_mtx[h].in_use) return EOS_KERN_INVALID;
    uint32_t crit = eos_port_enter_critical();
    for (int i = 0; i < g_mtx[h].waiter_count; i++)
        eos_task_unblock(g_mtx[h].waiters[i]);
    g_mtx[h].in_use = 0;
    eos_port_exit_critical(crit);
    return EOS_KERN_OK;
}

/* ============================================================
 * Semaphore — with timeout blocking
 * ============================================================ */

int eos_sem_create(eos_sem_handle_t *out, uint32_t initial, uint32_t max)
{
    if (!out || max == 0) return EOS_KERN_INVALID;
    uint32_t crit = eos_port_enter_critical();
    for (int i = 0; i < EOS_MAX_SEMAPHORES; i++) {
        if (!g_sem[i].in_use) {
            memset(&g_sem[i], 0, sizeof(sem_t));
            g_sem[i].in_use = 1;
            g_sem[i].count = (int32_t)initial;
            g_sem[i].max_count = (int32_t)max;
            *out = (uint8_t)i;
            eos_port_exit_critical(crit);
            return EOS_KERN_OK;
        }
    }
    eos_port_exit_critical(crit);
    return EOS_KERN_NO_MEMORY;
}

int eos_sem_wait(eos_sem_handle_t h, uint32_t timeout_ms)
{
    if (h >= EOS_MAX_SEMAPHORES || !g_sem[h].in_use) return EOS_KERN_INVALID;

    uint32_t crit = eos_port_enter_critical();
    if (g_sem[h].count > 0) {
        g_sem[h].count--;
        eos_port_exit_critical(crit);
        return EOS_KERN_OK;
    }

    if (timeout_ms == EOS_NO_WAIT) {
        eos_port_exit_critical(crit);
        return EOS_KERN_TIMEOUT;
    }

    uint8_t caller = (uint8_t)eos_task_get_current();
    if (g_sem[h].waiter_count < MTX_MAX_WAITERS)
        g_sem[h].waiters[g_sem[h].waiter_count++] = caller;
    eos_task_block_with_timeout(caller, timeout_ms);
    eos_port_exit_critical(crit);
    eos_port_yield();

    /* Re-check */
    crit = eos_port_enter_critical();
    if (g_sem[h].count > 0) {
        g_sem[h].count--;
        /* Remove from wait queue if still there */
        for (int i = 0; i < g_sem[h].waiter_count; i++) {
            if (g_sem[h].waiters[i] == caller) {
                for (int j = i; j < g_sem[h].waiter_count - 1; j++)
                    g_sem[h].waiters[j] = g_sem[h].waiters[j + 1];
                g_sem[h].waiter_count--;
                break;
            }
        }
        eos_port_exit_critical(crit);
        return EOS_KERN_OK;
    }
    /* Timeout — remove from queue */
    for (int i = 0; i < g_sem[h].waiter_count; i++) {
        if (g_sem[h].waiters[i] == caller) {
            for (int j = i; j < g_sem[h].waiter_count - 1; j++)
                g_sem[h].waiters[j] = g_sem[h].waiters[j + 1];
            g_sem[h].waiter_count--;
            break;
        }
    }
    eos_port_exit_critical(crit);
    return EOS_KERN_TIMEOUT;
}

int eos_sem_post(eos_sem_handle_t h)
{
    if (h >= EOS_MAX_SEMAPHORES || !g_sem[h].in_use) return EOS_KERN_INVALID;

    uint32_t crit = eos_port_enter_critical();
    if (g_sem[h].count < g_sem[h].max_count) {
        g_sem[h].count++;
        if (g_sem[h].waiter_count > 0) {
            /* Wake highest-priority waiter */
            int best = 0;
            uint8_t best_prio = 255;
            for (int i = 0; i < g_sem[h].waiter_count; i++) {
                uint8_t wp = eos_task_get_priority_internal(g_sem[h].waiters[i]);
                if (wp < best_prio) { best_prio = wp; best = i; }
            }
            uint8_t waker = g_sem[h].waiters[best];
            for (int j = best; j < g_sem[h].waiter_count - 1; j++)
                g_sem[h].waiters[j] = g_sem[h].waiters[j + 1];
            g_sem[h].waiter_count--;
            eos_task_unblock(waker);
        }
        eos_port_exit_critical(crit);
        return EOS_KERN_OK;
    }
    eos_port_exit_critical(crit);
    return EOS_KERN_FULL;
}

int eos_sem_delete(eos_sem_handle_t h)
{
    if (h >= EOS_MAX_SEMAPHORES || !g_sem[h].in_use) return EOS_KERN_INVALID;
    uint32_t crit = eos_port_enter_critical();
    for (int i = 0; i < g_sem[h].waiter_count; i++)
        eos_task_unblock(g_sem[h].waiters[i]);
    g_sem[h].in_use = 0;
    eos_port_exit_critical(crit);
    return EOS_KERN_OK;
}

uint32_t eos_sem_get_count(eos_sem_handle_t h)
{
    if (h >= EOS_MAX_SEMAPHORES || !g_sem[h].in_use) return 0;
    return (uint32_t)g_sem[h].count;
}
