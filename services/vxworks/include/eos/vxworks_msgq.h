// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file vxworks_msgq.h
 * @brief VxWorks Message Queue API Compatibility Layer for EoS
 *
 * Maps VxWorks message queue APIs to EoS kernel queue primitives.
 */

#ifndef EOS_VXWORKS_MSGQ_H
#define EOS_VXWORKS_MSGQ_H

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
typedef void *MSG_Q_ID;

#define MSG_Q_ID_NULL   ((MSG_Q_ID)0)

/* Message priority */
#define MSG_PRI_NORMAL  0
#define MSG_PRI_URGENT  1

/* Queue options */
#define MSG_Q_FIFO      0x00
#define MSG_Q_PRIORITY  0x01

/* Timeout constants (in ticks) */
#ifndef WAIT_FOREVER
#define WAIT_FOREVER    (-1)
#endif
#ifndef NO_WAIT
#define NO_WAIT         0
#endif

/**
 * Create a message queue.
 * @param maxMsgs       Maximum number of messages in the queue.
 * @param maxMsgLength  Maximum length of each message in bytes.
 * @param options       MSG_Q_FIFO or MSG_Q_PRIORITY (advisory).
 * @return MSG_Q_ID or MSG_Q_ID_NULL on failure.
 */
MSG_Q_ID msgQCreate(int maxMsgs, int maxMsgLength, int options);

/**
 * Send a message to a queue.
 * @param msgQId    Message queue handle.
 * @param buffer    Pointer to message data.
 * @param nBytes    Number of bytes to send (must be <= maxMsgLength).
 * @param timeout   Timeout in ticks, WAIT_FOREVER, or NO_WAIT.
 * @param priority  MSG_PRI_NORMAL or MSG_PRI_URGENT.
 * @return OK or ERROR.
 */
STATUS msgQSend(MSG_Q_ID msgQId, const char *buffer, uint32_t nBytes,
                int timeout, int priority);

/**
 * Receive a message from a queue.
 * @param msgQId    Message queue handle.
 * @param buffer    Buffer to receive message data.
 * @param maxNBytes Maximum bytes to receive.
 * @param timeout   Timeout in ticks, WAIT_FOREVER, or NO_WAIT.
 * @return Number of bytes received, or ERROR.
 */
int msgQReceive(MSG_Q_ID msgQId, char *buffer, uint32_t maxNBytes,
                int timeout);

/**
 * Delete a message queue.
 * @param msgQId  Message queue handle.
 * @return OK or ERROR.
 */
STATUS msgQDelete(MSG_Q_ID msgQId);

/**
 * Get the number of messages currently in the queue.
 * @param msgQId  Message queue handle.
 * @return Number of messages, or ERROR.
 */
int msgQNumMsgs(MSG_Q_ID msgQId);

#ifdef __cplusplus
}
#endif

#endif /* EOS_VXWORKS_MSGQ_H */
