// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file vxworks_msgq.c
 * @brief VxWorks Message Queue API implementation over EoS kernel
 *
 * @note MSG_PRI_URGENT is accepted but treated identically to MSG_PRI_NORMAL,
 *       as the EoS kernel queue does not support message-level priority.
 */

#include "eos/vxworks_msgq.h"
#include "eos/kernel.h"
#include "eos/hal.h"
#include <string.h>

/* ----------------------------------------------------------------
 * Configuration
 * ---------------------------------------------------------------- */

#ifndef EOS_VXWORKS_MAX_MSGQS
#define EOS_VXWORKS_MAX_MSGQS   8
#endif

/* ----------------------------------------------------------------
 * Internal mapping table
 * ---------------------------------------------------------------- */

typedef struct {
    bool               in_use;
    eos_queue_handle_t eos_handle;
    uint32_t           msg_size;
} vxw_msgq_entry_t;

static vxw_msgq_entry_t s_msgq_table[EOS_VXWORKS_MAX_MSGQS];

/* ----------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------- */

static int vxw_msgq_alloc(void)
{
    for (int i = 0; i < EOS_VXWORKS_MAX_MSGQS; i++) {
        if (!s_msgq_table[i].in_use) return i;
    }
    return -1;
}

static int vxw_msgq_id_to_slot(MSG_Q_ID id)
{
    if (id == MSG_Q_ID_NULL) return -1;
    int slot = (int)(uintptr_t)id - 1;
    if (slot < 0 || slot >= EOS_VXWORKS_MAX_MSGQS) return -1;
    if (!s_msgq_table[slot].in_use) return -1;
    return slot;
}

static MSG_Q_ID vxw_slot_to_msgq_id(int slot)
{
    return (MSG_Q_ID)(uintptr_t)(slot + 1);
}

static uint32_t vxw_msgq_timeout_to_ms(int timeout)
{
    if (timeout == WAIT_FOREVER) return EOS_WAIT_FOREVER;
    if (timeout <= 0)            return EOS_NO_WAIT;
    return (uint32_t)timeout * EOS_VXWORKS_TICK_PERIOD_MS;
}

/* ----------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------- */

MSG_Q_ID msgQCreate(int maxMsgs, int maxMsgLength, int options)
{
    (void)options;

    if (maxMsgs <= 0 || maxMsgLength <= 0) return MSG_Q_ID_NULL;

    int slot = vxw_msgq_alloc();
    if (slot < 0) return MSG_Q_ID_NULL;

    eos_queue_handle_t qh;
    if (eos_queue_create(&qh, (size_t)maxMsgLength, (uint32_t)maxMsgs) != EOS_KERN_OK) {
        return MSG_Q_ID_NULL;
    }

    s_msgq_table[slot].eos_handle = qh;
    s_msgq_table[slot].msg_size   = (uint32_t)maxMsgLength;
    s_msgq_table[slot].in_use     = true;

    return vxw_slot_to_msgq_id(slot);
}

STATUS msgQSend(MSG_Q_ID msgQId, const char *buffer, uint32_t nBytes,
                int timeout, int priority)
{
    (void)priority; /* MSG_PRI_URGENT treated as MSG_PRI_NORMAL */

    int slot = vxw_msgq_id_to_slot(msgQId);
    if (slot < 0 || buffer == NULL) return ERROR;

    vxw_msgq_entry_t *e = &s_msgq_table[slot];

    if (nBytes > e->msg_size) return ERROR;

    /*
     * EoS queues use fixed-size items.  If nBytes < msg_size we send
     * the full item_size buffer.  The caller must handle framing.
     * Copy into a zero-initialised staging buffer so the remainder
     * is deterministic.
     */
    uint8_t staging[e->msg_size];
    memset(staging, 0, e->msg_size);
    memcpy(staging, buffer, nBytes);

    uint32_t ms = vxw_msgq_timeout_to_ms(timeout);
    int rc = eos_queue_send(e->eos_handle, staging, ms);
    return (rc == EOS_KERN_OK) ? OK : ERROR;
}

int msgQReceive(MSG_Q_ID msgQId, char *buffer, uint32_t maxNBytes,
                int timeout)
{
    int slot = vxw_msgq_id_to_slot(msgQId);
    if (slot < 0 || buffer == NULL) return ERROR;

    vxw_msgq_entry_t *e = &s_msgq_table[slot];

    uint8_t staging[e->msg_size];
    uint32_t ms = vxw_msgq_timeout_to_ms(timeout);

    int rc = eos_queue_receive(e->eos_handle, staging, ms);
    if (rc != EOS_KERN_OK) return ERROR;

    uint32_t copy_len = (maxNBytes < e->msg_size) ? maxNBytes : e->msg_size;
    memcpy(buffer, staging, copy_len);

    return (int)copy_len;
}

STATUS msgQDelete(MSG_Q_ID msgQId)
{
    int slot = vxw_msgq_id_to_slot(msgQId);
    if (slot < 0) return ERROR;

    int rc = eos_queue_delete(s_msgq_table[slot].eos_handle);
    if (rc != EOS_KERN_OK) return ERROR;

    s_msgq_table[slot].in_use = false;
    return OK;
}

int msgQNumMsgs(MSG_Q_ID msgQId)
{
    int slot = vxw_msgq_id_to_slot(msgQId);
    if (slot < 0) return ERROR;

    return (int)eos_queue_count(s_msgq_table[slot].eos_handle);
}
