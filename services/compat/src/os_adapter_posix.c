// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file os_adapter_posix.c
 * @brief POSIX host adapter — runs EoS services on Linux, macOS or WSL.
 *
 * `os_adapter.h` defines a complete OS abstraction and a registry that holds up
 * to EOS_MAX_OS_ADAPTERS entries. Until now exactly one was implemented, the
 * EoS native kernel, so the abstraction had never been exercised against
 * anything but itself. An interface with one implementation is a guess about
 * what varies.
 *
 * This is the second. It backs the same vtable with pthreads, POSIX semaphores
 * and CLOCK_MONOTONIC, so EoS services run as an ordinary process on a host
 * that already has an operating system — which is the arrangement most
 * evaluators start from, and the one the Compute profile in §21 describes.
 *
 * It also makes the EoS-native adapter falsifiable: the compliance test can now
 * run the identical sequence against both and require the same answers.
 *
 * Deliberate limits, none of them accidental:
 *
 *   - `irq_disable` / `irq_enable` are no-ops. A user-space process cannot mask
 *     interrupts, and pretending otherwise would hide the fact that code
 *     relying on them for mutual exclusion is unsafe here. Use a mutex.
 *   - `priority` is accepted and recorded but not applied. Real-time scheduling
 *     needs privileges the process usually lacks, and silently running
 *     SCHED_OTHER while reporting success would be worse than saying so.
 *   - `stack_size` is honoured through pthread_attr_setstacksize, clamped up to
 *     PTHREAD_STACK_MIN.
 */

#include "eos/os_adapter.h"

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>

#define POSIX_MAX_TASKS   16
#define POSIX_MAX_MUTEXES 16
#define POSIX_MAX_SEMS    16
#define POSIX_MAX_QUEUES  8
#define POSIX_MAX_TIMERS  8
#define POSIX_NAME_MAX    32

/* ============================================================
 * Time helpers
 * ============================================================ */

static void ms_to_abstime(uint32_t ms, struct timespec *out)
{
    clock_gettime(CLOCK_REALTIME, out);
    out->tv_sec += (time_t)(ms / 1000u);
    out->tv_nsec += (long)((ms % 1000u) * 1000000L);
    if (out->tv_nsec >= 1000000000L) {
        out->tv_sec += 1;
        out->tv_nsec -= 1000000000L;
    }
}

static void sleep_ms(uint32_t ms)
{
    struct timespec ts = {
        .tv_sec = (time_t)(ms / 1000u),
        .tv_nsec = (long)((ms % 1000u) * 1000000L),
    };
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
        /* Interrupted by a signal — finish the remaining interval. */
    }
}

/* ============================================================
 * Tasks
 * ============================================================ */

typedef struct {
    bool                 in_use;
    char                 name[POSIX_NAME_MAX];
    pthread_t            thread;
    eos_osa_task_func_t  entry;
    void                *arg;
    uint8_t              priority;   /* recorded, not applied — see file header */

    /* Suspend gate. POSIX has no thread suspend, so a suspended task blocks on
     * its own condvar the next time it reaches a yield or delay.
     *
     * `release` only frees a task parked at the gate; it never cancels one. A
     * task that has not reached the gate yet still runs its entry function.
     * Conflating the two would make task_delete race task creation and skip the
     * body entirely. */
    pthread_mutex_t      gate_lock;
    pthread_cond_t       gate_cond;
    bool                 suspended;
    bool                 release;
} posix_task_t;

static posix_task_t   g_tasks[POSIX_MAX_TASKS];
static pthread_mutex_t g_tasks_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_key_t   g_self_key;
static pthread_once_t  g_self_key_once = PTHREAD_ONCE_INIT;

static void make_self_key(void)
{
    pthread_key_create(&g_self_key, NULL);
}

/* Block here while suspended. Called from yield and delay, which is where a
 * cooperative task is expected to be interruptible. */
static void gate_check(posix_task_t *t)
{
    pthread_mutex_lock(&t->gate_lock);
    while (t->suspended && !t->release) {
        pthread_cond_wait(&t->gate_cond, &t->gate_lock);
    }
    pthread_mutex_unlock(&t->gate_lock);
}

static void *task_trampoline(void *raw)
{
    posix_task_t *t = (posix_task_t *)raw;

    pthread_once(&g_self_key_once, make_self_key);
    pthread_setspecific(g_self_key, t);

    gate_check(t);
    if (t->entry) {
        t->entry(t->arg);
    }
    return NULL;
}

static posix_task_t *current_task(void)
{
    pthread_once(&g_self_key_once, make_self_key);
    return (posix_task_t *)pthread_getspecific(g_self_key);
}

static int posix_task_create(const char *name, eos_osa_task_func_t entry, void *arg,
                             uint8_t priority, uint32_t stack_size)
{
    if (!entry) return -1;

    pthread_mutex_lock(&g_tasks_lock);

    int slot = -1;
    for (int i = 0; i < POSIX_MAX_TASKS; i++) {
        if (!g_tasks[i].in_use) { slot = i; break; }
    }
    if (slot < 0) {
        pthread_mutex_unlock(&g_tasks_lock);
        return -1;
    }

    posix_task_t *t = &g_tasks[slot];
    memset(t, 0, sizeof(*t));
    t->in_use = true;
    t->entry = entry;
    t->arg = arg;
    t->priority = priority;
    if (name) {
        strncpy(t->name, name, POSIX_NAME_MAX - 1);
    }
    pthread_mutex_init(&t->gate_lock, NULL);
    pthread_cond_init(&t->gate_cond, NULL);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if (stack_size > 0) {
        size_t want = stack_size;
        if (want < (size_t)PTHREAD_STACK_MIN) {
            want = (size_t)PTHREAD_STACK_MIN;
        }
        pthread_attr_setstacksize(&attr, want);
    }

    const int rc = pthread_create(&t->thread, &attr, task_trampoline, t);
    pthread_attr_destroy(&attr);

    if (rc != 0) {
        pthread_mutex_destroy(&t->gate_lock);
        pthread_cond_destroy(&t->gate_cond);
        t->in_use = false;
        pthread_mutex_unlock(&g_tasks_lock);
        return -1;
    }

    pthread_mutex_unlock(&g_tasks_lock);
    return slot;
}

static bool task_valid(uint8_t handle)
{
    return handle < POSIX_MAX_TASKS && g_tasks[handle].in_use;
}

static int posix_task_delete(uint8_t handle)
{
    if (!task_valid(handle)) return -1;
    posix_task_t *t = &g_tasks[handle];

    /* Release the task if it is parked at the suspend gate, then wait for it.
     *
     * There is no safe way to kill a POSIX thread: pthread_cancel leaves any
     * mutex it holds locked forever. So delete means "let it finish and reclaim
     * the slot", and the entry function is required to return. A task that
     * loops forever will hang this call — which is the truth about threads on a
     * hosted OS, and better said plainly than papered over with a cancel that
     * corrupts the process. */
    pthread_mutex_lock(&t->gate_lock);
    t->release = true;
    t->suspended = false;
    pthread_cond_broadcast(&t->gate_cond);
    pthread_mutex_unlock(&t->gate_lock);

    pthread_join(t->thread, NULL);

    pthread_mutex_destroy(&t->gate_lock);
    pthread_cond_destroy(&t->gate_cond);

    pthread_mutex_lock(&g_tasks_lock);
    t->in_use = false;
    pthread_mutex_unlock(&g_tasks_lock);
    return 0;
}

static int posix_task_suspend(uint8_t handle)
{
    if (!task_valid(handle)) return -1;
    posix_task_t *t = &g_tasks[handle];
    pthread_mutex_lock(&t->gate_lock);
    t->suspended = true;
    pthread_mutex_unlock(&t->gate_lock);
    return 0;
}

static int posix_task_resume(uint8_t handle)
{
    if (!task_valid(handle)) return -1;
    posix_task_t *t = &g_tasks[handle];
    pthread_mutex_lock(&t->gate_lock);
    t->suspended = false;
    pthread_cond_broadcast(&t->gate_cond);
    pthread_mutex_unlock(&t->gate_lock);
    return 0;
}

static void posix_task_yield(void)
{
    posix_task_t *t = current_task();
    if (t) gate_check(t);
    sched_yield();
}

static void posix_task_delay_ms(uint32_t ms)
{
    posix_task_t *t = current_task();
    if (t) gate_check(t);
    sleep_ms(ms);
}

static uint8_t posix_task_get_current(void)
{
    const posix_task_t *t = current_task();
    if (!t) return 0xFFu;
    return (uint8_t)(t - g_tasks);
}

static const char *posix_task_get_name(uint8_t handle)
{
    if (!task_valid(handle)) return "invalid";
    return g_tasks[handle].name;
}

/* ============================================================
 * Mutexes
 * ============================================================ */

typedef struct {
    bool            in_use;
    pthread_mutex_t lock;
} posix_mutex_t;

static posix_mutex_t   g_mutexes[POSIX_MAX_MUTEXES];
static pthread_mutex_t g_mutexes_lock = PTHREAD_MUTEX_INITIALIZER;

static int posix_mutex_create(uint8_t *out)
{
    if (!out) return -1;
    pthread_mutex_lock(&g_mutexes_lock);
    for (int i = 0; i < POSIX_MAX_MUTEXES; i++) {
        if (!g_mutexes[i].in_use) {
            pthread_mutexattr_t attr;
            pthread_mutexattr_init(&attr);
            /* Recursive, to match the EoS mutex, which counts nesting. */
            pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
            pthread_mutex_init(&g_mutexes[i].lock, &attr);
            pthread_mutexattr_destroy(&attr);
            g_mutexes[i].in_use = true;
            *out = (uint8_t)i;
            pthread_mutex_unlock(&g_mutexes_lock);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_mutexes_lock);
    return -1;
}

static int posix_mutex_lock_fn(uint8_t handle, uint32_t timeout_ms)
{
    if (handle >= POSIX_MAX_MUTEXES || !g_mutexes[handle].in_use) return -1;
    pthread_mutex_t *m = &g_mutexes[handle].lock;

    if (timeout_ms == 0u) {
        return pthread_mutex_trylock(m) == 0 ? 0 : -1;
    }
    if (timeout_ms == EOS_OSA_WAIT_FOREVER) {
        return pthread_mutex_lock(m) == 0 ? 0 : -1;
    }

    struct timespec deadline;
    ms_to_abstime(timeout_ms, &deadline);
    return pthread_mutex_timedlock(m, &deadline) == 0 ? 0 : -1;
}

static int posix_mutex_unlock(uint8_t handle)
{
    if (handle >= POSIX_MAX_MUTEXES || !g_mutexes[handle].in_use) return -1;
    return pthread_mutex_unlock(&g_mutexes[handle].lock) == 0 ? 0 : -1;
}

static int posix_mutex_delete(uint8_t handle)
{
    if (handle >= POSIX_MAX_MUTEXES || !g_mutexes[handle].in_use) return -1;
    pthread_mutex_lock(&g_mutexes_lock);
    pthread_mutex_destroy(&g_mutexes[handle].lock);
    g_mutexes[handle].in_use = false;
    pthread_mutex_unlock(&g_mutexes_lock);
    return 0;
}

/* ============================================================
 * Semaphores
 * ============================================================ */

typedef struct {
    bool     in_use;
    sem_t    sem;
    uint32_t max;
} posix_sem_t;

static posix_sem_t     g_sems[POSIX_MAX_SEMS];
static pthread_mutex_t g_sems_lock = PTHREAD_MUTEX_INITIALIZER;

static int posix_sem_create(uint8_t *out, uint32_t initial, uint32_t max)
{
    if (!out || max == 0u || initial > max) return -1;
    pthread_mutex_lock(&g_sems_lock);
    for (int i = 0; i < POSIX_MAX_SEMS; i++) {
        if (!g_sems[i].in_use) {
            if (sem_init(&g_sems[i].sem, 0, (unsigned)initial) != 0) {
                pthread_mutex_unlock(&g_sems_lock);
                return -1;
            }
            g_sems[i].in_use = true;
            g_sems[i].max = max;
            *out = (uint8_t)i;
            pthread_mutex_unlock(&g_sems_lock);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_sems_lock);
    return -1;
}

static int posix_sem_wait(uint8_t handle, uint32_t timeout_ms)
{
    if (handle >= POSIX_MAX_SEMS || !g_sems[handle].in_use) return -1;
    sem_t *s = &g_sems[handle].sem;

    if (timeout_ms == 0u) {
        return sem_trywait(s) == 0 ? 0 : -1;
    }
    if (timeout_ms == EOS_OSA_WAIT_FOREVER) {
        int rc;
        while ((rc = sem_wait(s)) == -1 && errno == EINTR) { }
        return rc == 0 ? 0 : -1;
    }

    struct timespec deadline;
    ms_to_abstime(timeout_ms, &deadline);
    int rc;
    while ((rc = sem_timedwait(s, &deadline)) == -1 && errno == EINTR) { }
    return rc == 0 ? 0 : -1;
}

static int posix_sem_post(uint8_t handle)
{
    if (handle >= POSIX_MAX_SEMS || !g_sems[handle].in_use) return -1;

    /* Enforce the ceiling the caller asked for. POSIX semaphores have no
     * maximum, so without this the counting invariant the EoS semaphore
     * guarantees would silently not hold on this backend. */
    int value = 0;
    if (sem_getvalue(&g_sems[handle].sem, &value) == 0 &&
        (uint32_t)value >= g_sems[handle].max) {
        return -1;
    }
    return sem_post(&g_sems[handle].sem) == 0 ? 0 : -1;
}

static int posix_sem_delete(uint8_t handle)
{
    if (handle >= POSIX_MAX_SEMS || !g_sems[handle].in_use) return -1;
    pthread_mutex_lock(&g_sems_lock);
    sem_destroy(&g_sems[handle].sem);
    g_sems[handle].in_use = false;
    pthread_mutex_unlock(&g_sems_lock);
    return 0;
}

static uint32_t posix_sem_get_count(uint8_t handle)
{
    if (handle >= POSIX_MAX_SEMS || !g_sems[handle].in_use) return 0;
    int value = 0;
    if (sem_getvalue(&g_sems[handle].sem, &value) != 0 || value < 0) return 0;
    return (uint32_t)value;
}

/* ============================================================
 * Message queues
 * ============================================================ */

typedef struct {
    bool            in_use;
    uint8_t        *storage;
    size_t          item_size;
    uint32_t        capacity;
    uint32_t        count;
    uint32_t        head;
    uint32_t        tail;
    pthread_mutex_t lock;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
} posix_queue_t;

static posix_queue_t   g_queues[POSIX_MAX_QUEUES];
static pthread_mutex_t g_queues_lock = PTHREAD_MUTEX_INITIALIZER;

static int posix_queue_create(uint8_t *out, size_t item_size, uint32_t capacity)
{
    if (!out || item_size == 0u || capacity == 0u) return -1;
    /* Reject a size that would wrap the allocation. */
    if (item_size > SIZE_MAX / capacity) return -1;

    pthread_mutex_lock(&g_queues_lock);
    for (int i = 0; i < POSIX_MAX_QUEUES; i++) {
        if (!g_queues[i].in_use) {
            posix_queue_t *q = &g_queues[i];
            q->storage = (uint8_t *)calloc(capacity, item_size);
            if (!q->storage) {
                pthread_mutex_unlock(&g_queues_lock);
                return -1;
            }
            q->item_size = item_size;
            q->capacity = capacity;
            q->count = 0;
            q->head = 0;
            q->tail = 0;
            pthread_mutex_init(&q->lock, NULL);
            pthread_cond_init(&q->not_empty, NULL);
            pthread_cond_init(&q->not_full, NULL);
            q->in_use = true;
            *out = (uint8_t)i;
            pthread_mutex_unlock(&g_queues_lock);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_queues_lock);
    return -1;
}

static bool queue_valid(uint8_t handle)
{
    return handle < POSIX_MAX_QUEUES && g_queues[handle].in_use;
}

static int posix_queue_send(uint8_t handle, const void *item, uint32_t timeout_ms)
{
    if (!queue_valid(handle) || !item) return -1;
    posix_queue_t *q = &g_queues[handle];

    pthread_mutex_lock(&q->lock);

    while (q->count == q->capacity) {
        if (timeout_ms == 0u) {
            pthread_mutex_unlock(&q->lock);
            return -1;
        }
        if (timeout_ms == EOS_OSA_WAIT_FOREVER) {
            pthread_cond_wait(&q->not_full, &q->lock);
            continue;
        }
        struct timespec deadline;
        ms_to_abstime(timeout_ms, &deadline);
        if (pthread_cond_timedwait(&q->not_full, &q->lock, &deadline) == ETIMEDOUT) {
            pthread_mutex_unlock(&q->lock);
            return -1;
        }
    }

    memcpy(q->storage + ((size_t)q->tail * q->item_size), item, q->item_size);
    q->tail = (q->tail + 1u) % q->capacity;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

static int posix_queue_receive(uint8_t handle, void *item, uint32_t timeout_ms)
{
    if (!queue_valid(handle) || !item) return -1;
    posix_queue_t *q = &g_queues[handle];

    pthread_mutex_lock(&q->lock);

    while (q->count == 0u) {
        if (timeout_ms == 0u) {
            pthread_mutex_unlock(&q->lock);
            return -1;
        }
        if (timeout_ms == EOS_OSA_WAIT_FOREVER) {
            pthread_cond_wait(&q->not_empty, &q->lock);
            continue;
        }
        struct timespec deadline;
        ms_to_abstime(timeout_ms, &deadline);
        if (pthread_cond_timedwait(&q->not_empty, &q->lock, &deadline) == ETIMEDOUT) {
            pthread_mutex_unlock(&q->lock);
            return -1;
        }
    }

    memcpy(item, q->storage + ((size_t)q->head * q->item_size), q->item_size);
    q->head = (q->head + 1u) % q->capacity;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

static int posix_queue_delete(uint8_t handle)
{
    if (!queue_valid(handle)) return -1;
    posix_queue_t *q = &g_queues[handle];

    pthread_mutex_lock(&g_queues_lock);
    free(q->storage);
    q->storage = NULL;
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
    q->in_use = false;
    pthread_mutex_unlock(&g_queues_lock);
    return 0;
}

static uint32_t posix_queue_count(uint8_t handle)
{
    if (!queue_valid(handle)) return 0;
    posix_queue_t *q = &g_queues[handle];
    pthread_mutex_lock(&q->lock);
    const uint32_t n = q->count;
    pthread_mutex_unlock(&q->lock);
    return n;
}

static bool posix_queue_is_full(uint8_t handle)
{
    if (!queue_valid(handle)) return false;
    return posix_queue_count(handle) == g_queues[handle].capacity;
}

static bool posix_queue_is_empty(uint8_t handle)
{
    if (!queue_valid(handle)) return true;
    return posix_queue_count(handle) == 0u;
}

/* ============================================================
 * Software timers
 * ============================================================ */

typedef struct {
    bool                in_use;
    bool                running;
    bool                stop;
    char                name[POSIX_NAME_MAX];
    uint32_t            period_ms;
    bool                auto_reload;
    eos_osa_timer_cb_t  callback;
    void               *ctx;
    uint8_t             handle;
    pthread_t           thread;
    pthread_mutex_t     lock;
    pthread_cond_t      cond;
} posix_timer_t;

static posix_timer_t   g_timers[POSIX_MAX_TIMERS];
static pthread_mutex_t g_timers_lock = PTHREAD_MUTEX_INITIALIZER;

/* One thread per timer. A single timer wheel would be less thread-hungry, but
 * POSIX_MAX_TIMERS is 8 and a wheel adds a scheduling policy this adapter has
 * no business inventing — the host kernel already has one. */
static void *timer_thread(void *raw)
{
    posix_timer_t *t = (posix_timer_t *)raw;

    for (;;) {
        pthread_mutex_lock(&t->lock);
        while (!t->running && !t->stop) {
            pthread_cond_wait(&t->cond, &t->lock);
        }
        if (t->stop) {
            pthread_mutex_unlock(&t->lock);
            return NULL;
        }

        struct timespec deadline;
        ms_to_abstime(t->period_ms, &deadline);
        const int rc = pthread_cond_timedwait(&t->cond, &t->lock, &deadline);

        if (t->stop) {
            pthread_mutex_unlock(&t->lock);
            return NULL;
        }
        const bool expired = (rc == ETIMEDOUT) && t->running;
        const bool reload = t->auto_reload;
        eos_osa_timer_cb_t cb = t->callback;
        void *ctx = t->ctx;
        const uint8_t handle = t->handle;
        if (expired && !reload) {
            t->running = false;
        }
        pthread_mutex_unlock(&t->lock);

        /* Called outside the lock: a callback that touches this timer must not
         * deadlock against it. */
        if (expired && cb) {
            cb(handle, ctx);
        }
    }
}

static int posix_timer_create(uint8_t *out, const char *name, uint32_t period_ms,
                              bool auto_reload, eos_osa_timer_cb_t callback, void *ctx)
{
    if (!out || !callback || period_ms == 0u) return -1;

    pthread_mutex_lock(&g_timers_lock);
    for (int i = 0; i < POSIX_MAX_TIMERS; i++) {
        if (!g_timers[i].in_use) {
            posix_timer_t *t = &g_timers[i];
            memset(t, 0, sizeof(*t));
            t->in_use = true;
            t->period_ms = period_ms;
            t->auto_reload = auto_reload;
            t->callback = callback;
            t->ctx = ctx;
            t->handle = (uint8_t)i;
            if (name) strncpy(t->name, name, POSIX_NAME_MAX - 1);
            pthread_mutex_init(&t->lock, NULL);
            pthread_cond_init(&t->cond, NULL);

            if (pthread_create(&t->thread, NULL, timer_thread, t) != 0) {
                pthread_mutex_destroy(&t->lock);
                pthread_cond_destroy(&t->cond);
                t->in_use = false;
                pthread_mutex_unlock(&g_timers_lock);
                return -1;
            }
            *out = (uint8_t)i;
            pthread_mutex_unlock(&g_timers_lock);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_timers_lock);
    return -1;
}

static bool timer_valid(uint8_t handle)
{
    return handle < POSIX_MAX_TIMERS && g_timers[handle].in_use;
}

static int posix_timer_start(uint8_t handle)
{
    if (!timer_valid(handle)) return -1;
    posix_timer_t *t = &g_timers[handle];
    pthread_mutex_lock(&t->lock);
    t->running = true;
    pthread_cond_signal(&t->cond);
    pthread_mutex_unlock(&t->lock);
    return 0;
}

static int posix_timer_stop(uint8_t handle)
{
    if (!timer_valid(handle)) return -1;
    posix_timer_t *t = &g_timers[handle];
    pthread_mutex_lock(&t->lock);
    t->running = false;
    pthread_cond_signal(&t->cond);
    pthread_mutex_unlock(&t->lock);
    return 0;
}

static int posix_timer_delete(uint8_t handle)
{
    if (!timer_valid(handle)) return -1;
    posix_timer_t *t = &g_timers[handle];

    pthread_mutex_lock(&t->lock);
    t->stop = true;
    t->running = false;
    pthread_cond_broadcast(&t->cond);
    pthread_mutex_unlock(&t->lock);

    pthread_join(t->thread, NULL);

    pthread_mutex_lock(&g_timers_lock);
    pthread_mutex_destroy(&t->lock);
    pthread_cond_destroy(&t->cond);
    t->in_use = false;
    pthread_mutex_unlock(&g_timers_lock);
    return 0;
}

/* ============================================================
 * System
 * ============================================================ */

static uint32_t posix_get_tick_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u);
}

static void posix_delay_ms(uint32_t ms)
{
    sleep_ms(ms);
}

/* A user-space process cannot mask interrupts. These are no-ops rather than
 * an approximation, because code that relies on them for mutual exclusion is
 * unsafe on this backend and should use a mutex. Silently substituting a global
 * lock would hide that. */
static void posix_irq_disable(void) { }
static void posix_irq_enable(void)  { }

/* ============================================================
 * Adapter vtable
 * ============================================================ */

const eos_os_adapter_t eos_posix_adapter = {
    .name = "posix",

    .task_create      = posix_task_create,
    .task_delete      = posix_task_delete,
    .task_suspend     = posix_task_suspend,
    .task_resume      = posix_task_resume,
    .task_yield       = posix_task_yield,
    .task_delay_ms    = posix_task_delay_ms,
    .task_get_current = posix_task_get_current,
    .task_get_name    = posix_task_get_name,

    .mutex_create = posix_mutex_create,
    .mutex_lock   = posix_mutex_lock_fn,
    .mutex_unlock = posix_mutex_unlock,
    .mutex_delete = posix_mutex_delete,

    .sem_create    = posix_sem_create,
    .sem_wait      = posix_sem_wait,
    .sem_post      = posix_sem_post,
    .sem_delete    = posix_sem_delete,
    .sem_get_count = posix_sem_get_count,

    .queue_create   = posix_queue_create,
    .queue_send     = posix_queue_send,
    .queue_receive  = posix_queue_receive,
    .queue_delete   = posix_queue_delete,
    .queue_count    = posix_queue_count,
    .queue_is_full  = posix_queue_is_full,
    .queue_is_empty = posix_queue_is_empty,

    .timer_create = posix_timer_create,
    .timer_start  = posix_timer_start,
    .timer_stop   = posix_timer_stop,
    .timer_delete = posix_timer_delete,

    .get_tick_ms = posix_get_tick_ms,
    .delay_ms    = posix_delay_ms,
    .irq_disable = posix_irq_disable,
    .irq_enable  = posix_irq_enable,

    /* NULL means malloc/free, which is what a hosted process should use. */
    .mem_alloc = NULL,
    .mem_free  = NULL,
};
