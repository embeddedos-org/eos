// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file posix_sync.h
 * @brief POSIX Synchronization primitives for EoS
 *
 * Provides pthread_mutex, pthread_cond, pthread_rwlock, and POSIX semaphore
 * APIs mapped onto EoS kernel primitives.
 */

#ifndef EOS_POSIX_SYNC_H
#define EOS_POSIX_SYNC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Mutex
 * ============================================================ */

typedef struct {
    uint8_t handle;   /* eos_mutex_handle_t */
    bool    initialized;
} pthread_mutex_t;

typedef struct {
    int type; /* unused, reserved */
} pthread_mutexattr_t;

#define PTHREAD_MUTEX_INITIALIZER { 0, false }

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_trylock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

/* ============================================================
 * Condition Variable
 * ============================================================ */

typedef struct {
    uint8_t sem_handle;   /* eos_sem_handle_t — waiters block here */
    uint32_t waiters;     /* number of threads waiting */
    bool     initialized;
} pthread_cond_t;

typedef struct {
    int dummy;
} pthread_condattr_t;

#define PTHREAD_COND_INITIALIZER { 0, 0, false }

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);
int pthread_cond_destroy(pthread_cond_t *cond);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_broadcast(pthread_cond_t *cond);

/* ============================================================
 * Read-Write Lock
 * ============================================================ */

typedef struct {
    uint8_t  reader_sem;   /* counting sem for reader slots */
    uint8_t  writer_mutex; /* mutex for exclusive writer access */
    uint32_t reader_count;
    bool     initialized;
} pthread_rwlock_t;

typedef struct {
    int dummy;
} pthread_rwlockattr_t;

#define PTHREAD_RWLOCK_INITIALIZER { 0, 0, 0, false }

#ifndef EOS_POSIX_RWLOCK_MAX_READERS
#define EOS_POSIX_RWLOCK_MAX_READERS  EOS_MAX_TASKS
#endif

int pthread_rwlock_init(pthread_rwlock_t *rwlock,
                        const pthread_rwlockattr_t *attr);
int pthread_rwlock_destroy(pthread_rwlock_t *rwlock);
int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_unlock(pthread_rwlock_t *rwlock);

/* ============================================================
 * POSIX Semaphore
 * ============================================================ */

typedef struct {
    uint8_t handle;   /* eos_sem_handle_t */
    bool    initialized;
} sem_t;

int sem_init(sem_t *sem, int pshared, unsigned int value);
int sem_destroy(sem_t *sem);
int sem_wait(sem_t *sem);
int sem_trywait(sem_t *sem);
int sem_post(sem_t *sem);
int sem_getvalue(sem_t *sem, int *sval);

#ifdef __cplusplus
}
#endif

#endif /* EOS_POSIX_SYNC_H */
