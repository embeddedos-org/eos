// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_priority_inherit.c
 * @brief Priority-inheritance tests for the mutex implementation.
 *
 * With a no-op eos_port_yield(), a contended lock runs to its timeout path and
 * dequeues the caller, so the boosted state exists only during the yield. The
 * assertions therefore run from inside the eos_port_yield() stub, while the
 * waiter is still parked. Nesting that hook parks several waiters at once.
 *
 * Priorities are inverted: lower value == more urgent.
 */

#include "eos/kernel.h"
#include "eos/kernel_internal.h"
#include <stdio.h>
#include <assert.h>

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while (0)

static void (*g_yield_hook)(void);

static void noop_task(void *arg) { (void)arg; }

static eos_task_handle_t mk_task(const char *name, uint8_t prio)
{
    int h = eos_task_create(name, noop_task, NULL, prio, 256);
    assert(h >= 0);
    return (eos_task_handle_t)h;
}

static uint8_t prio_of(eos_task_handle_t h)
{
    return eos_task_get_priority_internal(h);
}

/* ---- Releasing one of several held mutexes must keep the other's boost ---- */

static eos_mutex_handle_t t1_m1, t1_m2;
static eos_task_handle_t  t1_low, t1_high;

static void t1_hook(void)
{
    assert(prio_of(t1_low) == 3);

    eos_task_set_current_internal(t1_low);
    assert(eos_mutex_unlock(t1_m1) == EOS_KERN_OK);
    assert(prio_of(t1_low) == 3);

    eos_task_set_current_internal(t1_high);
}

static void test_boost_survives_partial_release(void)
{
    eos_kernel_init();
    assert(eos_mutex_create(&t1_m1) == EOS_KERN_OK);
    assert(eos_mutex_create(&t1_m2) == EOS_KERN_OK);

    t1_low  = mk_task("low", 20);
    t1_high = mk_task("high", 3);

    eos_task_set_current_internal(t1_low);
    assert(eos_mutex_lock(t1_m1, EOS_NO_WAIT) == EOS_KERN_OK);
    assert(eos_mutex_lock(t1_m2, EOS_NO_WAIT) == EOS_KERN_OK);
    assert(prio_of(t1_low) == 20);

    eos_task_set_current_internal(t1_high);
    g_yield_hook = t1_hook;
    assert(eos_mutex_lock(t1_m2, 100) == EOS_KERN_TIMEOUT);
    g_yield_hook = NULL;

    assert(prio_of(t1_low) == 20);

    eos_task_set_current_internal(t1_low);
    assert(eos_mutex_unlock(t1_m2) == EOS_KERN_OK);
    assert(prio_of(t1_low) == 20);

    eos_mutex_delete(t1_m1);
    eos_mutex_delete(t1_m2);
    PASS("boost survives partial release");
}

/* ---- A boost must never become the task's baseline ---- */

static eos_mutex_handle_t t2_m1, t2_m2;
static eos_task_handle_t  t2_low;

static void t2_hook(void)
{
    assert(prio_of(t2_low) == 2);

    eos_task_set_current_internal(t2_low);
    assert(eos_mutex_lock(t2_m2, EOS_NO_WAIT) == EOS_KERN_OK);
    assert(eos_mutex_unlock(t2_m2) == EOS_KERN_OK);
    assert(prio_of(t2_low) == 2);
}

static void test_base_priority_not_overwritten_by_boost(void)
{
    eos_kernel_init();
    assert(eos_mutex_create(&t2_m1) == EOS_KERN_OK);
    assert(eos_mutex_create(&t2_m2) == EOS_KERN_OK);

    t2_low = mk_task("low", 30);
    eos_task_handle_t high = mk_task("high", 2);

    eos_task_set_current_internal(t2_low);
    assert(eos_mutex_lock(t2_m1, EOS_NO_WAIT) == EOS_KERN_OK);

    eos_task_set_current_internal(high);
    g_yield_hook = t2_hook;
    assert(eos_mutex_lock(t2_m1, 100) == EOS_KERN_TIMEOUT);
    g_yield_hook = NULL;

    assert(prio_of(t2_low) == 30);

    eos_task_set_current_internal(t2_low);
    assert(eos_mutex_unlock(t2_m1) == EOS_KERN_OK);
    assert(prio_of(t2_low) == 30);

    eos_mutex_delete(t2_m1);
    eos_mutex_delete(t2_m2);
    PASS("base priority not overwritten by boost");
}

/* ---- A waiter that times out must take its boost with it ---- */

static eos_task_handle_t t3_low;

static void t3_hook(void)
{
    assert(prio_of(t3_low) == 5);
}

static void test_timeout_withdraws_boost(void)
{
    eos_kernel_init();
    eos_mutex_handle_t m;
    assert(eos_mutex_create(&m) == EOS_KERN_OK);

    t3_low = mk_task("low", 40);
    eos_task_handle_t high = mk_task("high", 5);

    eos_task_set_current_internal(t3_low);
    assert(eos_mutex_lock(m, EOS_NO_WAIT) == EOS_KERN_OK);

    eos_task_set_current_internal(high);
    g_yield_hook = t3_hook;
    assert(eos_mutex_lock(m, 50) == EOS_KERN_TIMEOUT);
    g_yield_hook = NULL;

    assert(prio_of(t3_low) == 40);

    eos_task_set_current_internal(t3_low);
    assert(eos_mutex_unlock(m) == EOS_KERN_OK);
    eos_mutex_delete(m);
    PASS("timeout withdraws boost");
}

/* ---- A boost must reach the task at the head of the blocking chain ---- */

static eos_mutex_handle_t t4_m_ab, t4_m_bc;
static eos_task_handle_t  t4_a, t4_b, t4_c;

static void t4_inner_hook(void)
{
    assert(prio_of(t4_b) == 4);
    assert(prio_of(t4_c) == 4);
}

static void t4_outer_hook(void)
{
    assert(prio_of(t4_c) == 25);

    g_yield_hook = t4_inner_hook;
    eos_task_set_current_internal(t4_a);
    (void)eos_mutex_lock(t4_m_ab, 100);
    g_yield_hook = NULL;
}

static void test_transitive_chain(void)
{
    eos_kernel_init();
    assert(eos_mutex_create(&t4_m_ab) == EOS_KERN_OK);
    assert(eos_mutex_create(&t4_m_bc) == EOS_KERN_OK);

    t4_c = mk_task("c_low", 50);
    t4_b = mk_task("b_mid", 25);
    t4_a = mk_task("a_high", 4);

    eos_task_set_current_internal(t4_c);
    assert(eos_mutex_lock(t4_m_bc, EOS_NO_WAIT) == EOS_KERN_OK);

    eos_task_set_current_internal(t4_b);
    assert(eos_mutex_lock(t4_m_ab, EOS_NO_WAIT) == EOS_KERN_OK);

    g_yield_hook = t4_outer_hook;
    (void)eos_mutex_lock(t4_m_bc, 100);
    g_yield_hook = NULL;

    assert(prio_of(t4_c) == 50);
    assert(prio_of(t4_b) == 25);

    eos_mutex_delete(t4_m_ab);
    eos_mutex_delete(t4_m_bc);
    PASS("transitive chained inheritance");
}

/* ---- Deleting a mutex must retire the boost it was causing ---- */

static eos_mutex_handle_t t5_m;
static eos_task_handle_t  t5_low;

static void t5_hook(void)
{
    assert(prio_of(t5_low) == 6);
    assert(eos_mutex_delete(t5_m) == EOS_KERN_OK);
    assert(prio_of(t5_low) == 45);
}

static void test_delete_withdraws_boost(void)
{
    eos_kernel_init();
    assert(eos_mutex_create(&t5_m) == EOS_KERN_OK);

    t5_low = mk_task("low", 45);
    eos_task_handle_t high = mk_task("high", 6);

    eos_task_set_current_internal(t5_low);
    assert(eos_mutex_lock(t5_m, EOS_NO_WAIT) == EOS_KERN_OK);

    eos_task_set_current_internal(high);
    g_yield_hook = t5_hook;
    (void)eos_mutex_lock(t5_m, 100);
    g_yield_hook = NULL;

    assert(prio_of(t5_low) == 45);
    PASS("delete withdraws boost");
}

/* ---- A full wait queue must fail the call, not block un-enqueued ---- */

static eos_mutex_handle_t t6_m;
static eos_task_handle_t  t6_waiters[EOS_MAX_TASKS];
static int t6_next, t6_total, t6_overflow_reported, t6_enqueued;

static void t6_hook(void)
{
    if (t6_next >= t6_total) return;
    eos_task_handle_t w = t6_waiters[t6_next++];
    eos_task_set_current_internal(w);
    int rc = eos_mutex_lock(t6_m, EOS_WAIT_FOREVER);
    if (rc == EOS_KERN_NO_MEMORY) t6_overflow_reported = 1;
    else t6_enqueued++;
}

static void test_waitqueue_overflow_reports_error(void)
{
    eos_kernel_init();
    assert(eos_mutex_create(&t6_m) == EOS_KERN_OK);

    eos_task_handle_t owner = mk_task("owner", 60);
    eos_task_set_current_internal(owner);
    assert(eos_mutex_lock(t6_m, EOS_NO_WAIT) == EOS_KERN_OK);

    t6_total = 0;
    for (int i = 0; i < EOS_MAX_TASKS - 2; i++) {
        char name[8];
        snprintf(name, sizeof(name), "w%d", i);
        t6_waiters[t6_total++] = mk_task(name, (uint8_t)(10 + i));
    }
    t6_next = 0;
    t6_enqueued = 0;
    t6_overflow_reported = 0;

    g_yield_hook = t6_hook;
    eos_task_set_current_internal(t6_waiters[t6_next++]);
    int rc = eos_mutex_lock(t6_m, EOS_WAIT_FOREVER);
    if (rc == EOS_KERN_NO_MEMORY) t6_overflow_reported = 1;
    g_yield_hook = NULL;

    assert(t6_enqueued > 0);
    assert(t6_overflow_reported);

    eos_mutex_delete(t6_m);
    PASS("wait-queue overflow reports error");
}

/* Mock port functions for host-based simulation/testing */

uint32_t eos_port_enter_critical(void) { return 0; }
void eos_port_exit_critical(uint32_t state) { (void)state; }

void eos_port_yield(void)
{
    /* Re-entrant: the overflow test relies on the hook nesting. */
    if (g_yield_hook) g_yield_hook();
}

void eos_port_start_scheduler(void) {}
uint32_t *eos_port_init_stack(uint32_t *s, void (*e)(void*), void *a) { (void)e; (void)a; return s - 17; }
void eos_port_start_first_task(void) {}

int main(void)
{
    printf("=== EoS Priority Inheritance Tests ===\n");
    test_boost_survives_partial_release();
    test_base_priority_not_overwritten_by_boost();
    test_timeout_withdraws_boost();
    test_transitive_chain();
    test_delete_withdraws_boost();
    test_waitqueue_overflow_reports_error();
    printf("\n=== ALL %d PRIORITY INHERITANCE TESTS PASSED ===\n", passed);
    return 0;
}
