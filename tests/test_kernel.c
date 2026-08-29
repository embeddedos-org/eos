// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eos/kernel.h"
#include "eos/kernel_internal.h"

static int g_task_ran = 0;
static void test_entry(void *arg) { g_task_ran = 1; (void)arg; }

static void test_kernel_init(void) {
    assert(eos_kernel_init() == EOS_KERN_OK);
    assert(!eos_kernel_is_running());
    printf("[PASS] kernel init\n");
}

static void test_task_create(void) {
    eos_kernel_init();
    int h = eos_task_create("t1", test_entry, NULL, 5, 1024);
    assert(h >= 0);
    assert(eos_task_get_state((eos_task_handle_t)h) == EOS_TASK_READY);
    assert(strcmp(eos_task_get_name((eos_task_handle_t)h), "t1") == 0);
    printf("[PASS] task create\n");
}

static void test_task_create_invalid(void) {
    eos_kernel_init();
    assert(eos_task_create("t", NULL, NULL, 5, 1024) == EOS_KERN_INVALID);
    printf("[PASS] task create invalid\n");
}

static void test_task_delete(void) {
    eos_kernel_init();
    int h = eos_task_create("del", test_entry, NULL, 5, 1024);
    assert(strcmp(eos_task_get_name((eos_task_handle_t)h), "del") == 0);
    assert(eos_task_delete((eos_task_handle_t)h) == EOS_KERN_OK);
    assert(eos_task_get_state((eos_task_handle_t)h) == EOS_TASK_DELETED);
    assert(strcmp(eos_task_get_name((eos_task_handle_t)h), "invalid") == 0);
    printf("[PASS] task delete\n");
}

static void test_task_suspend_resume(void) {
    eos_kernel_init();
    int h = eos_task_create("sr", test_entry, NULL, 5, 1024);
    assert(eos_task_suspend((eos_task_handle_t)h) == EOS_KERN_OK);
    assert(eos_task_get_state((eos_task_handle_t)h) == EOS_TASK_SUSPENDED);
    assert(eos_task_resume((eos_task_handle_t)h) == EOS_KERN_OK);
    assert(eos_task_get_state((eos_task_handle_t)h) == EOS_TASK_READY);
    printf("[PASS] task suspend/resume\n");
}

static void test_mutex(void) {
    eos_kernel_init();
    eos_mutex_handle_t m;
    (void)m;
    assert(eos_mutex_create(&m) == EOS_KERN_OK);
    assert(eos_mutex_lock(m, 0) == EOS_KERN_OK);
    assert(eos_mutex_lock(m, 0) == EOS_KERN_OK);  /* recursive */
    assert(eos_mutex_unlock(m) == EOS_KERN_OK);
    assert(eos_mutex_unlock(m) == EOS_KERN_OK);
    assert(eos_mutex_delete(m) == EOS_KERN_OK);
    assert(eos_mutex_create(NULL) == EOS_KERN_INVALID);

    assert(eos_mutex_create(&m) == EOS_KERN_OK);
    for (int i = 0; i < 255; i++)
        assert(eos_mutex_lock(m, 0) == EOS_KERN_OK);
    assert(eos_mutex_lock(m, 0) == EOS_KERN_FULL);
    for (int i = 0; i < 255; i++)
        assert(eos_mutex_unlock(m) == EOS_KERN_OK);
    assert(eos_mutex_unlock(m) == EOS_KERN_INVALID);
    assert(eos_mutex_delete(m) == EOS_KERN_OK);
    printf("[PASS] mutex\n");
}

static void test_semaphore(void) {
    eos_kernel_init();
    eos_sem_handle_t s;
    (void)s;
    assert(eos_sem_create(&s, 3, 5) == EOS_KERN_OK);
    assert(eos_sem_get_count(s) == 3);
    assert(eos_sem_wait(s, 0) == EOS_KERN_OK);
    assert(eos_sem_get_count(s) == 2);
    assert(eos_sem_wait(s, 0) == EOS_KERN_OK);
    assert(eos_sem_wait(s, 0) == EOS_KERN_OK);
    assert(eos_sem_wait(s, 0) == EOS_KERN_TIMEOUT);  /* empty */
    assert(eos_sem_post(s) == EOS_KERN_OK);
    assert(eos_sem_get_count(s) == 1);
    assert(eos_sem_delete(s) == EOS_KERN_OK);
    assert(eos_sem_create(NULL, 1, 5) == EOS_KERN_INVALID);
    assert(eos_sem_create(&s, 0, 0) == EOS_KERN_INVALID);
    assert(eos_sem_create(&s, 10, 5) == EOS_KERN_INVALID);
    assert(eos_sem_create(&s, 1, 0x80000000u) == EOS_KERN_INVALID);
    printf("[PASS] semaphore\n");
}

static void test_queue(void) {
    eos_kernel_init();
    eos_queue_handle_t q;
    (void)q;
    assert(eos_queue_create(&q, sizeof(int), 4) == EOS_KERN_OK);
    assert(eos_queue_is_empty(q));
    int val = 42;
    assert(eos_queue_send(q, &val, 0) == EOS_KERN_OK);
    assert(eos_queue_count(q) == 1);
    assert(!eos_queue_is_empty(q));
    int out = 0;
    assert(eos_queue_receive(q, &out, 0) == EOS_KERN_OK);
    assert(out == 42);
    assert(eos_queue_is_empty(q));
    assert(eos_queue_receive(q, &out, 0) == EOS_KERN_EMPTY);
    assert(eos_queue_delete(q) == EOS_KERN_OK);
    assert(eos_queue_create(NULL, 4, 4) == EOS_KERN_INVALID);
    assert(eos_queue_create(&q, 0, 4) == EOS_KERN_INVALID);
    assert(eos_queue_create(&q, 64, 16) == EOS_KERN_OK);
    assert(eos_queue_delete(q) == EOS_KERN_OK);
    assert(eos_queue_create(&q, 64, 17) == EOS_KERN_NO_MEMORY);
    assert(eos_queue_create(&q, (size_t)0x10000000u, 16u) == EOS_KERN_NO_MEMORY);
    printf("[PASS] queue\n");
}

static void test_queue_full(void) {
    eos_kernel_init();
    eos_queue_handle_t q;
    (void)q;
    eos_queue_create(&q, sizeof(int), 2);
    int a = 1, b = 2, c = 3;
    assert(eos_queue_send(q, &a, 0) == EOS_KERN_OK);
    assert(eos_queue_send(q, &b, 0) == EOS_KERN_OK);
    assert(eos_queue_is_full(q));
    assert(eos_queue_send(q, &c, 0) == EOS_KERN_FULL);
    int out;
    eos_queue_peek(q, &out);
    assert(out == 1);
    eos_queue_delete(q);
    printf("[PASS] queue full/peek\n");
}

static void test_task_stats(void) {
    eos_kernel_init();
    int h = eos_task_create("stats_task", test_entry, NULL, 3, 1024);
    assert(h >= 0);

    eos_task_stats_t stats;
    assert(eos_task_get_stats((eos_task_handle_t)h, &stats) == EOS_KERN_OK);
    assert(stats.id == (uint8_t)h);
    assert(strcmp(stats.name, "stats_task") == 0);
    assert(stats.priority == 3);
    assert(stats.state == EOS_TASK_READY);
    assert(stats.stack_size == 1024);

    eos_task_stats_t all_stats[EOS_MAX_TASKS];
    int count = 0;
    assert(eos_task_get_all_stats(all_stats, EOS_MAX_TASKS, &count) == EOS_KERN_OK);
    assert(count >= 2); // idle task (0) + stats_task
    assert(eos_task_get_stats(EOS_MAX_TASKS, &stats) == EOS_KERN_INVALID);
    assert(eos_task_get_stats(0, NULL) == EOS_KERN_INVALID);
    printf("[PASS] task runtime stats\n");
}

static int g_timer_fired = 0;
static void timer_cb(eos_swtimer_handle_t h, void *ctx) {
    (void)h;
    (void)ctx;
    g_timer_fired++;
}

extern void eos_task_wake_check(uint32_t current_tick);
extern void eos_swtimer_tick(uint32_t current_tick);
extern volatile uint32_t g_tick;

static void test_tick_overflow(void) {
    eos_kernel_init();
    // Simulate near UINT32_MAX tick counter
    g_tick = 0xFFFFFFFFU - 5;

    // Test task delay across overflow
    int h = eos_task_create("overflow_task", test_entry, NULL, 5, 1024);
    assert(h >= 0);

    // Block task until tick wraps: deadline is (0xFFFFFFFF - 5) + 10 = 4 (wraps)
    eos_task_block_with_timeout((eos_task_handle_t)h, 10);
    assert(eos_task_get_state((eos_task_handle_t)h) == EOS_TASK_BLOCKED);

    // Current tick: 0xFFFFFFFF - 2 (not yet expired)
    eos_task_wake_check(0xFFFFFFFFU - 2);
    assert(eos_task_get_state((eos_task_handle_t)h) == EOS_TASK_BLOCKED);

    // Current tick wraps around to 4 (exact deadline)
    eos_task_wake_check(4);
    assert(eos_task_get_state((eos_task_handle_t)h) == EOS_TASK_READY);

    // Test swtimer across overflow
    eos_swtimer_handle_t tmr;
    g_timer_fired = 0;
    assert(eos_swtimer_create(&tmr, "tmr_overflow", 20, false, timer_cb, NULL) == EOS_KERN_OK);
    g_tick = 0xFFFFFFFFU - 10;
    assert(eos_swtimer_start(tmr) == EOS_KERN_OK); // deadline = 9

    // Tick before deadline
    eos_swtimer_tick(0xFFFFFFFFU - 5);
    assert(g_timer_fired == 0);

    // Tick after wrap past deadline
    eos_swtimer_tick(10);
    assert(g_timer_fired == 1);

    eos_swtimer_delete(tmr);
    printf("[PASS] tick overflow wraparound\n");
}

/* Mock port functions for host-based simulation/testing */
uint32_t eos_port_enter_critical(void) { return 0; }
void eos_port_exit_critical(uint32_t state) { (void)state; }
void eos_port_yield(void) {}
void eos_port_start_scheduler(void) {}
uint32_t *eos_port_init_stack(uint32_t *s, void (*e)(void*), void *a) { (void)e; (void)a; return s - 17; }
void eos_port_start_first_task(void) {}

int main(void) {
    printf("=== EoS Kernel Tests ===\n");
    test_kernel_init();
    test_task_create();
    test_task_create_invalid();
    test_task_delete();
    test_task_suspend_resume();
    test_mutex();
    test_semaphore();
    test_queue();
    test_queue_full();
    test_task_stats();
    test_tick_overflow();
    printf("=== ALL KERNEL TESTS PASSED (11/11) ===\n");
    return 0;
}