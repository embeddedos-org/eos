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
#include <stdint.h>

/* ============================================================
 * Mutex — with priority inheritance and timeout blocking
 * ============================================================ */

#define MTX_MAX_WAITERS 8

#define MTX_NO_OWNER 0xFFu

/* Bounds the walk along a chain of blocked tasks. A lock-order inversion
 * makes that chain cyclic, so the bound is required for termination. */
#define PI_MAX_CHAIN_DEPTH 8

typedef struct {
    uint8_t in_use;
    uint8_t locked;
    uint8_t owner;
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

/* ============================================================
 * Priority inheritance
 *
 * A task holding mutexes runs at
 *
 *     effective = min( base_priority,
 *                      priority of every task waiting on every mutex it owns )
 *
 * with lower meaning more urgent. Recomputing from that invariant on every
 * change, instead of saving and restoring a value around each lock, keeps the
 * nested cases correct and lets a boost propagate along the blocking chain.
 * ============================================================ */

/* Mutex each task is blocked on, as handle + 1 so that 0 means "none". */
static uint8_t g_blocked_on[EOS_MAX_TASKS];

static inline int task_valid(uint8_t t)
{
    return t < EOS_MAX_TASKS;
}

/** @return non-zero if the effective priority changed. */
static int pi_recompute(uint8_t task)
{
    if (!task_valid(task)) return 0;

    uint8_t eff = g_tasks[task].base_priority;

    for (int i = 0; i < EOS_MAX_MUTEXES; i++) {
        const mtx_t *m = &g_mtx[i];
        if (!m->in_use || !m->locked || m->owner != task) continue;
        for (int w = 0; w < m->waiter_count; w++) {
            uint8_t waiter = m->waiters[w];
            if (!task_valid(waiter)) continue;
            if (g_tasks[waiter].priority < eff)
                eff = g_tasks[waiter].priority;
        }
    }

    if (g_tasks[task].priority == eff) return 0;
    g_tasks[task].priority = eff;
    return 1;
}

static void pi_propagate(uint8_t task)
{
    for (int depth = 0; depth < PI_MAX_CHAIN_DEPTH; depth++) {
        if (!task_valid(task)) return;
        if (!pi_recompute(task) && depth > 0) return;

        uint8_t enc = g_blocked_on[task];
        if (enc == 0) return;

        uint8_t m = (uint8_t)(enc - 1u);
        if (m >= EOS_MAX_MUTEXES) return;
        if (!g_mtx[m].in_use || !g_mtx[m].locked) return;
        if (g_mtx[m].owner == task) return;

        task = g_mtx[m].owner;
    }
}

static int mtx_remove_waiter(mtx_t *m, uint8_t task)
{
    for (int i = 0; i < m->waiter_count; i++) {
        if (m->waiters[i] != task) continue;
        for (int j = i; j < m->waiter_count - 1; j++)
            m->waiters[j] = m->waiters[j + 1];
        m->waiter_count--;
        return 1;
    }
    return 0;
}

int eos_mutex_create(eos_mutex_handle_t *out)
{
    if (!out) return EOS_KERN_INVALID;
    uint32_t crit = eos_port_enter_critical();
    for (int i = 0; i < EOS_MAX_MUTEXES; i++) {
        if (!g_mtx[i].in_use) {
            memset(&g_mtx[i], 0, sizeof(mtx_t));
            g_mtx[i].in_use = 1;
            g_mtx[i].owner = MTX_NO_OWNER;
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
        g_mtx[h].rec_count = 1;
        eos_port_exit_critical(crit);
        return EOS_KERN_OK;
    }

    /* Recursive lock by owner */
    if (g_mtx[h].owner == caller) {
        if (g_mtx[h].rec_count == UINT8_MAX) {
            eos_port_exit_critical(crit);
            return EOS_KERN_FULL;
        }
        g_mtx[h].rec_count++;
        eos_port_exit_critical(crit);
        return EOS_KERN_OK;
    }

    /* No-wait: fail immediately */
    if (timeout_ms == EOS_NO_WAIT) {
        eos_port_exit_critical(crit);
        return EOS_KERN_TIMEOUT;
    }

    /* Refuse rather than block: a task that is not enqueued is never granted
     * the mutex, so with EOS_WAIT_FOREVER it would sleep forever. */
    if (g_mtx[h].waiter_count >= MTX_MAX_WAITERS) {
        eos_port_exit_critical(crit);
        return EOS_KERN_NO_MEMORY;
    }

    g_mtx[h].waiters[g_mtx[h].waiter_count++] = caller;
    if (task_valid(caller))
        g_blocked_on[caller] = (uint8_t)(h + 1u);
    pi_propagate(g_mtx[h].owner);

    /* Block the calling task */
    eos_task_block_with_timeout(caller, timeout_ms);
    eos_port_exit_critical(crit);
    eos_port_yield();

    /* Resumed — check if we got the mutex */
    crit = eos_port_enter_critical();
    if (task_valid(caller))
        g_blocked_on[caller] = 0;

    if (g_mtx[h].owner == caller) {
        eos_port_exit_critical(crit);
        return EOS_KERN_OK;
    }

    /* Timeout — leave the queue and drop the boost we were causing. */
    uint8_t owner = g_mtx[h].owner;
    mtx_remove_waiter(&g_mtx[h], caller);
    pi_propagate(owner);

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

    /* Grant to highest-priority waiter */
    uint8_t next_owner = MTX_NO_OWNER;
    if (g_mtx[h].waiter_count > 0) {
        int best = 0;
        uint8_t best_prio = 255;
        for (int i = 0; i < g_mtx[h].waiter_count; i++) {
            uint8_t wp = eos_task_get_priority_internal(g_mtx[h].waiters[i]);
            if (wp < best_prio) { best_prio = wp; best = i; }
        }
        next_owner = g_mtx[h].waiters[best];
        for (int j = best; j < g_mtx[h].waiter_count - 1; j++)
            g_mtx[h].waiters[j] = g_mtx[h].waiters[j + 1];
        g_mtx[h].waiter_count--;

        g_mtx[h].owner = next_owner;
        g_mtx[h].rec_count = 1;
        if (task_valid(next_owner))
            g_blocked_on[next_owner] = 0;
        eos_task_unblock(next_owner);
    } else {
        g_mtx[h].locked = 0;
        g_mtx[h].owner = MTX_NO_OWNER;
    }

    /* Recompute after ownership has moved, so the releasing task keeps any
     * boost its other mutexes still justify. */
    pi_propagate(caller);
    if (next_owner != MTX_NO_OWNER)
        pi_propagate(next_owner);

    eos_port_exit_critical(crit);
    return EOS_KERN_OK;
}

int eos_mutex_delete(eos_mutex_handle_t h)
{
    if (h >= EOS_MAX_MUTEXES || !g_mtx[h].in_use) return EOS_KERN_INVALID;
    uint32_t crit = eos_port_enter_critical();

    uint8_t owner = g_mtx[h].owner;
    uint8_t waiters[MTX_MAX_WAITERS];
    uint8_t waiter_count = g_mtx[h].waiter_count;
    for (int i = 0; i < waiter_count; i++) {
        waiters[i] = g_mtx[h].waiters[i];
        if (task_valid(waiters[i]))
            g_blocked_on[waiters[i]] = 0;
        eos_task_unblock(waiters[i]);
    }

    /* Retire the mutex before recomputing so it no longer contributes. */
    memset(&g_mtx[h], 0, sizeof(mtx_t));
    g_mtx[h].owner = MTX_NO_OWNER;

    pi_propagate(owner);
    for (int i = 0; i < waiter_count; i++)
        pi_propagate(waiters[i]);

    eos_port_exit_critical(crit);
    return EOS_KERN_OK;
}

/* ============================================================
 * Semaphore — with timeout blocking
 * ============================================================ */

int eos_sem_create(eos_sem_handle_t *out, uint32_t initial, uint32_t max)
{
    if (!out || max == 0 || initial > max) return EOS_KERN_INVALID;
    if (max > (uint32_t)INT32_MAX) return EOS_KERN_INVALID;
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

    /* Same lost-wakeup hazard as the mutex wait queue. */
    if (g_sem[h].waiter_count >= MTX_MAX_WAITERS) {
        eos_port_exit_critical(crit);
        return EOS_KERN_NO_MEMORY;
    }

    uint8_t caller = (uint8_t)eos_task_get_current();
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
