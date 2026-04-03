// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/kernel.h"
#include <string.h>

typedef struct { uint8_t in_use, locked, owner, rec_count; } mtx_t;
typedef struct { uint8_t in_use; int32_t count, max_count; } sem_t;

static mtx_t g_mtx[EOS_MAX_MUTEXES];
static sem_t g_sem[EOS_MAX_SEMAPHORES];

int eos_mutex_create(eos_mutex_handle_t *out) {
    if (!out) return EOS_KERN_INVALID;
    for (int i = 0; i < EOS_MAX_MUTEXES; i++) {
        if (!g_mtx[i].in_use) {
            memset(&g_mtx[i], 0, sizeof(mtx_t));
            g_mtx[i].in_use = 1; g_mtx[i].owner = 0xFF;
            *out = (uint8_t)i;
            return EOS_KERN_OK;
        }
    }
    return EOS_KERN_NO_MEMORY;
}

int eos_mutex_lock(eos_mutex_handle_t h, uint32_t timeout_ms) {
    if (h >= EOS_MAX_MUTEXES || !g_mtx[h].in_use) return EOS_KERN_INVALID;
    (void)timeout_ms;
    uint8_t caller = (uint8_t)eos_task_get_current();
    if (!g_mtx[h].locked) { g_mtx[h].locked = 1; g_mtx[h].owner = caller; g_mtx[h].rec_count = 1; return EOS_KERN_OK; }
    if (g_mtx[h].owner == caller) { g_mtx[h].rec_count++; return EOS_KERN_OK; }
    return EOS_KERN_TIMEOUT;
}

int eos_mutex_unlock(eos_mutex_handle_t h) {
    if (h >= EOS_MAX_MUTEXES || !g_mtx[h].in_use || !g_mtx[h].locked) return EOS_KERN_INVALID;
    if (--g_mtx[h].rec_count == 0) { g_mtx[h].locked = 0; g_mtx[h].owner = 0xFF; }
    return EOS_KERN_OK;
}

int eos_mutex_delete(eos_mutex_handle_t h) {
    if (h >= EOS_MAX_MUTEXES || !g_mtx[h].in_use) return EOS_KERN_INVALID;
    g_mtx[h].in_use = 0;
    return EOS_KERN_OK;
}

int eos_sem_create(eos_sem_handle_t *out, uint32_t initial, uint32_t max) {
    if (!out || max == 0) return EOS_KERN_INVALID;
    for (int i = 0; i < EOS_MAX_SEMAPHORES; i++) {
        if (!g_sem[i].in_use) {
            g_sem[i].in_use = 1; g_sem[i].count = (int32_t)initial; g_sem[i].max_count = (int32_t)max;
            *out = (uint8_t)i;
            return EOS_KERN_OK;
        }
    }
    return EOS_KERN_NO_MEMORY;
}

int eos_sem_wait(eos_sem_handle_t h, uint32_t timeout_ms) {
    if (h >= EOS_MAX_SEMAPHORES || !g_sem[h].in_use) return EOS_KERN_INVALID;
    (void)timeout_ms;
    if (g_sem[h].count > 0) { g_sem[h].count--; return EOS_KERN_OK; }
    return EOS_KERN_TIMEOUT;
}

int eos_sem_post(eos_sem_handle_t h) {
    if (h >= EOS_MAX_SEMAPHORES || !g_sem[h].in_use) return EOS_KERN_INVALID;
    if (g_sem[h].count < g_sem[h].max_count) { g_sem[h].count++; return EOS_KERN_OK; }
    return EOS_KERN_FULL;
}

int eos_sem_delete(eos_sem_handle_t h) {
    if (h >= EOS_MAX_SEMAPHORES || !g_sem[h].in_use) return EOS_KERN_INVALID;
    g_sem[h].in_use = 0;
    return EOS_KERN_OK;
}

uint32_t eos_sem_get_count(eos_sem_handle_t h) {
    if (h >= EOS_MAX_SEMAPHORES || !g_sem[h].in_use) return 0;
    return (uint32_t)g_sem[h].count;
}