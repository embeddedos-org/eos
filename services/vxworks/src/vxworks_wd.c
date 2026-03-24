// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file vxworks_wd.c
 * @brief VxWorks Watchdog Timer API implementation over EoS kernel
 */

#include "eos/vxworks_wd.h"
#include "eos/kernel.h"
#include "eos/hal.h"

/* ----------------------------------------------------------------
 * Configuration
 * ---------------------------------------------------------------- */

#ifndef EOS_VXWORKS_MAX_WDOGS
#define EOS_VXWORKS_MAX_WDOGS   8
#endif

/* ----------------------------------------------------------------
 * Internal mapping table
 * ---------------------------------------------------------------- */

typedef struct {
    bool                  in_use;
    bool                  created;
    eos_swtimer_handle_t  eos_handle;
    WDOG_FUNC             callback;
    int                   parameter;
} vxw_wdog_entry_t;

static vxw_wdog_entry_t s_wdog_table[EOS_VXWORKS_MAX_WDOGS];

/* ----------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------- */

static int vxw_wdog_alloc(void)
{
    for (int i = 0; i < EOS_VXWORKS_MAX_WDOGS; i++) {
        if (!s_wdog_table[i].in_use) return i;
    }
    return -1;
}

static int vxw_wdog_id_to_slot(WDOG_ID id)
{
    if (id == WDOG_ID_NULL) return -1;
    int slot = (int)(uintptr_t)id - 1;
    if (slot < 0 || slot >= EOS_VXWORKS_MAX_WDOGS) return -1;
    if (!s_wdog_table[slot].in_use) return -1;
    return slot;
}

static WDOG_ID vxw_slot_to_wdog_id(int slot)
{
    return (WDOG_ID)(uintptr_t)(slot + 1);
}

/*
 * EoS timer callback trampoline.
 * EoS callback signature: void cb(eos_swtimer_handle_t handle, void *ctx)
 * VxWorks callback signature: void cb(int parameter)
 */
static void vxw_wdog_trampoline(eos_swtimer_handle_t handle, void *ctx)
{
    (void)handle;
    vxw_wdog_entry_t *e = (vxw_wdog_entry_t *)ctx;
    if (e != NULL && e->callback != NULL) {
        e->callback(e->parameter);
    }
}

/* ----------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------- */

WDOG_ID wdCreate(void)
{
    int slot = vxw_wdog_alloc();
    if (slot < 0) return WDOG_ID_NULL;

    vxw_wdog_entry_t *e = &s_wdog_table[slot];

    /*
     * Create the underlying EoS timer as a one-shot with a placeholder
     * period. The real period will be set at wdStart().
     */
    eos_swtimer_handle_t th;
    int rc = eos_swtimer_create(&th, "vxWdog", 1, false,
                                 vxw_wdog_trampoline, e);
    if (rc != EOS_KERN_OK) {
        return WDOG_ID_NULL;
    }

    e->eos_handle = th;
    e->callback   = NULL;
    e->parameter  = 0;
    e->created    = true;
    e->in_use     = true;

    return vxw_slot_to_wdog_id(slot);
}

STATUS wdStart(WDOG_ID wdId, int delay_ticks,
               WDOG_FUNC callback, int parameter)
{
    int slot = vxw_wdog_id_to_slot(wdId);
    if (slot < 0 || callback == NULL || delay_ticks <= 0) return ERROR;

    vxw_wdog_entry_t *e = &s_wdog_table[slot];

    /* Stop any previously running timer before reconfiguring */
    eos_swtimer_stop(e->eos_handle);

    e->callback  = callback;
    e->parameter = parameter;

    /*
     * EoS software timer doesn't expose a "set period" API after creation.
     * Recreate the timer with the new period. Delete the old one first.
     */
    eos_swtimer_delete(e->eos_handle);

    uint32_t period_ms = (uint32_t)delay_ticks * EOS_VXWORKS_TICK_PERIOD_MS;
    if (period_ms == 0) period_ms = 1;

    eos_swtimer_handle_t th;
    int rc = eos_swtimer_create(&th, "vxWdog", period_ms, false,
                                 vxw_wdog_trampoline, e);
    if (rc != EOS_KERN_OK) {
        e->in_use = false;
        return ERROR;
    }

    e->eos_handle = th;

    rc = eos_swtimer_start(e->eos_handle);
    return (rc == EOS_KERN_OK) ? OK : ERROR;
}

STATUS wdCancel(WDOG_ID wdId)
{
    int slot = vxw_wdog_id_to_slot(wdId);
    if (slot < 0) return ERROR;

    int rc = eos_swtimer_stop(s_wdog_table[slot].eos_handle);
    return (rc == EOS_KERN_OK) ? OK : ERROR;
}

STATUS wdDelete(WDOG_ID wdId)
{
    int slot = vxw_wdog_id_to_slot(wdId);
    if (slot < 0) return ERROR;

    vxw_wdog_entry_t *e = &s_wdog_table[slot];

    eos_swtimer_stop(e->eos_handle);
    int rc = eos_swtimer_delete(e->eos_handle);
    if (rc != EOS_KERN_OK) return ERROR;

    e->in_use  = false;
    e->created = false;
    return OK;
}
