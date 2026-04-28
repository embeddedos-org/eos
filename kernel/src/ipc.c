// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file ipc.c
 * @brief Message queues with timeout-capable blocking
 */

#include "eos/kernel.h"
#include "eos/kernel_internal.h"
#include "eos/arch.h"
#include <string.h>

#define Q_MAX_WAITERS 8

typedef struct {
    uint8_t  in_use;
    uint8_t  *buf;
    uint8_t  _storage[16 * 64];
    size_t   item_size;
    uint32_t capacity, head, tail, count;
    /* Send waiters (blocked because queue is full) */
    uint8_t  send_waiters[Q_MAX_WAITERS];
    uint8_t  send_waiter_count;
    /* Receive waiters (blocked because queue is empty) */
    uint8_t  recv_waiters[Q_MAX_WAITERS];
    uint8_t  recv_waiter_count;
} queue_t;

static queue_t g_q[EOS_MAX_QUEUES];

int eos_queue_create(eos_queue_handle_t *out, size_t item_size, uint32_t capacity)
{
    if (!out || item_size == 0 || capacity == 0) return EOS_KERN_INVALID;
    if (item_size * capacity > sizeof(g_q[0]._storage)) return EOS_KERN_NO_MEMORY;

    uint32_t crit = eos_port_enter_critical();
    for (int i = 0; i < EOS_MAX_QUEUES; i++) {
        if (!g_q[i].in_use) {
            memset(&g_q[i], 0, sizeof(queue_t));
            g_q[i].in_use = 1;
            g_q[i].item_size = item_size;
            g_q[i].capacity = capacity;
            g_q[i].buf = g_q[i]._storage;
            *out = (uint8_t)i;
            eos_port_exit_critical(crit);
            return EOS_KERN_OK;
        }
    }
    eos_port_exit_critical(crit);
    return EOS_KERN_NO_MEMORY;
}

int eos_queue_send(eos_queue_handle_t h, const void *item, uint32_t timeout_ms)
{
    if (h >= EOS_MAX_QUEUES || !g_q[h].in_use || !item) return EOS_KERN_INVALID;

    uint32_t crit = eos_port_enter_critical();

    /* Try to enqueue */
    if (g_q[h].count < g_q[h].capacity) {
        memcpy(g_q[h].buf + g_q[h].head * g_q[h].item_size, item, g_q[h].item_size);
        g_q[h].head = (g_q[h].head + 1) % g_q[h].capacity;
        g_q[h].count++;

        /* Wake a receiver if any are waiting */
        if (g_q[h].recv_waiter_count > 0) {
            uint8_t waker = g_q[h].recv_waiters[0];
            for (int j = 0; j < g_q[h].recv_waiter_count - 1; j++)
                g_q[h].recv_waiters[j] = g_q[h].recv_waiters[j + 1];
            g_q[h].recv_waiter_count--;
            eos_task_unblock(waker);
        }

        eos_port_exit_critical(crit);
        return EOS_KERN_OK;
    }

    /* Queue full — block or fail */
    if (timeout_ms == EOS_NO_WAIT) {
        eos_port_exit_critical(crit);
        return EOS_KERN_FULL;
    }

    uint8_t caller = (uint8_t)eos_task_get_current();
    if (g_q[h].send_waiter_count < Q_MAX_WAITERS) {
        g_q[h].send_waiters[g_q[h].send_waiter_count++] = caller;
    }
    eos_task_block_with_timeout(caller, timeout_ms);
    eos_port_exit_critical(crit);
    eos_port_yield();

    /* Retry after waking */
    crit = eos_port_enter_critical();
    if (g_q[h].count < g_q[h].capacity) {
        memcpy(g_q[h].buf + g_q[h].head * g_q[h].item_size, item, g_q[h].item_size);
        g_q[h].head = (g_q[h].head + 1) % g_q[h].capacity;
        g_q[h].count++;
        eos_port_exit_critical(crit);
        return EOS_KERN_OK;
    }

    /* Still full — timeout */
    for (int i = 0; i < g_q[h].send_waiter_count; i++) {
        if (g_q[h].send_waiters[i] == caller) {
            for (int j = i; j < g_q[h].send_waiter_count - 1; j++)
                g_q[h].send_waiters[j] = g_q[h].send_waiters[j + 1];
            g_q[h].send_waiter_count--;
            break;
        }
    }
    eos_port_exit_critical(crit);
    return EOS_KERN_TIMEOUT;
}

int eos_queue_receive(eos_queue_handle_t h, void *item, uint32_t timeout_ms)
{
    if (h >= EOS_MAX_QUEUES || !g_q[h].in_use || !item) return EOS_KERN_INVALID;

    uint32_t crit = eos_port_enter_critical();

    if (g_q[h].count > 0) {
        memcpy(item, g_q[h].buf + g_q[h].tail * g_q[h].item_size, g_q[h].item_size);
        g_q[h].tail = (g_q[h].tail + 1) % g_q[h].capacity;
        g_q[h].count--;

        /* Wake a sender if any are waiting */
        if (g_q[h].send_waiter_count > 0) {
            uint8_t waker = g_q[h].send_waiters[0];
            for (int j = 0; j < g_q[h].send_waiter_count - 1; j++)
                g_q[h].send_waiters[j] = g_q[h].send_waiters[j + 1];
            g_q[h].send_waiter_count--;
            eos_task_unblock(waker);
        }

        eos_port_exit_critical(crit);
        return EOS_KERN_OK;
    }

    /* Queue empty — block or fail */
    if (timeout_ms == EOS_NO_WAIT) {
        eos_port_exit_critical(crit);
        return EOS_KERN_EMPTY;
    }

    uint8_t caller = (uint8_t)eos_task_get_current();
    if (g_q[h].recv_waiter_count < Q_MAX_WAITERS) {
        g_q[h].recv_waiters[g_q[h].recv_waiter_count++] = caller;
    }
    eos_task_block_with_timeout(caller, timeout_ms);
    eos_port_exit_critical(crit);
    eos_port_yield();

    /* Retry after waking */
    crit = eos_port_enter_critical();
    if (g_q[h].count > 0) {
        memcpy(item, g_q[h].buf + g_q[h].tail * g_q[h].item_size, g_q[h].item_size);
        g_q[h].tail = (g_q[h].tail + 1) % g_q[h].capacity;
        g_q[h].count--;
        eos_port_exit_critical(crit);
        return EOS_KERN_OK;
    }

    /* Still empty — timeout */
    for (int i = 0; i < g_q[h].recv_waiter_count; i++) {
        if (g_q[h].recv_waiters[i] == caller) {
            for (int j = i; j < g_q[h].recv_waiter_count - 1; j++)
                g_q[h].recv_waiters[j] = g_q[h].recv_waiters[j + 1];
            g_q[h].recv_waiter_count--;
            break;
        }
    }
    eos_port_exit_critical(crit);
    return EOS_KERN_TIMEOUT;
}

int eos_queue_peek(eos_queue_handle_t h, void *item)
{
    if (h >= EOS_MAX_QUEUES || !g_q[h].in_use || !item) return EOS_KERN_INVALID;
    uint32_t crit = eos_port_enter_critical();
    if (g_q[h].count == 0) {
        eos_port_exit_critical(crit);
        return EOS_KERN_EMPTY;
    }
    memcpy(item, g_q[h].buf + g_q[h].tail * g_q[h].item_size, g_q[h].item_size);
    eos_port_exit_critical(crit);
    return EOS_KERN_OK;
}

int eos_queue_delete(eos_queue_handle_t h)
{
    if (h >= EOS_MAX_QUEUES || !g_q[h].in_use) return EOS_KERN_INVALID;
    uint32_t crit = eos_port_enter_critical();
    /* Wake all waiters */
    for (int i = 0; i < g_q[h].send_waiter_count; i++)
        eos_task_unblock(g_q[h].send_waiters[i]);
    for (int i = 0; i < g_q[h].recv_waiter_count; i++)
        eos_task_unblock(g_q[h].recv_waiters[i]);
    g_q[h].in_use = 0;
    eos_port_exit_critical(crit);
    return EOS_KERN_OK;
}

uint32_t eos_queue_count(eos_queue_handle_t h)
{
    if (h >= EOS_MAX_QUEUES || !g_q[h].in_use) return 0;
    return g_q[h].count;
}

bool eos_queue_is_full(eos_queue_handle_t h)
{
    if (h >= EOS_MAX_QUEUES || !g_q[h].in_use) return false;
    return g_q[h].count >= g_q[h].capacity;
}

bool eos_queue_is_empty(eos_queue_handle_t h)
{
    if (h >= EOS_MAX_QUEUES || !g_q[h].in_use) return true;
    return g_q[h].count == 0;
}
