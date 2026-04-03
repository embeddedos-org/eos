// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file vxworks_sem.h
 * @brief VxWorks Semaphore API Compatibility Layer for EoS
 *
 * Maps VxWorks binary, mutex, and counting semaphore APIs to EoS
 * kernel mutex and semaphore primitives.
 */

#ifndef EOS_VXWORKS_SEM_H
#define EOS_VXWORKS_SEM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pull in STATUS / OK / ERROR if not already defined */
#ifndef OK
#define OK      0
#endif
#ifndef ERROR
#define ERROR   (-1)
#endif

#ifndef EOS_VXWORKS_TICK_PERIOD_MS
#define EOS_VXWORKS_TICK_PERIOD_MS  10
#endif

typedef int STATUS;
typedef void *SEM_ID;

#define SEM_ID_NULL     ((SEM_ID)0)

/* Binary semaphore initial state */
typedef enum {
    SEM_EMPTY = 0,
    SEM_FULL  = 1,
} SEM_B_STATE;

/* Semaphore options */
#define SEM_Q_FIFO          0x00
#define SEM_Q_PRIORITY      0x01
#define SEM_DELETE_SAFE     0x04
#define SEM_INVERSION_SAFE  0x08

/* Timeout constants (in ticks) */
#define WAIT_FOREVER    (-1)
#define NO_WAIT         0

/**
 * Create a binary semaphore.
 * @param options       SEM_Q_FIFO or SEM_Q_PRIORITY (advisory).
 * @param initialState  SEM_FULL or SEM_EMPTY.
 * @return SEM_ID or SEM_ID_NULL on failure.
 */
SEM_ID semBCreate(int options, SEM_B_STATE initialState);

/**
 * Create a mutual-exclusion semaphore.
 * @param options  SEM_Q_FIFO, SEM_Q_PRIORITY, SEM_INVERSION_SAFE, etc.
 * @return SEM_ID or SEM_ID_NULL on failure.
 */
SEM_ID semMCreate(int options);

/**
 * Create a counting semaphore.
 * @param options       SEM_Q_FIFO or SEM_Q_PRIORITY (advisory).
 * @param initialCount  Initial count value.
 * @return SEM_ID or SEM_ID_NULL on failure.
 */
SEM_ID semCCreate(int options, int initialCount);

/**
 * Take (acquire) a semaphore.
 * @param semId    Semaphore handle.
 * @param timeout  Timeout in ticks, WAIT_FOREVER, or NO_WAIT.
 * @return OK or ERROR.
 */
STATUS semTake(SEM_ID semId, int timeout);

/**
 * Give (release) a semaphore.
 * @param semId  Semaphore handle.
 * @return OK or ERROR.
 */
STATUS semGive(SEM_ID semId);

/**
 * Delete a semaphore and free resources.
 * @param semId  Semaphore handle.
 * @return OK or ERROR.
 */
STATUS semDelete(SEM_ID semId);

#ifdef __cplusplus
}
#endif

#endif /* EOS_VXWORKS_SEM_H */
