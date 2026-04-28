// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file task.c
 * @brief EoS kernel task scheduler with real context switching
 *
 * Priority-based preemptive scheduler with SysTick-driven tick,
 * PendSV context switching, software timers, and stack canary detection.
 */

#include "eos/kernel.h"
#include "eos/kernel_internal.h"
#include "eos/arch.h"
#include "eos/mem.h"
#include <string.h>

/* Stack canary value — placed at bottom of each task's stack */
#define EOS_STACK_CANARY    0xDEADBEEFU

/* Default idle task stack size */
#define IDLE_STACK_SIZE     256

/* ============================================================
 * Internal State
 * ============================================================ */

eos_task_t g_tasks[EOS_MAX_TASKS];
static int g_task_count = 0;
static int g_current = -1;
static int g_running = 0;

/* Tick counter — updated by SysTick_Handler */
volatile uint32_t g_tick = 0;

/* Reschedule flag */
volatile uint8_t g_needs_reschedule = 0;

/* Pointers used by the PendSV assembly handler */
uint32_t **g_current_sp = NULL;
uint32_t **g_next_sp = NULL;

/* Static heap for task stacks (used when eos_malloc is not available) */
#ifndef EOS_TASK_STACK_POOL_SIZE
#define EOS_TASK_STACK_POOL_SIZE  (EOS_MAX_TASKS * 1024)
#endif
static uint32_t g_stack_pool[EOS_TASK_STACK_POOL_SIZE / sizeof(uint32_t)];
static uint32_t g_stack_pool_offset = 0;

/* Idle task stack */
static uint32_t g_idle_stack[IDLE_STACK_SIZE / sizeof(uint32_t)];

/* ============================================================
 * Software Timers
 * ============================================================ */

typedef struct {
    uint8_t in_use;
    uint8_t active;
    uint8_t auto_reload;
    const char *name;
    uint32_t period_ticks;
    uint32_t next_tick;
    eos_swtimer_callback_t callback;
    void *ctx;
} swtimer_t;

static swtimer_t g_timers[EOS_MAX_TIMERS];

/* ============================================================
 * Stack Allocation
 * ============================================================ */

static uint32_t *alloc_task_stack(uint32_t size_bytes)
{
    uint32_t words = size_bytes / sizeof(uint32_t);
    if (g_stack_pool_offset + words > EOS_TASK_STACK_POOL_SIZE / sizeof(uint32_t)) {
        return NULL;  /* Pool exhausted */
    }
    uint32_t *base = &g_stack_pool[g_stack_pool_offset];
    g_stack_pool_offset += words;

    /* Place canary at the bottom of the stack */
    base[0] = EOS_STACK_CANARY;

    return base;
}

/* ============================================================
 * Scheduler
 * ============================================================ */

/**
 * @brief Find the highest-priority ready task.
 */
static int find_next_task(void)
{
    int best = -1;
    uint8_t best_prio = 255;
    int start = (g_current + 1) % EOS_MAX_TASKS;

    for (int i = 0; i < EOS_MAX_TASKS; i++) {
        int idx = (start + i) % EOS_MAX_TASKS;
        eos_task_t *t = &g_tasks[idx];
        if (t->state != EOS_TASK_READY) continue;
        if (t->priority < best_prio) {
            best_prio = t->priority;
            best = idx;
        }
    }
    return best;
}

/**
 * @brief Called by PendSV to select the next task.
 *
 * Updates g_current_sp and g_next_sp for the assembly handler.
 */
void eos_schedule(void)
{
    int next = find_next_task();
    if (next < 0) return;  /* No ready tasks — stay on current */

    /* Mark current task as ready (if it was running) */
    if (g_current >= 0 && g_tasks[g_current].state == EOS_TASK_RUNNING) {
        g_tasks[g_current].state = EOS_TASK_READY;
    }

    /* Check stack canary on current task */
    if (g_current >= 0 && g_tasks[g_current].stack_base) {
        if (g_tasks[g_current].stack_base[0] != EOS_STACK_CANARY) {
            /* Stack overflow detected! */
            /* In production: trigger fault handler or reboot */
            g_tasks[g_current].state = EOS_TASK_DELETED;
        }
    }

    /* Switch to next task */
    g_current_sp = &g_tasks[g_current >= 0 ? g_current : next].stack_ptr;
    g_current = next;
    g_tasks[next].state = EOS_TASK_RUNNING;
    g_tasks[next].run_count++;
    g_next_sp = &g_tasks[next].stack_ptr;
}

/**
 * @brief Wake blocked tasks whose delay has expired.
 * Called from SysTick_Handler.
 */
void eos_task_wake_check(uint32_t current_tick)
{
    for (int i = 0; i < EOS_MAX_TASKS; i++) {
        eos_task_t *t = &g_tasks[i];
        if (t->state == EOS_TASK_BLOCKED && t->wake_tick > 0 &&
            current_tick >= t->wake_tick) {
            t->state = EOS_TASK_READY;
            t->wake_tick = 0;
        }
    }
}

/* ============================================================
 * Idle Task
 * ============================================================ */

static void idle_task_func(void *arg)
{
    (void)arg;
    while (1) {
        /* WFI — wait for interrupt, saves power */
#if defined(__ARM_ARCH)
        __asm volatile ("wfi");
#elif defined(__x86_64__) || defined(_M_X64)
        __asm volatile ("hlt");
#elif defined(__riscv)
        __asm volatile ("wfi");
#else
        /* Host/simulation: just spin */
        volatile int dummy = 0;
        (void)dummy;
#endif
    }
}

/* ============================================================
 * Kernel API Implementation
 * ============================================================ */

int eos_kernel_init(void)
{
    memset(g_tasks, 0, sizeof(g_tasks));
    memset(g_timers, 0, sizeof(g_timers));
    g_task_count = 0;
    g_current = -1;
    g_running = 0;
    g_tick = 0;
    g_stack_pool_offset = 0;
    g_current_sp = NULL;
    g_next_sp = NULL;

    /* Create idle task at lowest priority */
    eos_task_t *idle = &g_tasks[0];
    idle->id = 0;
    idle->name = "idle";
    idle->entry = idle_task_func;
    idle->arg = NULL;
    idle->priority = 255;  /* Lowest priority */
    idle->stack_size = IDLE_STACK_SIZE;
    idle->stack_base = g_idle_stack;
    idle->stack_base[0] = EOS_STACK_CANARY;
    idle->stack_ptr = eos_port_init_stack(
        g_idle_stack + IDLE_STACK_SIZE / sizeof(uint32_t),
        idle_task_func, NULL
    );
    idle->state = EOS_TASK_READY;
    g_task_count = 1;

    return EOS_KERN_OK;
}

void eos_kernel_start(void)
{
    g_running = 1;

    /* Find highest-priority task to start */
    int first = find_next_task();
    if (first < 0) first = 0;  /* Fall back to idle */

    g_current = first;
    g_tasks[first].state = EOS_TASK_RUNNING;
    g_tasks[first].run_count++;
    g_next_sp = &g_tasks[first].stack_ptr;
    g_current_sp = g_next_sp;

    /* Start the first task — this never returns */
    eos_port_start_first_task();

    /* Should never reach here */
    while (1) {}
}

bool eos_kernel_is_running(void)
{
    return g_running != 0;
}

void eos_kernel_tick(void)
{
    g_tick++;
}

int eos_task_create(const char *name, eos_task_func_t entry, void *arg,
                     uint8_t priority, uint32_t stack_size)
{
    if (!entry) return EOS_KERN_INVALID;
    if (stack_size == 0) stack_size = 1024;

    /* Find free slot (skip slot 0 = idle) */
    int slot = -1;
    for (int i = 1; i < EOS_MAX_TASKS; i++) {
        if (g_tasks[i].entry == NULL ||
            g_tasks[i].state == EOS_TASK_DELETED) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return EOS_KERN_NO_MEMORY;

    /* Allocate stack */
    uint32_t *stack_base = alloc_task_stack(stack_size);
    if (!stack_base) return EOS_KERN_NO_MEMORY;

    uint32_t *stack_top = stack_base + stack_size / sizeof(uint32_t);

    memset(&g_tasks[slot], 0, sizeof(eos_task_t));
    g_tasks[slot].id = (uint8_t)slot;
    g_tasks[slot].name = name;
    g_tasks[slot].entry = entry;
    g_tasks[slot].arg = arg;
    g_tasks[slot].priority = priority;
    g_tasks[slot].stack_size = stack_size;
    g_tasks[slot].stack_base = stack_base;
    g_tasks[slot].stack_ptr = eos_port_init_stack(stack_top, entry, arg);
    g_tasks[slot].state = EOS_TASK_READY;
    g_task_count++;

    /* If kernel is running and new task has higher priority, yield */
    if (g_running && g_current >= 0 &&
        priority < g_tasks[g_current].priority) {
        eos_port_yield();
    }

    return slot;
}

int eos_task_delete(eos_task_handle_t h)
{
    if (h >= EOS_MAX_TASKS) return EOS_KERN_INVALID;
    if (g_tasks[h].state == EOS_TASK_DELETED || g_tasks[h].entry == NULL)
        return EOS_KERN_INVALID;
    g_tasks[h].state = EOS_TASK_DELETED;
    g_tasks[h].entry = NULL;
    g_task_count--;

    if (h == (eos_task_handle_t)g_current) {
        eos_port_yield();
    }
    return EOS_KERN_OK;
}

int eos_task_suspend(eos_task_handle_t h)
{
    if (h >= EOS_MAX_TASKS || g_tasks[h].entry == NULL) return EOS_KERN_INVALID;
    g_tasks[h].state = EOS_TASK_SUSPENDED;
    if (h == (eos_task_handle_t)g_current) {
        eos_port_yield();
    }
    return EOS_KERN_OK;
}

int eos_task_resume(eos_task_handle_t h)
{
    if (h >= EOS_MAX_TASKS || g_tasks[h].entry == NULL) return EOS_KERN_INVALID;
    if (g_tasks[h].state == EOS_TASK_SUSPENDED) {
        g_tasks[h].state = EOS_TASK_READY;
    }
    return EOS_KERN_OK;
}

void eos_task_yield(void)
{
    if (g_current >= 0) {
        g_tasks[g_current].state = EOS_TASK_READY;
    }
    eos_port_yield();
}

void eos_task_delay_ms(uint32_t ms)
{
    if (g_current >= 0) {
        uint32_t crit = eos_port_enter_critical();
        g_tasks[g_current].wake_tick = g_tick + ms;
        g_tasks[g_current].state = EOS_TASK_BLOCKED;
        eos_port_exit_critical(crit);
        eos_port_yield();
    }
}

eos_task_handle_t eos_task_get_current(void)
{
    return (g_current >= 0) ? (eos_task_handle_t)g_current : 0xFF;
}

eos_task_state_t eos_task_get_state(eos_task_handle_t h)
{
    if (h >= EOS_MAX_TASKS) return EOS_TASK_DELETED;
    return g_tasks[h].state;
}

const char *eos_task_get_name(eos_task_handle_t h)
{
    if (h >= EOS_MAX_TASKS || !g_tasks[h].name) return "invalid";
    return g_tasks[h].name;
}

/* ============================================================
 * Software Timer Implementation
 * ============================================================ */

int eos_swtimer_create(eos_swtimer_handle_t *out, const char *name,
                        uint32_t period_ms, bool auto_reload,
                        eos_swtimer_callback_t callback, void *ctx)
{
    if (!out || !callback || period_ms == 0) return EOS_KERN_INVALID;

    for (int i = 0; i < EOS_MAX_TIMERS; i++) {
        if (!g_timers[i].in_use) {
            g_timers[i].in_use = 1;
            g_timers[i].active = 0;
            g_timers[i].auto_reload = auto_reload ? 1 : 0;
            g_timers[i].name = name;
            g_timers[i].period_ticks = period_ms;  /* 1 tick = 1 ms */
            g_timers[i].next_tick = 0;
            g_timers[i].callback = callback;
            g_timers[i].ctx = ctx;
            *out = (uint8_t)i;
            return EOS_KERN_OK;
        }
    }
    return EOS_KERN_NO_MEMORY;
}

int eos_swtimer_start(eos_swtimer_handle_t h)
{
    if (h >= EOS_MAX_TIMERS || !g_timers[h].in_use) return EOS_KERN_INVALID;
    g_timers[h].next_tick = g_tick + g_timers[h].period_ticks;
    g_timers[h].active = 1;
    return EOS_KERN_OK;
}

int eos_swtimer_stop(eos_swtimer_handle_t h)
{
    if (h >= EOS_MAX_TIMERS || !g_timers[h].in_use) return EOS_KERN_INVALID;
    g_timers[h].active = 0;
    return EOS_KERN_OK;
}

int eos_swtimer_reset(eos_swtimer_handle_t h)
{
    if (h >= EOS_MAX_TIMERS || !g_timers[h].in_use) return EOS_KERN_INVALID;
    g_timers[h].next_tick = g_tick + g_timers[h].period_ticks;
    return EOS_KERN_OK;
}

int eos_swtimer_delete(eos_swtimer_handle_t h)
{
    if (h >= EOS_MAX_TIMERS || !g_timers[h].in_use) return EOS_KERN_INVALID;
    g_timers[h].in_use = 0;
    g_timers[h].active = 0;
    return EOS_KERN_OK;
}

/**
 * @brief Process software timers — called from SysTick_Handler.
 */
void eos_swtimer_tick(uint32_t current_tick)
{
    for (int i = 0; i < EOS_MAX_TIMERS; i++) {
        swtimer_t *t = &g_timers[i];
        if (!t->in_use || !t->active) continue;
        if (current_tick >= t->next_tick) {
            t->callback((eos_swtimer_handle_t)i, t->ctx);
            if (t->auto_reload) {
                t->next_tick = current_tick + t->period_ticks;
            } else {
                t->active = 0;
            }
        }
    }
}

/* ============================================================
 * Internal Accessor Functions (used by sync.c and ipc.c)
 * ============================================================ */

uint32_t eos_tick_get(void)
{
    return g_tick;
}

uint8_t eos_task_get_priority_internal(eos_task_handle_t h)
{
    if (h >= EOS_MAX_TASKS) return 255;
    return g_tasks[h].priority;
}

void eos_task_set_priority_internal(eos_task_handle_t h, uint8_t prio)
{
    if (h >= EOS_MAX_TASKS) return;
    g_tasks[h].priority = prio;
}

void eos_task_block_with_timeout(eos_task_handle_t h, uint32_t timeout_ms)
{
    if (h >= EOS_MAX_TASKS) return;
    g_tasks[h].state = EOS_TASK_BLOCKED;
    if (timeout_ms == EOS_WAIT_FOREVER) {
        g_tasks[h].wake_tick = 0;  /* Never auto-wake */
    } else {
        g_tasks[h].wake_tick = g_tick + timeout_ms;
    }
}

void eos_task_unblock(eos_task_handle_t h)
{
    if (h >= EOS_MAX_TASKS) return;
    g_tasks[h].state = EOS_TASK_READY;
    g_tasks[h].wake_tick = 0;
}
