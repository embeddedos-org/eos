// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file posix_threads.h
 * @brief POSIX Threads (pthreads) compatibility layer for EoS
 *
 * Maps pthread API onto EoS kernel task primitives.
 */

#ifndef EOS_POSIX_THREADS_H
#define EOS_POSIX_THREADS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Thread handle — maps to an index in the posix thread table */
typedef uint8_t pthread_t;

/* Scheduling parameter */
struct sched_param {
    int sched_priority;
};

/* Thread attributes */
typedef struct {
    uint8_t  priority;
    uint32_t stack_size;
    int      detach_state;
} pthread_attr_t;

#define PTHREAD_CREATE_JOINABLE  0
#define PTHREAD_CREATE_DETACHED  1

#define PTHREAD_STACK_MIN        256

/**
 * Initialize thread attributes to default values.
 * Default: priority=5, stack_size=1024, joinable.
 */
int pthread_attr_init(pthread_attr_t *attr);

/**
 * Destroy thread attributes (no-op on EoS).
 */
int pthread_attr_destroy(pthread_attr_t *attr);

/**
 * Set scheduling priority in thread attributes.
 */
int pthread_attr_setschedparam(pthread_attr_t *attr,
                               const struct sched_param *param);

/**
 * Get scheduling priority from thread attributes.
 */
int pthread_attr_getschedparam(const pthread_attr_t *attr,
                               struct sched_param *param);

/**
 * Set stack size in thread attributes.
 */
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize);

/**
 * Get stack size from thread attributes.
 */
int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize);

/**
 * Create a new thread (maps to eos_task_create).
 *
 * @param thread   Output thread ID.
 * @param attr     Thread attributes (NULL for defaults).
 * @param start    Thread entry function (void*(*)(void*)).
 * @param arg      Argument passed to start.
 * @return 0 on success, errno on failure.
 */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start)(void *), void *arg);

/**
 * Wait for a thread to terminate.
 *
 * @param thread  Thread to join.
 * @param retval  Unused on EoS (set to NULL).
 * @return 0 on success, errno on failure.
 */
int pthread_join(pthread_t thread, void **retval);

/**
 * Terminate the calling thread.
 */
void pthread_exit(void *retval);

/**
 * Get the calling thread's ID.
 */
pthread_t pthread_self(void);

/**
 * Yield the processor to another thread.
 */
int pthread_yield(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_POSIX_THREADS_H */
