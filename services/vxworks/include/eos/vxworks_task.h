// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file vxworks_task.h
 * @brief VxWorks Task API Compatibility Layer for EoS
 *
 * Maps VxWorks task management functions to EoS kernel task primitives.
 */

#ifndef EOS_VXWORKS_TASK_H
#define EOS_VXWORKS_TASK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* VxWorks status codes */
#define OK      0
#define ERROR   (-1)

typedef int STATUS;
typedef int TASK_ID;

#define TASK_ID_ERROR   0

/* VxWorks task options (accepted but largely ignored on EoS) */
#define VX_FP_TASK          0x0008
#define VX_NO_STACK_FILL    0x0100
#define VX_UNBREAKABLE      0x0002
#define VX_DEALLOC_STACK    0x0004

/* VxWorks tick period in ms (configurable) */
#ifndef EOS_VXWORKS_TICK_PERIOD_MS
#define EOS_VXWORKS_TICK_PERIOD_MS  10
#endif

/* VxWorks task entry function type: receives up to 10 int-sized arguments */
typedef int (*FUNCPTR)(int, int, int, int, int, int, int, int, int, int);

/**
 * Spawn a new task.
 *
 * @param name      Task name (may be NULL).
 * @param priority  VxWorks priority 0 (highest) – 255 (lowest).
 * @param options   Task option flags (accepted, not all honored).
 * @param stackSize Stack size in bytes.
 * @param entryPt   Task entry function.
 * @param arg1..arg10  Arguments passed to the entry function.
 * @return TASK_ID on success, TASK_ID_ERROR on failure.
 */
TASK_ID taskSpawn(const char *name, int priority, int options,
                  int stackSize, FUNCPTR entryPt,
                  int arg1, int arg2, int arg3, int arg4, int arg5,
                  int arg6, int arg7, int arg8, int arg9, int arg10);

/**
 * Delete a task.
 * @param tid  Task ID (0 = self).
 * @return OK or ERROR.
 */
STATUS taskDelete(TASK_ID tid);

/**
 * Suspend a task.
 * @param tid  Task ID (0 = self).
 * @return OK or ERROR.
 */
STATUS taskSuspend(TASK_ID tid);

/**
 * Resume a suspended task.
 * @param tid  Task ID.
 * @return OK or ERROR.
 */
STATUS taskResume(TASK_ID tid);

/**
 * Delay the calling task by the given number of ticks.
 * @param ticks  Number of system ticks to delay.
 *               0 yields the CPU (equivalent to taskIdSelf yield).
 * @return OK or ERROR.
 */
STATUS taskDelay(int ticks);

/**
 * Get the TASK_ID of the calling task.
 * @return TASK_ID of the current task.
 */
TASK_ID taskIdSelf(void);

/**
 * Look up a task by name.
 * @param name  Task name to search for.
 * @return TASK_ID or TASK_ID_ERROR if not found.
 */
TASK_ID taskNameToId(const char *name);

/**
 * Disable task preemption (implemented via interrupt lock on EoS).
 * @return OK.
 */
STATUS taskLock(void);

/**
 * Re-enable task preemption.
 * @return OK.
 */
STATUS taskUnlock(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_VXWORKS_TASK_H */
