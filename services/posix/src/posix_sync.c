// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file posix_sync.c
 * @brief POSIX Synchronization primitives implementation for EoS
 */

#include <eos/posix_sync.h>
#include <eos/kernel.h>
#include <eos/hal.h>

#ifndef EINVAL
#define EINVAL  22
#endif
#ifndef EAGAIN
#define EAGAIN  11
#endif
#ifndef ENOMEM
#define ENOMEM  12
#endif
#ifndef EBUSY
#define EBUSY   16
#endif
#ifndef EPERM
#define EPERM    1
#endif

/* ============================================================
 * Mutex
 * ============================================================ */

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    (void)attr;
    if (!mutex) return EINVAL;

    eos_mutex_handle_t h;
    if (eos_mutex_create(&h) != EOS_KERN_OK) return ENOMEM;

    mutex->handle      = h;
    mutex->initialized = true;
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    if (!mutex || !mutex->initialized) return EINVAL;
    eos_mutex_delete(mutex->handle);
    mutex->initialized = false;
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
    if (!mutex) return EINVAL;
    if (!mutex->initialized) {
        int rc = pthread_mutex_init(mutex, NULL);
        if (rc != 0) return rc;
    }
    int rc = eos_mutex_lock(mutex->handle, EOS_WAIT_FOREVER);
    return (rc == EOS_KERN_OK) ? 0 : EINVAL;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex) {
    if (!mutex) return EINVAL;
    if (!mutex->initialized) {
        int rc = pthread_mutex_init(mutex, NULL);
        if (rc != 0) return rc;
    }
    int rc = eos_mutex_lock(mutex->handle, EOS_NO_WAIT);
    if (rc == EOS_KERN_OK)      return 0;
    if (rc == EOS_KERN_TIMEOUT) return EBUSY;
    return EINVAL;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    if (!mutex || !mutex->initialized) return EINVAL;
    int rc = eos_mutex_unlock(mutex->handle);
    return (rc == EOS_KERN_OK) ? 0 : EPERM;
}

/* ============================================================
 * Condition Variable
 *
 * Implementation: a counting semaphore (initial=0) acts as the
 * wait queue. cond_wait releases the mutex, blocks on the sem,
 * then re-acquires the mutex. signal posts once; broadcast
 * posts for each waiter.
 * ============================================================ */

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
    (void)attr;
    if (!cond) return EINVAL;

    eos_sem_handle_t h;
    if (eos_sem_create(&h, 0, EOS_MAX_TASKS) != EOS_KERN_OK) return ENOMEM;

    cond->sem_handle  = h;
    cond->waiters     = 0;
    cond->initialized = true;
    return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond) {
    if (!cond || !cond->initialized) return EINVAL;
    eos_sem_delete(cond->sem_handle);
    cond->initialized = false;
    return 0;
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    if (!cond || !mutex) return EINVAL;
    if (!cond->initialized) {
        int rc = pthread_cond_init(cond, NULL);
        if (rc != 0) return rc;
    }

    eos_irq_disable();
    cond->waiters++;
    eos_irq_enable();

    pthread_mutex_unlock(mutex);
    eos_sem_wait(cond->sem_handle, EOS_WAIT_FOREVER);
    pthread_mutex_lock(mutex);
    return 0;
}

int pthread_cond_signal(pthread_cond_t *cond) {
    if (!cond || !cond->initialized) return EINVAL;

    eos_irq_disable();
    if (cond->waiters > 0) {
        cond->waiters--;
        eos_irq_enable();
        eos_sem_post(cond->sem_handle);
    } else {
        eos_irq_enable();
    }
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t *cond) {
    if (!cond || !cond->initialized) return EINVAL;

    eos_irq_disable();
    uint32_t n = cond->waiters;
    cond->waiters = 0;
    eos_irq_enable();

    for (uint32_t i = 0; i < n; i++) {
        eos_sem_post(cond->sem_handle);
    }
    return 0;
}

/* ============================================================
 * Read-Write Lock
 *
 * Strategy:
 *   - reader_sem: counting semaphore initialized to MAX_READERS.
 *     Each reader acquires one count; writer acquires all counts.
 *   - writer_mutex: ensures only one writer at a time.
 * ============================================================ */

int pthread_rwlock_init(pthread_rwlock_t *rwlock,
                        const pthread_rwlockattr_t *attr) {
    (void)attr;
    if (!rwlock) return EINVAL;

    eos_sem_handle_t rsem;
    eos_mutex_handle_t wmtx;

    if (eos_sem_create(&rsem, EOS_POSIX_RWLOCK_MAX_READERS,
                       EOS_POSIX_RWLOCK_MAX_READERS) != EOS_KERN_OK)
        return ENOMEM;

    if (eos_mutex_create(&wmtx) != EOS_KERN_OK) {
        eos_sem_delete(rsem);
        return ENOMEM;
    }

    rwlock->reader_sem   = rsem;
    rwlock->writer_mutex = wmtx;
    rwlock->reader_count = 0;
    rwlock->initialized  = true;
    return 0;
}

int pthread_rwlock_destroy(pthread_rwlock_t *rwlock) {
    if (!rwlock || !rwlock->initialized) return EINVAL;
    eos_sem_delete(rwlock->reader_sem);
    eos_mutex_delete(rwlock->writer_mutex);
    rwlock->initialized = false;
    return 0;
}

int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock) {
    if (!rwlock || !rwlock->initialized) return EINVAL;
    /* Acquire one reader slot */
    int rc = eos_sem_wait(rwlock->reader_sem, EOS_WAIT_FOREVER);
    if (rc != EOS_KERN_OK) return EINVAL;

    eos_irq_disable();
    rwlock->reader_count++;
    eos_irq_enable();
    return 0;
}

int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock) {
    if (!rwlock || !rwlock->initialized) return EINVAL;

    /* Exclusive: acquire writer mutex first */
    if (eos_mutex_lock(rwlock->writer_mutex, EOS_WAIT_FOREVER) != EOS_KERN_OK)
        return EINVAL;

    /* Drain all reader slots to ensure no readers */
    for (uint32_t i = 0; i < EOS_POSIX_RWLOCK_MAX_READERS; i++) {
        if (eos_sem_wait(rwlock->reader_sem, EOS_WAIT_FOREVER) != EOS_KERN_OK) {
            eos_mutex_unlock(rwlock->writer_mutex);
            return EINVAL;
        }
    }
    return 0;
}

int pthread_rwlock_unlock(pthread_rwlock_t *rwlock) {
    if (!rwlock || !rwlock->initialized) return EINVAL;

    eos_irq_disable();
    if (rwlock->reader_count > 0) {
        /* Reader unlock: release one slot */
        rwlock->reader_count--;
        eos_irq_enable();
        eos_sem_post(rwlock->reader_sem);
    } else {
        /* Writer unlock: release all reader slots + writer mutex */
        eos_irq_enable();
        for (uint32_t i = 0; i < EOS_POSIX_RWLOCK_MAX_READERS; i++) {
            eos_sem_post(rwlock->reader_sem);
        }
        eos_mutex_unlock(rwlock->writer_mutex);
    }
    return 0;
}

/* ============================================================
 * POSIX Semaphore — thin wrapper over eos_sem_*
 * ============================================================ */

int sem_init(sem_t *sem, int pshared, unsigned int value) {
    (void)pshared;
    if (!sem) return -1;

    eos_sem_handle_t h;
    if (eos_sem_create(&h, value, 0xFFFFFFFF) != EOS_KERN_OK) return -1;

    sem->handle      = h;
    sem->initialized = true;
    return 0;
}

int sem_destroy(sem_t *sem) {
    if (!sem || !sem->initialized) return -1;
    eos_sem_delete(sem->handle);
    sem->initialized = false;
    return 0;
}

int sem_wait(sem_t *sem) {
    if (!sem || !sem->initialized) return -1;
    return (eos_sem_wait(sem->handle, EOS_WAIT_FOREVER) == EOS_KERN_OK) ? 0 : -1;
}

int sem_trywait(sem_t *sem) {
    if (!sem || !sem->initialized) return -1;
    int rc = eos_sem_wait(sem->handle, EOS_NO_WAIT);
    if (rc == EOS_KERN_OK) return 0;
    return -1; /* EAGAIN */
}

int sem_post(sem_t *sem) {
    if (!sem || !sem->initialized) return -1;
    return (eos_sem_post(sem->handle) == EOS_KERN_OK) ? 0 : -1;
}

int sem_getvalue(sem_t *sem, int *sval) {
    if (!sem || !sem->initialized || !sval) return -1;
    *sval = (int)eos_sem_get_count(sem->handle);
    return 0;
}
