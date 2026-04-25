# Chapter 6: RTOS Kernel

*Srikanth Patchava & EmbeddedOS Contributors*

---

## 6.1 Overview


![Figure: EoS Kernel Internals — scheduler, IPC, memory management, sync primitives](images/kernel-internals.png)

The EoS kernel is a lightweight, preemptive real-time operating system designed for
resource-constrained embedded systems. It provides:

- **Task management** with priority-based preemptive scheduling
- **Mutexes** for mutual exclusion with priority inheritance
- **Counting semaphores** for resource counting and signaling
- **Message queues** for inter-task communication
- **Software timers** for deferred execution

All kernel objects use lightweight 8-bit handles for minimal memory overhead.

### Kernel Configuration Defaults

| Macro | Default | Description |
|-------|---------|-------------|
| `EOS_MAX_TASKS` | 16 | Maximum concurrent tasks |
| `EOS_MAX_MUTEXES` | 8 | Maximum mutexes |
| `EOS_MAX_SEMAPHORES` | 8 | Maximum semaphores |
| `EOS_MAX_QUEUES` | 8 | Maximum message queues |
| `EOS_MAX_TIMERS` | 8 | Maximum software timers |
| `EOS_WAIT_FOREVER` | `0xFFFFFFFF` | Infinite timeout |
| `EOS_NO_WAIT` | `0` | Non-blocking operation |

### Return Codes

| Code | Value | Meaning |
|------|-------|---------|
| `EOS_KERN_OK` | 0 | Success |
| `EOS_KERN_ERR` | -1 | Generic error |
| `EOS_KERN_TIMEOUT` | -2 | Operation timed out |
| `EOS_KERN_FULL` | -3 | Queue/pool is full |
| `EOS_KERN_EMPTY` | -4 | Queue/pool is empty |
| `EOS_KERN_INVALID` | -5 | Invalid parameter |
| `EOS_KERN_NO_MEMORY` | -6 | Out of memory |

## 6.2 Kernel Lifecycle

```c
#include <eos/kernel.h>

int main(void)
{
    eos_hal_init();
    eos_kernel_init();

    // Create tasks, mutexes, queues, etc.
    eos_task_create("main_task", main_task_fn, NULL, 5, 1024);

    // Start the scheduler — this never returns
    eos_kernel_start();

    return 0; // Never reached
}
```

### Kernel API

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_kernel_init` | `int eos_kernel_init(void)` | Initialize kernel data structures |
| `eos_kernel_start` | `void eos_kernel_start(void)` | Start scheduler (does not return) |
| `eos_kernel_is_running` | `bool eos_kernel_is_running(void)` | Check if scheduler is active |
| `eos_kernel_tick` | `void eos_kernel_tick(void)` | Called from timer ISR |

## 6.3 Task Management

Tasks are the fundamental unit of execution. Each task has its own stack, priority
level, and entry function.

### Task States

```
        ┌──────────┐
   ┌───→│  READY   │◄──────────────────────┐
   │    └────┬─────┘                        │
   │         │ scheduled                    │ event / timeout
   │    ┌────▼─────┐                   ┌────┴─────┐
   │    │ RUNNING  │──── wait/block ──→│ BLOCKED  │
   │    └────┬─────┘                   └──────────┘
   │         │ yield / preempt
   │    ┌────▼─────┐
   │    │SUSPENDED │
   │    └────┬─────┘
   │         │ resume
   │         └──────────────────────────────┘
```

| State | Value | Description |
|-------|-------|-------------|
| `EOS_TASK_READY` | 0 | Eligible to run |
| `EOS_TASK_RUNNING` | 1 | Currently executing |
| `EOS_TASK_BLOCKED` | 2 | Waiting on resource |
| `EOS_TASK_SUSPENDED` | 3 | Manually suspended |
| `EOS_TASK_DELETED` | 4 | Marked for cleanup |

### Task API Reference

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_task_create` | `int eos_task_create(const char *name, eos_task_func_t entry, void *arg, uint8_t priority, uint32_t stack_size)` | Create a new task |
| `eos_task_delete` | `int eos_task_delete(eos_task_handle_t h)` | Delete a task |
| `eos_task_suspend` | `int eos_task_suspend(eos_task_handle_t h)` | Suspend a task |
| `eos_task_resume` | `int eos_task_resume(eos_task_handle_t h)` | Resume a suspended task |
| `eos_task_yield` | `void eos_task_yield(void)` | Yield to other ready tasks |
| `eos_task_delay_ms` | `void eos_task_delay_ms(uint32_t ms)` | Sleep for N milliseconds |
| `eos_task_get_current` | `eos_task_handle_t eos_task_get_current(void)` | Get current task handle |
| `eos_task_get_state` | `eos_task_state_t eos_task_get_state(eos_task_handle_t h)` | Query task state |
| `eos_task_get_name` | `const char *eos_task_get_name(eos_task_handle_t h)` | Get task name |

### Example: Multiple Tasks

```c
#include <eos/hal.h>
#include <eos/kernel.h>
#include <stdio.h>

void sensor_task(void *arg)
{
    while (1) {
        printf("[sensor] Reading sensor data...\n");
        eos_task_delay_ms(1000);
    }
}

void display_task(void *arg)
{
    while (1) {
        printf("[display] Updating display...\n");
        eos_task_delay_ms(500);
    }
}

void comms_task(void *arg)
{
    while (1) {
        printf("[comms] Sending data...\n");
        eos_task_delay_ms(2000);
    }
}

int main(void)
{
    eos_hal_init();
    eos_kernel_init();

    eos_task_create("sensor",  sensor_task,  NULL, 5, 1024);
    eos_task_create("display", display_task, NULL, 4, 1024);
    eos_task_create("comms",   comms_task,   NULL, 3, 2048);

    eos_kernel_start();
    return 0;
}
```

### Priority Guidelines

| Priority | Use Case |
|----------|----------|
| 0 (lowest) | Idle / background tasks |
| 1–3 | Low priority: logging, diagnostics |
| 4–6 | Normal: application logic, UI |
| 7–9 | High: communication, sensor sampling |
| 10+ | Critical: safety, motor control, ISR-driven |

## 6.4 Mutex

Mutexes provide mutual exclusion for protecting shared resources.

### Mutex API

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_mutex_create` | `int eos_mutex_create(eos_mutex_handle_t *out)` | Create mutex |
| `eos_mutex_lock` | `int eos_mutex_lock(eos_mutex_handle_t h, uint32_t timeout_ms)` | Acquire lock |
| `eos_mutex_unlock` | `int eos_mutex_unlock(eos_mutex_handle_t h)` | Release lock |
| `eos_mutex_delete` | `int eos_mutex_delete(eos_mutex_handle_t h)` | Destroy mutex |

### Example: Protected Shared Counter

```c
static eos_mutex_handle_t counter_lock;
static volatile uint32_t shared_counter = 0;

void increment_task(void *arg)
{
    while (1) {
        eos_mutex_lock(counter_lock, EOS_WAIT_FOREVER);
        shared_counter++;
        printf("Counter: %u\n", shared_counter);
        eos_mutex_unlock(counter_lock);
        eos_task_delay_ms(100);
    }
}

int main(void)
{
    eos_hal_init();
    eos_kernel_init();

    eos_mutex_create(&counter_lock);
    eos_task_create("inc1", increment_task, NULL, 5, 512);
    eos_task_create("inc2", increment_task, NULL, 5, 512);

    eos_kernel_start();
    return 0;
}
```

## 6.5 Semaphore

Counting semaphores are used for resource counting and task synchronization.

### Semaphore API

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_sem_create` | `int eos_sem_create(eos_sem_handle_t *out, uint32_t initial, uint32_t max)` | Create semaphore |
| `eos_sem_wait` | `int eos_sem_wait(eos_sem_handle_t h, uint32_t timeout_ms)` | Decrement (block if 0) |
| `eos_sem_post` | `int eos_sem_post(eos_sem_handle_t h)` | Increment |
| `eos_sem_delete` | `int eos_sem_delete(eos_sem_handle_t h)` | Destroy semaphore |
| `eos_sem_get_count` | `uint32_t eos_sem_get_count(eos_sem_handle_t h)` | Current count |

### Example: Producer-Consumer with Semaphore

```c
static eos_sem_handle_t data_ready;
static int sensor_value = 0;

void producer(void *arg)
{
    while (1) {
        sensor_value = eos_adc_read(0);   // Read sensor
        eos_sem_post(data_ready);          // Signal consumer
        eos_task_delay_ms(100);
    }
}

void consumer(void *arg)
{
    while (1) {
        eos_sem_wait(data_ready, EOS_WAIT_FOREVER);
        printf("Sensor: %d\n", sensor_value);
    }
}

int main(void)
{
    eos_hal_init();
    eos_kernel_init();

    eos_sem_create(&data_ready, 0, 1);  // Binary semaphore
    eos_task_create("producer", producer, NULL, 6, 512);
    eos_task_create("consumer", consumer, NULL, 5, 512);

    eos_kernel_start();
    return 0;
}
```

## 6.6 Message Queues

Message queues enable type-safe inter-task communication by passing fixed-size data
items between tasks.

### Queue API

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_queue_create` | `int eos_queue_create(eos_queue_handle_t *out, size_t item_size, uint32_t capacity)` | Create queue |
| `eos_queue_send` | `int eos_queue_send(eos_queue_handle_t h, const void *item, uint32_t timeout_ms)` | Enqueue item |
| `eos_queue_receive` | `int eos_queue_receive(eos_queue_handle_t h, void *item, uint32_t timeout_ms)` | Dequeue item |
| `eos_queue_peek` | `int eos_queue_peek(eos_queue_handle_t h, void *item)` | Peek without removing |
| `eos_queue_delete` | `int eos_queue_delete(eos_queue_handle_t h)` | Destroy queue |
| `eos_queue_count` | `uint32_t eos_queue_count(eos_queue_handle_t h)` | Items in queue |
| `eos_queue_is_full` | `bool eos_queue_is_full(eos_queue_handle_t h)` | Check if full |
| `eos_queue_is_empty` | `bool eos_queue_is_empty(eos_queue_handle_t h)` | Check if empty |

### Example: Sensor Data Pipeline

```c
typedef struct {
    uint16_t sensor_id;
    int32_t  value;
    uint32_t timestamp;
} sensor_event_t;

static eos_queue_handle_t event_queue;

void sensor_task(void *arg)
{
    while (1) {
        sensor_event_t evt = {
            .sensor_id = 1,
            .value     = eos_adc_read(0),
            .timestamp = eos_get_tick_ms(),
        };
        eos_queue_send(event_queue, &evt, EOS_WAIT_FOREVER);
        eos_task_delay_ms(100);
    }
}

void logger_task(void *arg)
{
    sensor_event_t evt;
    while (1) {
        if (eos_queue_receive(event_queue, &evt, 5000) == EOS_KERN_OK) {
            printf("[%u] Sensor %u = %d\n",
                   evt.timestamp, evt.sensor_id, evt.value);
        } else {
            printf("No data for 5 seconds\n");
        }
    }
}

int main(void)
{
    eos_hal_init();
    eos_kernel_init();

    eos_queue_create(&event_queue, sizeof(sensor_event_t), 16);
    eos_task_create("sensor", sensor_task, NULL, 6, 1024);
    eos_task_create("logger", logger_task, NULL, 4, 1024);

    eos_kernel_start();
    return 0;
}
```

## 6.7 Software Timers

Software timers execute callbacks at specified intervals without requiring a dedicated
task or hardware timer.

### Timer API

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_swtimer_create` | `int eos_swtimer_create(eos_swtimer_handle_t *out, const char *name, uint32_t period_ms, bool auto_reload, eos_swtimer_callback_t cb, void *ctx)` | Create timer |
| `eos_swtimer_start` | `int eos_swtimer_start(eos_swtimer_handle_t h)` | Start timer |
| `eos_swtimer_stop` | `int eos_swtimer_stop(eos_swtimer_handle_t h)` | Stop timer |
| `eos_swtimer_reset` | `int eos_swtimer_reset(eos_swtimer_handle_t h)` | Reset period |
| `eos_swtimer_delete` | `int eos_swtimer_delete(eos_swtimer_handle_t h)` | Destroy timer |

### Example: Watchdog Feed Timer

```c
void wdt_feed_cb(eos_swtimer_handle_t h, void *ctx)
{
    eos_wdt_feed();
    printf("Watchdog fed at %u ms\n", eos_get_tick_ms());
}

int main(void)
{
    eos_hal_init();
    eos_kernel_init();

    eos_swtimer_handle_t wdt_timer;
    eos_swtimer_create(&wdt_timer, "wdt_feed", 500, true,
                        wdt_feed_cb, NULL);
    eos_swtimer_start(wdt_timer);

    eos_kernel_start();
    return 0;
}
```

## 6.8 Common Patterns

### Pattern: Timeout with Retry

```c
int send_with_retry(eos_queue_handle_t q, const void *item, int max_retries)
{
    for (int i = 0; i < max_retries; i++) {
        int rc = eos_queue_send(q, item, 100);  // 100ms timeout
        if (rc == EOS_KERN_OK) return 0;
        if (rc != EOS_KERN_TIMEOUT) return rc;
    }
    return EOS_KERN_TIMEOUT;
}
```

### Pattern: Event Loop

```c
void app_task(void *arg)
{
    app_event_t evt;
    while (1) {
        if (eos_queue_receive(app_queue, &evt, EOS_WAIT_FOREVER) == EOS_KERN_OK) {
            switch (evt.type) {
            case EVT_BUTTON:  handle_button(&evt);  break;
            case EVT_SENSOR:  handle_sensor(&evt);  break;
            case EVT_TIMER:   handle_timer(&evt);   break;
            }
        }
    }
}
```

## 6.9 Summary

| Primitive | Use Case | Typical Capacity |
|-----------|----------|-----------------|
| **Task** | Independent execution unit | 16 max |
| **Mutex** | Protect shared resources | 8 max |
| **Semaphore** | Counting/signaling | 8 max |
| **Queue** | Inter-task data passing | 8 queues, variable depth |
| **SW Timer** | Deferred callbacks | 8 max |

---

*Next: [Chapter 7 — Multicore](ch07-multicore.md)*
