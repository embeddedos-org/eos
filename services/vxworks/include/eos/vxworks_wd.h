// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file vxworks_wd.h
 * @brief VxWorks Watchdog Timer API Compatibility Layer for EoS
 *
 * Maps VxWorks watchdog timer APIs to EoS kernel software timer primitives.
 */

#ifndef EOS_VXWORKS_WD_H
#define EOS_VXWORKS_WD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

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
typedef void *WDOG_ID;

#define WDOG_ID_NULL    ((WDOG_ID)0)

/* VxWorks watchdog callback: receives a single int-sized parameter */
typedef void (*WDOG_FUNC)(int parameter);

/**
 * Create a watchdog timer.
 * @return WDOG_ID or WDOG_ID_NULL on failure.
 */
WDOG_ID wdCreate(void);

/**
 * Start (or restart) a watchdog timer.
 * @param wdId          Watchdog handle.
 * @param delay_ticks   Delay in system ticks before the callback fires.
 * @param callback      Function to call on expiry.
 * @param parameter     Integer parameter passed to the callback.
 * @return OK or ERROR.
 */
STATUS wdStart(WDOG_ID wdId, int delay_ticks,
               WDOG_FUNC callback, int parameter);

/**
 * Cancel a running watchdog timer.
 * @param wdId  Watchdog handle.
 * @return OK or ERROR.
 */
STATUS wdCancel(WDOG_ID wdId);

/**
 * Delete a watchdog timer and free resources.
 * @param wdId  Watchdog handle.
 * @return OK or ERROR.
 */
STATUS wdDelete(WDOG_ID wdId);

#ifdef __cplusplus
}
#endif

#endif /* EOS_VXWORKS_WD_H */
