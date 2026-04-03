// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file posix_threads.c
 * @brief POSIX Threads implementation for EoS
 */

#include <eos/posix_threads.h>
#include <eos/kernel.h>
#include <eos/hal.h>
#include <string.h>

/* ============================================================
 * Thread table — maps pthread_t to EoS task handles
 * ============================================================ */

typedef struct {
    bool              active;
    eos_task_handle_t eos_handle;
    eos_sem_handle_t  join_sem;
    void *(*start_routine)(void *);
    void             *arg;
    void             *retval;
    bool              detached;
} posix_thread_entry_t;

static posix_thread_entry_t posix_thread_table[EOS_MAX_TASKS];
static bool                 posix_threads_initialized = false;

static void posix_threads_init_once(void) {
    if (!posix_threads_initialized) {
        memset(posix_thread_table, 0, sizeof(posix_thread_table));
        posix_threads_initialized = true;
    }
}

static int posix_thread_alloc(void) {
    for (int i = 0; i < EOS_MAX_TASKS; i++) {
        if (!posix_thread_table[i].active) {
            return i;
        }
    }
    return -1;
}

/* Wrapper: EoS tasks use void(*)(void*), pthreads use void*(*)(void*) */
static void posix_thread_wrapper(void *arg) {
    uint8_t slot = (uint8_t)(uintptr_t)arg;
    posix_thread_entry_t *entry = &posix_thread_table[slot];

    entry->retval = entry->start_routine(entry->arg);

    /* Signal any joiner */
    eos_sem_post(entry->join_sem);

    if (entry->detached) {
        entry->active = false;
        eos_sem_delete(entry->join_sem);
    }

    eos_task_delete(entry->eos_handle);
}

/* ============================================================
 * pthread_attr
 * ============================================================ */

int pthread_attr_init(pthread_attr_t *attr) {
    if (!attr) return EINVAL;
    attr->priority     = 5;
    attr->stack_size   = 1024;
    attr->detach_state = PTHREAD_CREATE_JOINABLE;
    return 0;
}

int pthread_attr_destroy(pthread_attr_t *attr) {
    (void)attr;
    return 0;
}

int pthread_attr_setschedparam(pthread_attr_t *attr,
                               const struct sched_param *param) {
    if (!attr || !param) return EINVAL;
    attr->priority = (uint8_t)param->sched_priority;
    return 0;
}

int pthread_attr_getschedparam(const pthread_attr_t *attr,
                               struct sched_param *param) {
    if (!attr || !param) return EINVAL;
    param->sched_priority = attr->priority;
    return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize) {
    if (!attr || stacksize < PTHREAD_STACK_MIN) return EINVAL;
    attr->stack_size = (uint32_t)stacksize;
    return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize) {
    if (!attr || !stacksize) return EINVAL;
    *stacksize = attr->stack_size;
    return 0;
}

/* ============================================================
 * pthread_create / join / exit / self / yield
 * ============================================================ */

#ifndef EAGAIN
#define EAGAIN  11
#endif
#ifndef EINVAL
#define EINVAL  22
#endif
#ifndef ESRCH
#define ESRCH    3
#endif
#ifndef EDEADLK
#define EDEADLK 35
#endif

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start)(void *), void *arg) {
    if (!thread || !start) return EINVAL;

    posix_threads_init_once();

    int slot = posix_thread_alloc();
    if (slot < 0) return EAGAIN;

    posix_thread_entry_t *entry = &posix_thread_table[slot];

    /* Create join semaphore (initial=0, max=1) */
    if (eos_sem_create(&entry->join_sem, 0, 1) != EOS_KERN_OK) {
        return EAGAIN;
    }

    uint8_t prio       = attr ? attr->priority   : 5;
    uint32_t stack_sz  = attr ? attr->stack_size  : 1024;
    bool detached      = attr ? (attr->detach_state == PTHREAD_CREATE_DETACHED)
                              : false;

    entry->start_routine = start;
    entry->arg           = arg;
    entry->retval        = NULL;
    entry->detached      = detached;
    entry->active        = true;

    int handle = eos_task_create("pthread", posix_thread_wrapper,
                                (void *)(uintptr_t)slot, prio, stack_sz);
    if (handle < 0) {
        entry->active = false;
        eos_sem_delete(entry->join_sem);
        return EAGAIN;
    }

    entry->eos_handle = (eos_task_handle_t)handle;
    *thread = (pthread_t)slot;
    return 0;
}

int pthread_join(pthread_t thread, void **retval) {
    posix_threads_init_once();

    if (thread >= EOS_MAX_TASKS || !posix_thread_table[thread].active) {
        return ESRCH;
    }

    posix_thread_entry_t *entry = &posix_thread_table[thread];
    if (entry->detached) return EINVAL;

    /* Block until thread signals completion */
    int rc = eos_sem_wait(entry->join_sem, EOS_WAIT_FOREVER);
    if (rc != EOS_KERN_OK) return ESRCH;

    if (retval) *retval = entry->retval;

    /* Clean up */
    eos_sem_delete(entry->join_sem);
    entry->active = false;
    return 0;
}

void pthread_exit(void *retval) {
    eos_task_handle_t cur = eos_task_get_current();

    for (int i = 0; i < EOS_MAX_TASKS; i++) {
        if (posix_thread_table[i].active &&
            posix_thread_table[i].eos_handle == cur) {
            posix_thread_table[i].retval = retval;
            eos_sem_post(posix_thread_table[i].join_sem);
            if (posix_thread_table[i].detached) {
                posix_thread_table[i].active = false;
                eos_sem_delete(posix_thread_table[i].join_sem);
            }
            break;
        }
    }

    eos_task_delete(cur);
    /* Should not return; kernel removes the task */
    for (;;) { eos_task_yield(); }
}

pthread_t pthread_self(void) {
    eos_task_handle_t cur = eos_task_get_current();
    for (int i = 0; i < EOS_MAX_TASKS; i++) {
        if (posix_thread_table[i].active &&
            posix_thread_table[i].eos_handle == cur) {
            return (pthread_t)i;
        }
    }
    return 0;
}

int pthread_yield(void) {
    eos_task_yield();
    return 0;
}
