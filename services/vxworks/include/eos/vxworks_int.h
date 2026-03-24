// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file vxworks_int.h
 * @brief VxWorks Interrupt API Compatibility Layer for EoS
 *
 * Maps VxWorks interrupt management APIs to EoS HAL interrupt primitives.
 */

#ifndef EOS_VXWORKS_INT_H
#define EOS_VXWORKS_INT_H

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

typedef int STATUS;

/* VxWorks interrupt routine type */
typedef void (*VOIDFUNCPTR)(int parameter);

/**
 * Connect a C routine to a hardware interrupt vector.
 *
 * @param vector     Interrupt vector number.
 * @param routine    ISR to connect.
 * @param parameter  Integer parameter passed to the ISR.
 * @return OK or ERROR.
 */
STATUS intConnect(int vector, VOIDFUNCPTR routine, int parameter);

/**
 * Lock out interrupts (disable all maskable interrupts).
 *
 * @return Lock key to be passed to intUnlock().
 */
int intLock(void);

/**
 * Unlock (re-enable) interrupts.
 *
 * @param lockKey  Value returned by the matching intLock() call.
 */
void intUnlock(int lockKey);

#ifdef __cplusplus
}
#endif

#endif /* EOS_VXWORKS_INT_H */
