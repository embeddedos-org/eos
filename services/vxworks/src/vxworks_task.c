// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file vxworks_task.c
 * @brief VxWorks Task API implementation over EoS kernel
 */

#include "eos/vxworks_task.h"
#include "eos/kernel.h"
#include "eos/hal.h"
#include <string.h>

/* ----------------------------------------------------------------
 * Configuration
 * ---------------------------------------------------------------- */

#ifndef EOS_VXWORKS_MAX_TASKS
#define EOS_VXWORKS_MAX_TASKS   EOS_MAX_TASKS
#endif

/* ----------------------------------------------------------------
 * Internal: wrapper argument block
 * ---------------------------------------------------------------- */

typedef struct {
    FUNCPTR entry;
    int     args[10];
    bool    in_use;
    eos_task_handle_t eos_handle;
} vxw_task_entry_t;

static vxw_task_entry_t s_task_table[EOS_VXWORKS_MAX_TASKS];

/* ----------------------------------------------------------------
 * Priority mapping
 *
 * VxWorks: 0 = highest, 255 = lowest
 * EoS kernel uses uint8_t priority where higher numeric values
 * mean higher priority. Invert the scale.
 * ---------------------------------------------------------------- */

static uint8_t vxw_priority_to_eos(int vxw_prio)
{
    if (vxw_prio < 0)   vxw_prio = 0;
    if (vxw_prio > 255) vxw_prio = 255;
    return (uint8_t)(255 - vxw_prio);
}

/* ----------------------------------------------------------------
 * Timeout conversion helpers
 * ---------------------------------------------------------------- */

static uint32_t vxw_ticks_to_ms(int ticks)
{
    if (ticks <= 0) return 0;
    return (uint32_t)ticks * EOS_VXWORKS_TICK_PERIOD_MS;
}

/* ----------------------------------------------------------------
 * Internal: find a free slot
 * ---------------------------------------------------------------- */

static int vxw_task_alloc_slot(void)
{
    for (int i = 0; i < EOS_VXWORKS_MAX_TASKS; i++) {
        if (!s_task_table[i].in_use) {
            return i;
        }
    }
    return -1;
}

/* Map TASK_ID back to table slot.  TASK_ID = slot + 1 (non-zero). */
static int vxw_task_id_to_slot(TASK_ID tid)
{
    int slot = tid - 1;
    if (slot < 0 || slot >= EOS_VXWORKS_MAX_TASKS) return -1;
    if (!s_task_table[slot].in_use) return -1;
    return slot;
}

/* Find slot by eos_handle */
static int vxw_task_find_by_handle(eos_task_handle_t h)
{
    for (int i = 0; i < EOS_VXWORKS_MAX_TASKS; i++) {
        if (s_task_table[i].in_use && s_task_table[i].eos_handle == h) {
            return i;
        }
    }
    return -1;
}

/* ----------------------------------------------------------------
 * Wrapper entry point: calls the real VxWorks-style entry with 10 args
 * ---------------------------------------------------------------- */

static void vxw_task_wrapper(void *arg)
{
    vxw_task_entry_t *e = (vxw_task_entry_t *)arg;
    (void)e->entry(e->args[0], e->args[1], e->args[2], e->args[3],
                   e->args[4], e->args[5], e->args[6], e->args[7],
                   e->args[8], e->args[9]);
}

/* ----------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------- */

TASK_ID taskSpawn(const char *name, int priority, int options,
                  int stackSize, FUNCPTR entryPt,
                  int arg1, int arg2, int arg3, int arg4, int arg5,
                  int arg6, int arg7, int arg8, int arg9, int arg10)
{
    (void)options;

    if (entryPt == NULL || stackSize <= 0) {
        return TASK_ID_ERROR;
    }

    int slot = vxw_task_alloc_slot();
    if (slot < 0) {
        return TASK_ID_ERROR;
    }

    vxw_task_entry_t *e = &s_task_table[slot];
    e->entry   = entryPt;
    e->args[0] = arg1;  e->args[1] = arg2;
    e->args[2] = arg3;  e->args[3] = arg4;
    e->args[4] = arg5;  e->args[5] = arg6;
    e->args[6] = arg7;  e->args[7] = arg8;
    e->args[8] = arg9;  e->args[9] = arg10;

    uint8_t eos_prio = vxw_priority_to_eos(priority);

    int rc = eos_task_create(name ? name : "tVxW",
                             vxw_task_wrapper, e,
                             eos_prio, (uint32_t)stackSize);
    if (rc < 0) {
        return TASK_ID_ERROR;
    }

    e->eos_handle = (eos_task_handle_t)rc;
    e->in_use     = true;

    return (TASK_ID)(slot + 1);
}

/* Resolve TASK_ID 0 → current task's slot */
static int vxw_resolve_tid(TASK_ID tid, eos_task_handle_t *out_handle)
{
    if (tid == 0) {
        eos_task_handle_t cur = eos_task_get_current();
        int slot = vxw_task_find_by_handle(cur);
        if (slot < 0) {
            /* Task not created through VxWorks layer – use handle directly */
            *out_handle = cur;
            return 0;
        }
        *out_handle = s_task_table[slot].eos_handle;
        return slot;
    }

    int slot = vxw_task_id_to_slot(tid);
    if (slot < 0) return -1;
    *out_handle = s_task_table[slot].eos_handle;
    return slot;
}

STATUS taskDelete(TASK_ID tid)
{
    eos_task_handle_t h;
    int slot = vxw_resolve_tid(tid, &h);
    if (slot == -1 && tid != 0) return ERROR;

    int rc = eos_task_delete(h);
    if (rc != EOS_KERN_OK) return ERROR;

    if (slot >= 0) {
        s_task_table[slot].in_use = false;
    }
    return OK;
}

STATUS taskSuspend(TASK_ID tid)
{
    eos_task_handle_t h;
    int slot = vxw_resolve_tid(tid, &h);
    if (slot == -1 && tid != 0) return ERROR;

    return (eos_task_suspend(h) == EOS_KERN_OK) ? OK : ERROR;
}

STATUS taskResume(TASK_ID tid)
{
    eos_task_handle_t h;
    int slot = vxw_resolve_tid(tid, &h);
    if (slot == -1 && tid != 0) return ERROR;

    return (eos_task_resume(h) == EOS_KERN_OK) ? OK : ERROR;
}

STATUS taskDelay(int ticks)
{
    if (ticks == 0) {
        eos_task_yield();
        return OK;
    }
    if (ticks < 0) return ERROR;

    eos_task_delay_ms(vxw_ticks_to_ms(ticks));
    return OK;
}

TASK_ID taskIdSelf(void)
{
    eos_task_handle_t cur = eos_task_get_current();
    int slot = vxw_task_find_by_handle(cur);
    if (slot < 0) {
        /* Task not managed by VxWorks layer; return handle + 1 as a
         * best-effort TASK_ID so the caller gets a non-zero value. */
        return (TASK_ID)(cur + 1);
    }
    return (TASK_ID)(slot + 1);
}

TASK_ID taskNameToId(const char *name)
{
    if (name == NULL) return TASK_ID_ERROR;

    for (int i = 0; i < EOS_VXWORKS_MAX_TASKS; i++) {
        if (!s_task_table[i].in_use) continue;

        const char *tn = eos_task_get_name(s_task_table[i].eos_handle);
        if (tn != NULL && strcmp(tn, name) == 0) {
            return (TASK_ID)(i + 1);
        }
    }
    return TASK_ID_ERROR;
}

STATUS taskLock(void)
{
    eos_irq_disable();
    return OK;
}

STATUS taskUnlock(void)
{
    eos_irq_enable();
    return OK;
}
