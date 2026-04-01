// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eos/kernel.h"

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
    assert(eos_task_delete((eos_task_handle_t)h) == EOS_KERN_OK);
    assert(eos_task_get_state((eos_task_handle_t)h) == EOS_TASK_DELETED);
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
    assert(eos_mutex_create(&m) == EOS_KERN_OK);
    assert(eos_mutex_lock(m, 0) == EOS_KERN_OK);
    assert(eos_mutex_lock(m, 0) == EOS_KERN_OK);  /* recursive */
    assert(eos_mutex_unlock(m) == EOS_KERN_OK);
    assert(eos_mutex_unlock(m) == EOS_KERN_OK);
    assert(eos_mutex_delete(m) == EOS_KERN_OK);
    assert(eos_mutex_create(NULL) == EOS_KERN_INVALID);
    printf("[PASS] mutex\n");
}

static void test_semaphore(void) {
    eos_kernel_init();
    eos_sem_handle_t s;
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
    printf("[PASS] semaphore\n");
}

static void test_queue(void) {
    eos_kernel_init();
    eos_queue_handle_t q;
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
    printf("[PASS] queue\n");
}

static void test_queue_full(void) {
    eos_kernel_init();
    eos_queue_handle_t q;
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
    printf("=== ALL KERNEL TESTS PASSED (9/9) ===\n");
    return 0;
}