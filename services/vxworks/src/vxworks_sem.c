// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file vxworks_sem.c
 * @brief VxWorks Semaphore API implementation over EoS kernel
 */

#include "eos/vxworks_sem.h"
#include "eos/kernel.h"
#include "eos/hal.h"

/* ----------------------------------------------------------------
 * Configuration
 * ---------------------------------------------------------------- */

#ifndef EOS_VXWORKS_MAX_SEMS
#define EOS_VXWORKS_MAX_SEMS    16
#endif

/* ----------------------------------------------------------------
 * Internal type-tagged semaphore table
 * ---------------------------------------------------------------- */

typedef enum {
    VXW_SEM_TYPE_NONE = 0,
    VXW_SEM_TYPE_BINARY,
    VXW_SEM_TYPE_MUTEX,
    VXW_SEM_TYPE_COUNTING,
} vxw_sem_type_t;

typedef struct {
    vxw_sem_type_t type;
    bool           in_use;
    union {
        eos_sem_handle_t   sem;
        eos_mutex_handle_t mtx;
    } handle;
} vxw_sem_entry_t;

static vxw_sem_entry_t s_sem_table[EOS_VXWORKS_MAX_SEMS];

/* ----------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------- */

static int vxw_sem_alloc(void)
{
    for (int i = 0; i < EOS_VXWORKS_MAX_SEMS; i++) {
        if (!s_sem_table[i].in_use) return i;
    }
    return -1;
}

static int vxw_sem_id_to_slot(SEM_ID id)
{
    if (id == SEM_ID_NULL) return -1;
    int slot = (int)(uintptr_t)id - 1;
    if (slot < 0 || slot >= EOS_VXWORKS_MAX_SEMS) return -1;
    if (!s_sem_table[slot].in_use) return -1;
    return slot;
}

static SEM_ID vxw_slot_to_sem_id(int slot)
{
    return (SEM_ID)(uintptr_t)(slot + 1);
}

static uint32_t vxw_sem_timeout_to_ms(int timeout)
{
    if (timeout == WAIT_FOREVER) return EOS_WAIT_FOREVER;
    if (timeout <= 0)            return EOS_NO_WAIT;
    return (uint32_t)timeout * EOS_VXWORKS_TICK_PERIOD_MS;
}

/* ----------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------- */

SEM_ID semBCreate(int options, SEM_B_STATE initialState)
{
    (void)options;

    int slot = vxw_sem_alloc();
    if (slot < 0) return SEM_ID_NULL;

    eos_sem_handle_t sh;
    uint32_t initial = (initialState == SEM_FULL) ? 1 : 0;
    if (eos_sem_create(&sh, initial, 1) != EOS_KERN_OK) {
        return SEM_ID_NULL;
    }

    s_sem_table[slot].type       = VXW_SEM_TYPE_BINARY;
    s_sem_table[slot].handle.sem = sh;
    s_sem_table[slot].in_use     = true;

    return vxw_slot_to_sem_id(slot);
}

SEM_ID semMCreate(int options)
{
    (void)options;

    int slot = vxw_sem_alloc();
    if (slot < 0) return SEM_ID_NULL;

    eos_mutex_handle_t mh;
    if (eos_mutex_create(&mh) != EOS_KERN_OK) {
        return SEM_ID_NULL;
    }

    s_sem_table[slot].type       = VXW_SEM_TYPE_MUTEX;
    s_sem_table[slot].handle.mtx = mh;
    s_sem_table[slot].in_use     = true;

    return vxw_slot_to_sem_id(slot);
}

SEM_ID semCCreate(int options, int initialCount)
{
    (void)options;

    int slot = vxw_sem_alloc();
    if (slot < 0) return SEM_ID_NULL;

    if (initialCount < 0) initialCount = 0;

    /* Use a large maximum for counting semaphores */
    uint32_t max_count = 0xFFFFFFFFU;

    eos_sem_handle_t sh;
    if (eos_sem_create(&sh, (uint32_t)initialCount, max_count) != EOS_KERN_OK) {
        return SEM_ID_NULL;
    }

    s_sem_table[slot].type       = VXW_SEM_TYPE_COUNTING;
    s_sem_table[slot].handle.sem = sh;
    s_sem_table[slot].in_use     = true;

    return vxw_slot_to_sem_id(slot);
}

STATUS semTake(SEM_ID semId, int timeout)
{
    int slot = vxw_sem_id_to_slot(semId);
    if (slot < 0) return ERROR;

    vxw_sem_entry_t *e = &s_sem_table[slot];
    uint32_t ms = vxw_sem_timeout_to_ms(timeout);
    int rc;

    switch (e->type) {
    case VXW_SEM_TYPE_BINARY:
    case VXW_SEM_TYPE_COUNTING:
        rc = eos_sem_wait(e->handle.sem, ms);
        break;
    case VXW_SEM_TYPE_MUTEX:
        rc = eos_mutex_lock(e->handle.mtx, ms);
        break;
    default:
        return ERROR;
    }

    return (rc == EOS_KERN_OK) ? OK : ERROR;
}

STATUS semGive(SEM_ID semId)
{
    int slot = vxw_sem_id_to_slot(semId);
    if (slot < 0) return ERROR;

    vxw_sem_entry_t *e = &s_sem_table[slot];
    int rc;

    switch (e->type) {
    case VXW_SEM_TYPE_BINARY:
    case VXW_SEM_TYPE_COUNTING:
        rc = eos_sem_post(e->handle.sem);
        break;
    case VXW_SEM_TYPE_MUTEX:
        rc = eos_mutex_unlock(e->handle.mtx);
        break;
    default:
        return ERROR;
    }

    return (rc == EOS_KERN_OK) ? OK : ERROR;
}

STATUS semDelete(SEM_ID semId)
{
    int slot = vxw_sem_id_to_slot(semId);
    if (slot < 0) return ERROR;

    vxw_sem_entry_t *e = &s_sem_table[slot];
    int rc;

    switch (e->type) {
    case VXW_SEM_TYPE_BINARY:
    case VXW_SEM_TYPE_COUNTING:
        rc = eos_sem_delete(e->handle.sem);
        break;
    case VXW_SEM_TYPE_MUTEX:
        rc = eos_mutex_delete(e->handle.mtx);
        break;
    default:
        return ERROR;
    }

    if (rc != EOS_KERN_OK) return ERROR;

    e->in_use = false;
    e->type   = VXW_SEM_TYPE_NONE;
    return OK;
}
