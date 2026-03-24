/**
 * @file sync.c
 * @brief EoS Kernel — Synchronization primitives (mutex, semaphore)
 */

#include <eos/kernel.h>
#include <string.h>

/* ============================================================
 * Mutex
 * ============================================================ */

typedef struct {
    bool used;
    bool locked;
    eos_task_handle_t owner;
} eos_mutex_internal_t;

static eos_mutex_internal_t mutex_pool[EOS_MAX_MUTEXES];
static uint8_t mutex_count = 0;

int eos_mutex_create(eos_mutex_handle_t *out)
{
    if (!out) return EOS_KERN_INVALID;
    if (mutex_count >= EOS_MAX_MUTEXES) return EOS_KERN_NO_MEMORY;

    uint8_t idx = mutex_count++;
    mutex_pool[idx].used = true;
    mutex_pool[idx].locked = false;
    mutex_pool[idx].owner = 0xFF;
    *out = idx;
    return EOS_KERN_OK;
}

int eos_mutex_lock(eos_mutex_handle_t handle, uint32_t timeout_ms)
{
    if (handle >= EOS_MAX_MUTEXES || !mutex_pool[handle].used) {
        return EOS_KERN_INVALID;
    }

    (void)timeout_ms;

    if (mutex_pool[handle].locked) {
        /* In a real RTOS, block the current task and add to wait queue */
        return EOS_KERN_TIMEOUT;
    }

    mutex_pool[handle].locked = true;
    mutex_pool[handle].owner = eos_task_get_current();
    return EOS_KERN_OK;
}

int eos_mutex_unlock(eos_mutex_handle_t handle)
{
    if (handle >= EOS_MAX_MUTEXES || !mutex_pool[handle].used) {
        return EOS_KERN_INVALID;
    }

    if (!mutex_pool[handle].locked) return EOS_KERN_ERR;
    if (mutex_pool[handle].owner != eos_task_get_current()) return EOS_KERN_ERR;

    mutex_pool[handle].locked = false;
    mutex_pool[handle].owner = 0xFF;

    /* In a real RTOS, wake the highest-priority waiting task */
    return EOS_KERN_OK;
}

int eos_mutex_delete(eos_mutex_handle_t handle)
{
    if (handle >= EOS_MAX_MUTEXES || !mutex_pool[handle].used) {
        return EOS_KERN_INVALID;
    }

    mutex_pool[handle].used = false;
    mutex_pool[handle].locked = false;
    return EOS_KERN_OK;
}

/* ============================================================
 * Semaphore
 * ============================================================ */

typedef struct {
    bool used;
    uint32_t count;
    uint32_t max;
} eos_sem_internal_t;

static eos_sem_internal_t sem_pool[EOS_MAX_SEMAPHORES];
static uint8_t sem_count = 0;

int eos_sem_create(eos_sem_handle_t *out, uint32_t initial, uint32_t max)
{
    if (!out || max == 0 || initial > max) return EOS_KERN_INVALID;
    if (sem_count >= EOS_MAX_SEMAPHORES) return EOS_KERN_NO_MEMORY;

    uint8_t idx = sem_count++;
    sem_pool[idx].used = true;
    sem_pool[idx].count = initial;
    sem_pool[idx].max = max;
    *out = idx;
    return EOS_KERN_OK;
}

int eos_sem_wait(eos_sem_handle_t handle, uint32_t timeout_ms)
{
    if (handle >= EOS_MAX_SEMAPHORES || !sem_pool[handle].used) {
        return EOS_KERN_INVALID;
    }

    (void)timeout_ms;

    if (sem_pool[handle].count == 0) {
        /* In a real RTOS, block the task and add to wait queue */
        return EOS_KERN_TIMEOUT;
    }

    sem_pool[handle].count--;
    return EOS_KERN_OK;
}

int eos_sem_post(eos_sem_handle_t handle)
{
    if (handle >= EOS_MAX_SEMAPHORES || !sem_pool[handle].used) {
        return EOS_KERN_INVALID;
    }

    if (sem_pool[handle].count >= sem_pool[handle].max) {
        return EOS_KERN_FULL;
    }

    sem_pool[handle].count++;

    /* In a real RTOS, wake the highest-priority waiting task */
    return EOS_KERN_OK;
}

int eos_sem_delete(eos_sem_handle_t handle)
{
    if (handle >= EOS_MAX_SEMAPHORES || !sem_pool[handle].used) {
        return EOS_KERN_INVALID;
    }

    sem_pool[handle].used = false;
    return EOS_KERN_OK;
}

uint32_t eos_sem_get_count(eos_sem_handle_t handle)
{
    if (handle >= EOS_MAX_SEMAPHORES || !sem_pool[handle].used) {
        return 0;
    }
    return sem_pool[handle].count;
}
