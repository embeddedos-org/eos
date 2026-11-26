// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file task.c
 * @brief EoS kernel task scheduler with real context switching
 *
 * Priority-based preemptive scheduler with SysTick-driven tick,
 * PendSV context switching, software timers, and stack canary detection.
 *
 * With EOS_SMP_CORES > 1 the same scheduler runs on every CPU: each CPU has
 * its own current task and idle task, and picks the best READY task that it
 * may run (core_mask) and that no other CPU is still executing. A task that
 * blocks or yields is marked before its CPU has switched away from it, so
 * "READY" alone does not mean its context is saved; the per-CPU current
 * slots do. Every change to scheduler state happens inside the port's
 * critical section, which on SMP is also the kernel lock.
 */

#include "eos/kernel.h"
#include "eos/kernel_internal.h"
#include "eos/arch.h"
#include "eos/mem.h"
#include <string.h>

/* ============================================================
 * Overflow-Safe Tick Comparison
 * ============================================================ */

/**
 * @brief Check whether a tick deadline has expired (overflow-safe).
 *
 * Uses unsigned subtraction so that the comparison works correctly
 * even when the 32-bit tick counter wraps past UINT32_MAX.  This is
 * the standard technique used by FreeRTOS, Zephyr, and the Linux
 * kernel (time_after macro).  Valid for deadline distances up to
 * INT32_MAX ticks (~24.8 days at 1 kHz).
 *
 * @param current  The current tick value.
 * @param deadline The target tick at which the event should fire.
 * @return true if the deadline has passed.
 */
static inline bool tick_expired(uint32_t current, uint32_t deadline)
{
    return (int32_t)(current - deadline) >= 0;
}

/* Stack canary value — placed at bottom of each task's stack */
#define EOS_STACK_CANARY    0xDEADBEEFU

/* Default idle task stack size */
#define IDLE_STACK_SIZE     256

/* ============================================================
 * Internal State
 * ============================================================ */

eos_task_t g_tasks[EOS_MAX_TASKS];
static int g_task_count = 0;
static int g_running = 0;

/* The task each CPU is executing (-1 before the kernel starts). */
#if EOS_SMP_CORES > 1
static int g_cur[EOS_SMP_CORES] = { [0 ... EOS_SMP_CORES - 1] = -1 };
#else
static int g_cur[1] = { -1 };
#endif
#if EOS_SMP_CORES > 1
#define THIS_CPU() ((int)eos_port_cpu_id())
#else
#define THIS_CPU() 0
#endif
#define g_current (g_cur[THIS_CPU()])

/* Slots 0 .. EOS_SMP_CORES-1 are the idle tasks, one per CPU. */
#define FIRST_USER_SLOT EOS_SMP_CORES

/* Index of the CPU executing task @p i, or -1. */
static int task_cpu(int i)
{
    for (int c = 0; c < EOS_SMP_CORES; c++) {
        if (g_cur[c] == i) return c;
    }
    return -1;
}

/* Tick counter — updated by SysTick_Handler */
volatile uint32_t g_tick = 0;

/* Reschedule flag */
volatile uint8_t g_needs_reschedule = 0;

/* Pointers used by the PendSV assembly handler */
#if EOS_SMP_CORES > 1
uint32_t **g_cpu_sp[EOS_SMP_CORES];
#else
uint32_t **g_current_sp = NULL;
uint32_t **g_next_sp = NULL;
#endif

/* Static heap for task stacks (used when eos_malloc is not available) */
#ifndef EOS_TASK_STACK_POOL_SIZE
#define EOS_TASK_STACK_POOL_SIZE  (EOS_MAX_TASKS * 1024)
#endif
static uint32_t g_stack_pool[EOS_TASK_STACK_POOL_SIZE / sizeof(uint32_t)];

/* Idle task stacks, one per CPU */
static uint32_t g_idle_stack[EOS_SMP_CORES][IDLE_STACK_SIZE / sizeof(uint32_t)];

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

/*
 * The pool keeps no free list of its own: the task table is the allocation
 * table. A region is in use while the task holding it is alive, and also
 * while it is still the task executing -- a task that deletes itself keeps
 * running on its stack until the switch away. Everything else is free, so a
 * deleted task's stack becomes reusable without a separate free call.
 */
static bool task_stack_in_use(int i)
{
    const eos_task_t *t = &g_tasks[i];
    if (t->stack_base == NULL) return false;
    if (task_cpu(i) >= 0) return true;
    return t->entry != NULL && t->state != EOS_TASK_DELETED;
}

static uint32_t *alloc_task_stack(uint32_t size_bytes)
{
    const uint32_t pool_words = EOS_TASK_STACK_POOL_SIZE / sizeof(uint32_t);
    uint32_t words = size_bytes / sizeof(uint32_t);
    uint32_t start = 0;

    /* First fit. On overlap with a live stack, retry just past its end;
     * `start` strictly increases, so the search terminates. The idle tasks'
     * stacks are not in the pool. */
    while (words <= pool_words && start <= pool_words - words) {
        uint32_t next = start;
        for (int i = FIRST_USER_SLOT; i < EOS_MAX_TASKS; i++) {
            if (!task_stack_in_use(i)) continue;
            uint32_t s = (uint32_t)(g_tasks[i].stack_base - g_stack_pool);
            uint32_t e = s + g_tasks[i].stack_size / sizeof(uint32_t);
            if (start < e && s < start + words) {
                next = e;
                break;
            }
        }
        if (next == start) {
            uint32_t *base = &g_stack_pool[start];
            /* Place canary at the bottom of the stack */
            base[0] = EOS_STACK_CANARY;
            return base;
        }
        start = next;
    }
    return NULL;  /* Pool exhausted */
}

/* ============================================================
 * Scheduler
 * ============================================================ */

/**
 * @brief Find the highest-priority ready task this CPU may run.
 */
static int find_next_task(void)
{
    int best = -1;
    uint8_t best_prio = 255;
    int cpu = THIS_CPU();
    int start = (g_cur[cpu] + 1) % EOS_MAX_TASKS;

    for (int i = 0; i < EOS_MAX_TASKS; i++) {
        int idx = (start + i) % EOS_MAX_TASKS;
        const eos_task_t *t = &g_tasks[idx];
        if (t->entry == NULL || t->state != EOS_TASK_READY) continue;
#if EOS_SMP_CORES > 1
        /* Not one another CPU still executes (its context is not saved),
         * and not one barred from this CPU. */
        int on = task_cpu(idx);
        if (on >= 0 && on != cpu) continue;
        if ((t->core_mask & (1U << cpu)) == 0U) continue;
#endif
        if (best < 0 || t->priority < best_prio) {
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
    /* Validate the outgoing task before making it eligible for selection. */
    if (g_current >= 0 && g_tasks[g_current].stack_base) {
        if (g_tasks[g_current].stack_base[0] != EOS_STACK_CANARY) {
            /* Stack overflow detected! */
            /* In production: trigger fault handler or reboot */
            g_tasks[g_current].state = EOS_TASK_DELETED;
        }
    }

    /* A preempted task remains runnable and competes at its normal priority. */
    if (g_current >= 0 && g_tasks[g_current].state == EOS_TASK_RUNNING) {
        g_tasks[g_current].state = EOS_TASK_READY;
    }
#if EOS_SMP_CORES > 1
    int outgoing = g_current;
#endif

    int next = find_next_task();
    /*
     * Unreachable by construction. Each CPU's idle task (slot = CPU number) is
     * one, which
     * eos_task_delete() and eos_task_suspend() refuse, and idle_task_func()
     * only spins or waits for an interrupt, so it never blocks: the idle
     * task is always READY (or was RUNNING and was made READY just above),
     * and the "best < 0 ||" term in find_next_task() lets it be chosen at
     * priority 255 when nothing else is. The only way to reach it is the
     * canary check above marking the idle task DELETED, which is a fault,
     * not a schedule. find_next_task() scans every slot including the
     * outgoing one, so next < 0 means nothing at all is READY; the return
     * leaves g_current_sp and g_next_sp naming the outgoing task and its
     * state as the canary check left it. Nothing here recovers from that,
     * and nothing should: an overflowed idle stack is a fault handler's job.
     */
    if (next < 0) return;  /* No valid task is runnable. */

    /*
     * PendSV has already saved the outgoing PSP through g_current_sp.
     * Point both assembly-facing slots at the task that is now current, so
     * the next PendSV saves that task's PSP into its own control block.
     */
    g_current = next;
    g_tasks[next].state = EOS_TASK_RUNNING;
    g_tasks[next].run_count++;
    g_current_sp = &g_tasks[next].stack_ptr;
#if EOS_SMP_CORES == 1
    g_next_sp = g_current_sp;
#else
    /* A task this CPU just let go of while it is still READY may be what
     * another CPU should run; until now no CPU could take it (it was still
     * executing here), so tell the others to look again. */
    if (outgoing >= 0 && outgoing != next && g_tasks[outgoing].state == EOS_TASK_READY &&
        (g_tasks[outgoing].core_mask & ~(1U << THIS_CPU())) != 0U) {
        eos_port_kick_other_cpus();
    }
#endif
}

/**
 * @brief Wake blocked tasks whose delay has expired.
 * Called from SysTick_Handler.
 */
void eos_task_wake_check(uint32_t current_tick)
{
#if EOS_SMP_CORES > 1
    bool woke = false;
#endif
    for (int i = 0; i < EOS_MAX_TASKS; i++) {
        eos_task_t *t = &g_tasks[i];
        /* wake_armed, not wake_tick > 0: a deadline of 0 is valid when
         * g_tick + timeout wraps. WAIT_FOREVER leaves wake_armed clear. */
        if (t->state == EOS_TASK_BLOCKED && t->wake_armed &&
            tick_expired(current_tick, t->wake_tick)) {
            t->state = EOS_TASK_READY;
            t->wake_armed = 0;
            t->wake_tick = 0;
#if EOS_SMP_CORES > 1
            woke = true;
#endif
        }
    }
#if EOS_SMP_CORES > 1
    /* Only CPU 0 runs this; the others learn of woken tasks now. */
    if (woke) eos_port_yield();
#endif
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
#elif defined(__riscv)
        __asm volatile ("wfi");
#elif defined(__XTENSA__)
        /* Sleep at interrupt level 0 until the next interrupt. */
        __asm volatile ("waiti 0");
#elif (defined(__x86_64__) || defined(_M_X64)) && !defined(_WIN32) && \
      !defined(__STDC_HOSTED__)
        __asm volatile ("hlt");
#else
        /* Hosted host/simulation: just spin */
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
    g_running = 0;
    g_tick = 0;
#if EOS_SMP_CORES > 1
    for (int c = 0; c < EOS_SMP_CORES; c++) {
        g_cur[c] = -1;
        g_cpu_sp[c] = NULL;
    }
#else
    g_cur[0] = -1;
    g_current_sp = NULL;
    g_next_sp = NULL;
#endif

    /* Create the idle task(s) at lowest priority, one per CPU */
    for (int c = 0; c < EOS_SMP_CORES; c++) {
        eos_task_t *idle = &g_tasks[c];
        idle->id = (uint8_t)c;
        idle->name = "idle";
        idle->entry = idle_task_func;
        idle->arg = NULL;
        idle->priority = 255;  /* Lowest priority */
        idle->base_priority = 255;
        idle->stack_size = IDLE_STACK_SIZE;
        idle->stack_base = g_idle_stack[c];
        idle->stack_base[0] = EOS_STACK_CANARY;
        idle->stack_ptr = eos_port_init_stack(
            g_idle_stack[c] + IDLE_STACK_SIZE / sizeof(uint32_t),
            idle_task_func, NULL
        );
#if EOS_SMP_CORES > 1
        idle->core_mask = 1U << c;   /* each CPU always has its own */
#endif
        idle->state = EOS_TASK_READY;
    }
    g_task_count = EOS_SMP_CORES;

    return EOS_KERN_OK;
}

void eos_kernel_start(void)
{
    g_running = 1;

    /* Find highest-priority task to start */
    uint32_t crit = eos_port_enter_critical();
    int first = find_next_task();
    if (first < 0) first = THIS_CPU();  /* Fall back to this CPU's idle */

    g_current = first;
    g_tasks[first].state = EOS_TASK_RUNNING;
    g_tasks[first].run_count++;
    g_current_sp = &g_tasks[first].stack_ptr;
#if EOS_SMP_CORES == 1
    g_next_sp = g_current_sp;
#endif
    eos_port_exit_critical(crit);

#if EOS_SMP_CORES > 1
    eos_port_start_secondary_cpus();
#endif

    /* Start the first task — this never returns */
    eos_port_start_first_task();

    /* Should never reach here */
    while (1) {}
}

#if EOS_SMP_CORES > 1
void eos_kernel_start_secondary(void)
{
    uint32_t crit = eos_port_enter_critical();
    int first = find_next_task();
    if (first < 0) first = THIS_CPU();
    g_current = first;
    g_tasks[first].state = EOS_TASK_RUNNING;
    g_tasks[first].run_count++;
    g_current_sp = &g_tasks[first].stack_ptr;
    eos_port_exit_critical(crit);
    eos_port_start_first_task();
    while (1) {}
}

uint32_t eos_kernel_cpu_id(void)
{
    return eos_port_cpu_id();
}

int eos_task_set_core_mask(eos_task_handle_t h, uint32_t mask)
{
    if (h < FIRST_USER_SLOT || h >= EOS_MAX_TASKS) return EOS_KERN_INVALID;
    mask &= (1U << EOS_SMP_CORES) - 1U;
    if (mask == 0U) return EOS_KERN_INVALID;
    uint32_t crit = eos_port_enter_critical();
    if (g_tasks[h].entry == NULL || g_tasks[h].state == EOS_TASK_DELETED) {
        eos_port_exit_critical(crit);
        return EOS_KERN_INVALID;
    }
    g_tasks[h].core_mask = mask;
    bool move = task_cpu(h) >= 0;
    eos_port_exit_critical(crit);
    if (move) eos_port_yield();   /* reschedules every CPU */
    return EOS_KERN_OK;
}

int eos_task_get_cpu(eos_task_handle_t h)
{
    if (h >= EOS_MAX_TASKS) return -1;
    uint32_t crit = eos_port_enter_critical();
    int c = task_cpu(h);
    eos_port_exit_critical(crit);
    return c;
}
#endif

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
    if (stack_size < EOS_MIN_STACK_SIZE) return EOS_KERN_INVALID;

    uint32_t crit = eos_port_enter_critical();

    /* Find free slot (skip the idle tasks' slots) */
    int slot = -1;
    for (int i = FIRST_USER_SLOT; i < EOS_MAX_TASKS; i++) {
        /* Not a slot a CPU is still executing: a task that deleted itself
         * runs on until its CPU switches away, and that switch stores its
         * stack pointer into the slot (on SMP another CPU can create a task
         * in between). */
        if (task_cpu(i) >= 0) continue;
        if (g_tasks[i].entry == NULL ||
            g_tasks[i].state == EOS_TASK_DELETED) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        eos_port_exit_critical(crit);
        return EOS_KERN_NO_MEMORY;
    }

    /* Allocate stack */
    uint32_t *stack_base = alloc_task_stack(stack_size);
    if (!stack_base) {
        eos_port_exit_critical(crit);
        return EOS_KERN_NO_MEMORY;
    }

    uint32_t *stack_top = stack_base + stack_size / sizeof(uint32_t);

    memset(&g_tasks[slot], 0, sizeof(eos_task_t));
    g_tasks[slot].state = EOS_TASK_DELETED;
    g_tasks[slot].id = (uint8_t)slot;
    g_tasks[slot].name = name;
    g_tasks[slot].arg = arg;
    g_tasks[slot].priority = priority;
    g_tasks[slot].base_priority = priority;
    g_tasks[slot].stack_size = stack_size;
    g_tasks[slot].stack_base = stack_base;
    g_tasks[slot].stack_ptr = eos_port_init_stack(stack_top, entry, arg);
#if EOS_SMP_CORES > 1
    g_tasks[slot].core_mask = (1U << EOS_SMP_CORES) - 1U;
#endif

    /* Publish the entry and READY state only after the TCB is complete. */
    g_tasks[slot].entry = entry;
    g_tasks[slot].state = EOS_TASK_READY;
    g_task_count++;

    /* If kernel is running and new task has higher priority, yield. On SMP
     * any CPU may be the one to run it, so every CPU reschedules. */
#if EOS_SMP_CORES > 1
    bool should_preempt = g_running != 0;
#else
    bool should_preempt = g_running && g_current >= 0 &&
                          priority < g_tasks[g_current].priority;
#endif
    eos_port_exit_critical(crit);
    if (should_preempt) {
        eos_port_yield();
    }

    return slot;
}

int eos_task_delete(eos_task_handle_t h)
{
    if (h < FIRST_USER_SLOT || h >= EOS_MAX_TASKS) return EOS_KERN_INVALID;

    uint32_t crit = eos_port_enter_critical();
    if (g_tasks[h].state == EOS_TASK_DELETED || g_tasks[h].entry == NULL) {
        eos_port_exit_critical(crit);
        return EOS_KERN_INVALID;
    }
    g_tasks[h].state = EOS_TASK_DELETED;
    g_tasks[h].entry = NULL;
    g_tasks[h].wake_armed = 0;
    g_task_count--;
    /* A task deleted while blocked is still on a wait list. Left there, an
     * unlock hands the mutex to the dead task and a post or send wakes it
     * instead of a live waiter. */
    eos_sync_forget_task(h);
    eos_ipc_forget_task(h);
    /* Running here or on another CPU: that CPU must switch away. */
    bool running = task_cpu(h) >= 0;
    eos_port_exit_critical(crit);

    if (running) {
        eos_port_yield();
    }
    return EOS_KERN_OK;
}

int eos_task_suspend(eos_task_handle_t h)
{
    if (h < FIRST_USER_SLOT || h >= EOS_MAX_TASKS) return EOS_KERN_INVALID;
    uint32_t crit = eos_port_enter_critical();
    if (g_tasks[h].entry == NULL) {
        eos_port_exit_critical(crit);
        return EOS_KERN_INVALID;
    }
    g_tasks[h].state = EOS_TASK_SUSPENDED;
    bool running = task_cpu(h) >= 0;
    eos_port_exit_critical(crit);
    if (running) {
        eos_port_yield();
    }
    return EOS_KERN_OK;
}

int eos_task_resume(eos_task_handle_t h)
{
    if (h >= EOS_MAX_TASKS) return EOS_KERN_INVALID;
    uint32_t crit = eos_port_enter_critical();
    if (g_tasks[h].entry == NULL) {
        eos_port_exit_critical(crit);
        return EOS_KERN_INVALID;
    }
    bool woke = false;
    if (g_tasks[h].state == EOS_TASK_SUSPENDED) {
        g_tasks[h].state = EOS_TASK_READY;
        woke = true;
    }
    eos_port_exit_critical(crit);
#if EOS_SMP_CORES > 1
    /* Another CPU may be idle and able to run it now. */
    if (woke && g_running) eos_port_yield();
#else
    (void)woke;
#endif
    return EOS_KERN_OK;
}

void eos_task_yield(void)
{
    uint32_t crit = eos_port_enter_critical();
    if (g_current >= 0) {
        g_tasks[g_current].state = EOS_TASK_READY;
    }
    eos_port_exit_critical(crit);
    eos_port_yield();
}

void eos_task_delay_ms(uint32_t ms)
{
    if (g_current >= 0) {
        uint32_t crit = eos_port_enter_critical();
        g_tasks[g_current].wake_tick = g_tick + ms;
        g_tasks[g_current].wake_armed = 1;
        g_tasks[g_current].state = EOS_TASK_BLOCKED;
        eos_port_exit_critical(crit);
        eos_port_yield();
    }
}

eos_task_handle_t eos_task_get_current(void)
{
    /* Masked, so the caller cannot move to another CPU between reading
     * the CPU number and its slot. */
    uint32_t crit = eos_port_enter_critical();
    int cur = g_current;
    eos_port_exit_critical(crit);
    return (cur >= 0) ? (eos_task_handle_t)cur : 0xFF;
}

void eos_task_set_current_internal(eos_task_handle_t h)
{
    if (h >= EOS_MAX_TASKS) {
        g_current = -1;
        return;
    }
    g_current = (int)h;
    g_tasks[h].state = EOS_TASK_RUNNING;
}

eos_task_state_t eos_task_get_state(eos_task_handle_t h)
{
    if (h >= EOS_MAX_TASKS) return EOS_TASK_DELETED;
    return g_tasks[h].state;
}

const char *eos_task_get_name(eos_task_handle_t h)
{
    if (h >= EOS_MAX_TASKS || g_tasks[h].state == EOS_TASK_DELETED || !g_tasks[h].name) {
        return "invalid";
    }
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
        if (tick_expired(current_tick, t->next_tick)) {
            /* Re-arm or retire before the callback, so whatever the callback
             * does to its own timer (start, stop, reset, delete) is what
             * stands. A one-shot that restarted itself used to be switched
             * off again here on return. */
            if (t->auto_reload) {
                t->next_tick = current_tick + t->period_ticks;
            } else {
                t->active = 0;
            }
            t->callback((eos_swtimer_handle_t)i, t->ctx);
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
        g_tasks[h].wake_tick = 0;
        g_tasks[h].wake_armed = 0;  /* Never auto-wake */
    } else {
        g_tasks[h].wake_tick = g_tick + timeout_ms;
        g_tasks[h].wake_armed = 1;
    }
}

void eos_task_unblock(eos_task_handle_t h)
{
    if (h >= EOS_MAX_TASKS) return;
    g_tasks[h].state = EOS_TASK_READY;
    g_tasks[h].wake_tick = 0;
    g_tasks[h].wake_armed = 0;
#if EOS_SMP_CORES > 1
    /* Another CPU may be idle and able to run it now, instead of at its
     * next tick: ping-pong across CPUs took one tick per round trip. */
    if (g_running) eos_port_yield();
#endif
}

/* ============================================================
 * Runtime Statistics API
 * ============================================================ */

int eos_task_get_stats(eos_task_handle_t h, eos_task_stats_t *out)
{
    if (h >= EOS_MAX_TASKS || !out) return EOS_KERN_INVALID;
    if (g_tasks[h].entry == NULL && h != 0) return EOS_KERN_INVALID;

    out->id        = g_tasks[h].id;
    out->name      = g_tasks[h].name;
    out->priority  = g_tasks[h].priority;
    out->state     = g_tasks[h].state;
    out->run_count = g_tasks[h].run_count;
    out->stack_size = g_tasks[h].stack_size;

    /* Estimate stack usage: scan from base upward for canary/zero words */
    if (g_tasks[h].stack_base && g_tasks[h].stack_size > 0) {
        uint32_t total_words = g_tasks[h].stack_size / sizeof(uint32_t);
        uint32_t unused_words = 0;
        /* Skip word 0 (canary), count zero-filled words from bottom up */
        for (uint32_t w = 1; w < total_words; w++) {
            if (g_tasks[h].stack_base[w] == 0) {
                unused_words++;
            } else {
                break;
            }
        }
        uint32_t used_bytes = (total_words - 1 - unused_words) * sizeof(uint32_t);
        out->stack_used = used_bytes;
    } else {
        out->stack_used = 0;
    }

    return EOS_KERN_OK;
}

int eos_task_get_all_stats(eos_task_stats_t *out, int max_entries, int *count)
{
    if (!out || max_entries <= 0 || !count) return EOS_KERN_INVALID;

    int n = 0;
    for (int i = 0; i < EOS_MAX_TASKS && n < max_entries; i++) {
        if (g_tasks[i].entry != NULL || i == 0) {
            eos_task_get_stats((eos_task_handle_t)i, &out[n]);
            n++;
        }
    }
    *count = n;
    return EOS_KERN_OK;
}
