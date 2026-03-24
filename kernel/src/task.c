/**
 * @file task.c
 * @brief EoS Kernel — Task management
 *
 * Implements cooperative/preemptive task scheduling with priority-based
 * round-robin. Tasks are allocated from a static pool.
 */

#include <eos/kernel.h>
#include <string.h>
#include <stdlib.h>

static eos_task_t task_pool[EOS_MAX_TASKS];
static uint8_t task_count = 0;
static uint8_t current_task = 0;
static bool kernel_running = false;
static volatile uint32_t kernel_ticks = 0;

int eos_kernel_init(void)
{
    memset(task_pool, 0, sizeof(task_pool));
    task_count = 0;
    current_task = 0;
    kernel_running = false;
    kernel_ticks = 0;
    return EOS_KERN_OK;
}

void eos_kernel_start(void)
{
    if (task_count == 0) return;

    kernel_running = true;

    /* Start with the highest-priority ready task */
    uint8_t best = 0;
    uint8_t best_prio = 0;
    for (uint8_t i = 0; i < task_count; i++) {
        if (task_pool[i].state == EOS_TASK_READY && task_pool[i].priority > best_prio) {
            best = i;
            best_prio = task_pool[i].priority;
        }
    }

    current_task = best;
    task_pool[current_task].state = EOS_TASK_RUNNING;

    /* In a real implementation, this would set up the initial stack
     * frame and trigger a context switch via PendSV on Cortex-M */
    if (task_pool[current_task].entry) {
        task_pool[current_task].entry(task_pool[current_task].arg);
    }
}

bool eos_kernel_is_running(void)
{
    return kernel_running;
}

int eos_task_create(const char *name, eos_task_func_t entry, void *arg,
                     uint8_t priority, uint32_t stack_size)
{
    if (task_count >= EOS_MAX_TASKS) return EOS_KERN_NO_MEMORY;
    if (!entry) return EOS_KERN_INVALID;

    eos_task_t *t = &task_pool[task_count];
    t->id = task_count;
    t->name = name;
    t->state = EOS_TASK_READY;
    t->priority = priority;
    t->stack_size = stack_size;
    t->entry = entry;
    t->arg = arg;
    t->wake_tick = 0;
    t->run_count = 0;

    /* Allocate stack (in a real RTOS, this comes from a static pool or MPU region) */
    t->stack_base = (uint32_t *)malloc(stack_size);
    if (!t->stack_base) return EOS_KERN_NO_MEMORY;

    t->stack_ptr = t->stack_base + (stack_size / sizeof(uint32_t));

    task_count++;
    return (int)t->id;
}

int eos_task_delete(eos_task_handle_t handle)
{
    if (handle >= task_count) return EOS_KERN_INVALID;

    task_pool[handle].state = EOS_TASK_DELETED;
    if (task_pool[handle].stack_base) {
        free(task_pool[handle].stack_base);
        task_pool[handle].stack_base = NULL;
    }
    return EOS_KERN_OK;
}

int eos_task_suspend(eos_task_handle_t handle)
{
    if (handle >= task_count) return EOS_KERN_INVALID;
    task_pool[handle].state = EOS_TASK_SUSPENDED;
    return EOS_KERN_OK;
}

int eos_task_resume(eos_task_handle_t handle)
{
    if (handle >= task_count) return EOS_KERN_INVALID;
    if (task_pool[handle].state == EOS_TASK_SUSPENDED) {
        task_pool[handle].state = EOS_TASK_READY;
    }
    return EOS_KERN_OK;
}

void eos_task_yield(void)
{
    if (!kernel_running) return;

    task_pool[current_task].state = EOS_TASK_READY;

    /* Simple round-robin: find next ready task */
    uint8_t next = (current_task + 1) % task_count;
    for (uint8_t i = 0; i < task_count; i++) {
        uint8_t idx = (current_task + 1 + i) % task_count;
        if (task_pool[idx].state == EOS_TASK_READY) {
            next = idx;
            break;
        }
    }

    current_task = next;
    task_pool[current_task].state = EOS_TASK_RUNNING;
    task_pool[current_task].run_count++;

    /* In a real RTOS, trigger PendSV for context switch */
}

void eos_task_delay_ms(uint32_t ms)
{
    if (!kernel_running) return;

    task_pool[current_task].wake_tick = kernel_ticks + ms;
    task_pool[current_task].state = EOS_TASK_BLOCKED;

    eos_task_yield();
}

eos_task_handle_t eos_task_get_current(void)
{
    return current_task;
}

eos_task_state_t eos_task_get_state(eos_task_handle_t handle)
{
    if (handle >= task_count) return EOS_TASK_DELETED;
    return task_pool[handle].state;
}

const char *eos_task_get_name(eos_task_handle_t handle)
{
    if (handle >= task_count) return NULL;
    return task_pool[handle].name;
}

void eos_kernel_tick(void)
{
    kernel_ticks++;

    /* Wake up tasks whose delay has expired */
    for (uint8_t i = 0; i < task_count; i++) {
        if (task_pool[i].state == EOS_TASK_BLOCKED &&
            kernel_ticks >= task_pool[i].wake_tick) {
            task_pool[i].state = EOS_TASK_READY;
        }
    }
}
