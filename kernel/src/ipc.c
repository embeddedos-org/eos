// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/kernel.h"
#include <string.h>

typedef struct {
    uint8_t  in_use;
    uint8_t  *buf;
    uint8_t  _storage[16 * 64];
    size_t   item_size;
    uint32_t capacity, head, tail, count;
} queue_t;

static queue_t g_q[EOS_MAX_QUEUES];

int eos_queue_create(eos_queue_handle_t *out, size_t item_size, uint32_t capacity) {
    if (!out || item_size == 0 || capacity == 0) return EOS_KERN_INVALID;
    if (item_size * capacity > sizeof(g_q[0]._storage)) return EOS_KERN_NO_MEMORY;
    for (int i = 0; i < EOS_MAX_QUEUES; i++) {
        if (!g_q[i].in_use) {
            memset(&g_q[i], 0, sizeof(queue_t));
            g_q[i].in_use = 1; g_q[i].item_size = item_size;
            g_q[i].capacity = capacity; g_q[i].buf = g_q[i]._storage;
            *out = (uint8_t)i;
            return EOS_KERN_OK;
        }
    }
    return EOS_KERN_NO_MEMORY;
}

int eos_queue_send(eos_queue_handle_t h, const void *item, uint32_t timeout_ms) {
    if (h >= EOS_MAX_QUEUES || !g_q[h].in_use || !item) return EOS_KERN_INVALID;
    (void)timeout_ms;
    if (g_q[h].count >= g_q[h].capacity) return EOS_KERN_FULL;
    memcpy(g_q[h].buf + g_q[h].head * g_q[h].item_size, item, g_q[h].item_size);
    g_q[h].head = (g_q[h].head + 1) % g_q[h].capacity;
    g_q[h].count++;
    return EOS_KERN_OK;
}

int eos_queue_receive(eos_queue_handle_t h, void *item, uint32_t timeout_ms) {
    if (h >= EOS_MAX_QUEUES || !g_q[h].in_use || !item) return EOS_KERN_INVALID;
    (void)timeout_ms;
    if (g_q[h].count == 0) return EOS_KERN_EMPTY;
    memcpy(item, g_q[h].buf + g_q[h].tail * g_q[h].item_size, g_q[h].item_size);
    g_q[h].tail = (g_q[h].tail + 1) % g_q[h].capacity;
    g_q[h].count--;
    return EOS_KERN_OK;
}

int eos_queue_peek(eos_queue_handle_t h, void *item) {
    if (h >= EOS_MAX_QUEUES || !g_q[h].in_use || !item) return EOS_KERN_INVALID;
    if (g_q[h].count == 0) return EOS_KERN_EMPTY;
    memcpy(item, g_q[h].buf + g_q[h].tail * g_q[h].item_size, g_q[h].item_size);
    return EOS_KERN_OK;
}

int eos_queue_delete(eos_queue_handle_t h) {
    if (h >= EOS_MAX_QUEUES || !g_q[h].in_use) return EOS_KERN_INVALID;
    g_q[h].in_use = 0;
    return EOS_KERN_OK;
}

uint32_t eos_queue_count(eos_queue_handle_t h) {
    if (h >= EOS_MAX_QUEUES || !g_q[h].in_use) return 0;
    return g_q[h].count;
}

bool eos_queue_is_full(eos_queue_handle_t h) {
    if (h >= EOS_MAX_QUEUES || !g_q[h].in_use) return false;
    return g_q[h].count >= g_q[h].capacity;
}

bool eos_queue_is_empty(eos_queue_handle_t h) {
    if (h >= EOS_MAX_QUEUES || !g_q[h].in_use) return true;
    return g_q[h].count == 0;
}