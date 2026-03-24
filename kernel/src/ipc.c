/**
 * @file ipc.c
 * @brief EoS Kernel — Inter-process communication (message queues, mailboxes)
 */

#include <eos/kernel.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================
 * Message Queue
 * ============================================================ */

typedef struct {
    bool used;
    uint8_t *buffer;
    size_t item_size;
    uint32_t capacity;
    uint32_t count;
    uint32_t head;
    uint32_t tail;
} eos_queue_internal_t;

static eos_queue_internal_t queue_pool[EOS_MAX_QUEUES];
static uint8_t queue_count = 0;

int eos_queue_create(eos_queue_handle_t *out, size_t item_size, uint32_t capacity)
{
    if (!out || item_size == 0 || capacity == 0) return EOS_KERN_INVALID;
    if (queue_count >= EOS_MAX_QUEUES) return EOS_KERN_NO_MEMORY;

    uint8_t idx = queue_count;
    eos_queue_internal_t *q = &queue_pool[idx];

    q->buffer = (uint8_t *)malloc(item_size * capacity);
    if (!q->buffer) return EOS_KERN_NO_MEMORY;

    q->used = true;
    q->item_size = item_size;
    q->capacity = capacity;
    q->count = 0;
    q->head = 0;
    q->tail = 0;

    queue_count++;
    *out = idx;
    return EOS_KERN_OK;
}

int eos_queue_send(eos_queue_handle_t handle, const void *item, uint32_t timeout_ms)
{
    if (handle >= EOS_MAX_QUEUES || !queue_pool[handle].used) {
        return EOS_KERN_INVALID;
    }

    eos_queue_internal_t *q = &queue_pool[handle];
    (void)timeout_ms;

    if (q->count >= q->capacity) {
        return EOS_KERN_FULL;
    }

    uint8_t *dst = q->buffer + (q->tail * q->item_size);
    memcpy(dst, item, q->item_size);
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;

    return EOS_KERN_OK;
}

int eos_queue_receive(eos_queue_handle_t handle, void *item, uint32_t timeout_ms)
{
    if (handle >= EOS_MAX_QUEUES || !queue_pool[handle].used) {
        return EOS_KERN_INVALID;
    }

    eos_queue_internal_t *q = &queue_pool[handle];
    (void)timeout_ms;

    if (q->count == 0) {
        return EOS_KERN_EMPTY;
    }

    uint8_t *src = q->buffer + (q->head * q->item_size);
    memcpy(item, src, q->item_size);
    q->head = (q->head + 1) % q->capacity;
    q->count--;

    return EOS_KERN_OK;
}

int eos_queue_peek(eos_queue_handle_t handle, void *item)
{
    if (handle >= EOS_MAX_QUEUES || !queue_pool[handle].used) {
        return EOS_KERN_INVALID;
    }

    eos_queue_internal_t *q = &queue_pool[handle];

    if (q->count == 0) {
        return EOS_KERN_EMPTY;
    }

    uint8_t *src = q->buffer + (q->head * q->item_size);
    memcpy(item, src, q->item_size);

    return EOS_KERN_OK;
}

int eos_queue_delete(eos_queue_handle_t handle)
{
    if (handle >= EOS_MAX_QUEUES || !queue_pool[handle].used) {
        return EOS_KERN_INVALID;
    }

    eos_queue_internal_t *q = &queue_pool[handle];

    if (q->buffer) {
        free(q->buffer);
        q->buffer = NULL;
    }

    q->used = false;
    return EOS_KERN_OK;
}

uint32_t eos_queue_count(eos_queue_handle_t handle)
{
    if (handle >= EOS_MAX_QUEUES || !queue_pool[handle].used) return 0;
    return queue_pool[handle].count;
}

bool eos_queue_is_full(eos_queue_handle_t handle)
{
    if (handle >= EOS_MAX_QUEUES || !queue_pool[handle].used) return true;
    return queue_pool[handle].count >= queue_pool[handle].capacity;
}

bool eos_queue_is_empty(eos_queue_handle_t handle)
{
    if (handle >= EOS_MAX_QUEUES || !queue_pool[handle].used) return true;
    return queue_pool[handle].count == 0;
}

/* ============================================================
 * Software Timer
 * ============================================================ */

typedef struct {
    bool used;
    bool running;
    const char *name;
    uint32_t period_ms;
    uint32_t next_tick;
    bool auto_reload;
    eos_swtimer_callback_t callback;
    void *ctx;
} eos_swtimer_internal_t;

static eos_swtimer_internal_t timer_pool[EOS_MAX_TIMERS];
static uint8_t swtimer_count = 0;

int eos_swtimer_create(eos_swtimer_handle_t *out, const char *name,
                        uint32_t period_ms, bool auto_reload,
                        eos_swtimer_callback_t callback, void *ctx)
{
    if (!out || !callback || period_ms == 0) return EOS_KERN_INVALID;
    if (swtimer_count >= EOS_MAX_TIMERS) return EOS_KERN_NO_MEMORY;

    uint8_t idx = swtimer_count++;
    eos_swtimer_internal_t *t = &timer_pool[idx];

    t->used = true;
    t->running = false;
    t->name = name;
    t->period_ms = period_ms;
    t->next_tick = 0;
    t->auto_reload = auto_reload;
    t->callback = callback;
    t->ctx = ctx;

    *out = idx;
    return EOS_KERN_OK;
}

int eos_swtimer_start(eos_swtimer_handle_t handle)
{
    if (handle >= EOS_MAX_TIMERS || !timer_pool[handle].used) {
        return EOS_KERN_INVALID;
    }

    timer_pool[handle].running = true;
    /* next_tick should be set based on current kernel tick */
    return EOS_KERN_OK;
}

int eos_swtimer_stop(eos_swtimer_handle_t handle)
{
    if (handle >= EOS_MAX_TIMERS || !timer_pool[handle].used) {
        return EOS_KERN_INVALID;
    }

    timer_pool[handle].running = false;
    return EOS_KERN_OK;
}

int eos_swtimer_reset(eos_swtimer_handle_t handle)
{
    if (handle >= EOS_MAX_TIMERS || !timer_pool[handle].used) {
        return EOS_KERN_INVALID;
    }

    /* Reset the timer period countdown */
    timer_pool[handle].running = true;
    return EOS_KERN_OK;
}

int eos_swtimer_delete(eos_swtimer_handle_t handle)
{
    if (handle >= EOS_MAX_TIMERS || !timer_pool[handle].used) {
        return EOS_KERN_INVALID;
    }

    timer_pool[handle].used = false;
    timer_pool[handle].running = false;
    return EOS_KERN_OK;
}
