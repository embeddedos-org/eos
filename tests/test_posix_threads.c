// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
/*
 * posix_threads.c compiled and exercised on the host (#270).
 *
 * The point of this suite is that it compiles the file at all. Every errno
 * constant services/posix/src/posix_threads.c returns used to come from a
 * `#ifndef EINVAL / #define EINVAL 22` block sitting halfway down the file --
 * below pthread_attr_init() and its five siblings, which already returned
 * EINVAL. The translation unit could not compile, and nothing said so for as
 * long as the file existed, because EOS_ENABLE_POSIX defaults OFF and no CI
 * job turned it on. The constants now come from <eos/posix_errno.h> at the
 * top of the file; this suite is what notices if that ever slides back down
 * or goes missing, without waiting for the option to be switched on.
 *
 * posix_threads.c is #included with its POSIX names renamed -- it defines
 * pthread_create/join/exit/self and the pthread_attr_* set, which would
 * otherwise collide with the host libc's -- the way test_posix_io.c and
 * test_posix_time.c do. The kernel calls it needs are stubbed below, so the
 * task table is under test control and eos_posix (off by default, and not
 * buildable on every host) is not needed.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

#include <eos/kernel.h>

#define pthread_attr_init           eos_test_pthread_attr_init
#define pthread_attr_destroy        eos_test_pthread_attr_destroy
#define pthread_attr_setschedparam  eos_test_pthread_attr_setschedparam
#define pthread_attr_getschedparam  eos_test_pthread_attr_getschedparam
#define pthread_attr_setstacksize   eos_test_pthread_attr_setstacksize
#define pthread_attr_getstacksize   eos_test_pthread_attr_getstacksize
#define pthread_create              eos_test_pthread_create
#define pthread_join                eos_test_pthread_join
#define pthread_exit                eos_test_pthread_exit
#define pthread_self                eos_test_pthread_self
#define pthread_yield               eos_test_pthread_yield
#define pthread_t                   eos_test_pthread_t
#define pthread_attr_t              eos_test_pthread_attr_t
#define sched_param                 eos_test_sched_param
#include "../services/posix/src/posix_threads.c"

/* ---- Stubs for the kernel calls posix_threads.c needs ---- */

static int      stub_task_create_rc = 0;   /* slot handed back, or negative */
static int      stub_sem_create_rc  = EOS_KERN_OK;
static int      stub_sem_wait_rc    = EOS_KERN_OK;
static int      stub_sem_deletes    = 0;
static int      stub_sem_posts      = 0;
static uint8_t  stub_last_priority  = 0;
static uint32_t stub_last_stack     = 0;
static eos_task_func_t stub_entry   = NULL;
static void    *stub_entry_arg      = NULL;
static eos_task_handle_t stub_current = 0;

int eos_task_create(const char *name, eos_task_func_t entry, void *arg,
                    uint8_t priority, uint32_t stack_size) {
    (void)name;
    stub_entry = entry;
    stub_entry_arg = arg;
    stub_last_priority = priority;
    stub_last_stack = stack_size;
    return stub_task_create_rc;
}

int eos_task_delete(eos_task_handle_t handle) { (void)handle; return EOS_KERN_OK; }
void eos_task_yield(void) { }
eos_task_handle_t eos_task_get_current(void) { return stub_current; }

int eos_sem_create(eos_sem_handle_t *out, uint32_t initial, uint32_t max) {
    (void)initial; (void)max;
    if (out) *out = 7;
    return stub_sem_create_rc;
}
int eos_sem_wait(eos_sem_handle_t handle, uint32_t timeout_ms) {
    (void)handle; (void)timeout_ms; return stub_sem_wait_rc;
}
int eos_sem_post(eos_sem_handle_t handle) {
    (void)handle; stub_sem_posts++; return EOS_KERN_OK;
}
int eos_sem_delete(eos_sem_handle_t handle) {
    (void)handle; stub_sem_deletes++; return EOS_KERN_OK;
}

/* ---- Cases ---- */

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

static void reset(void) {
    memset(posix_thread_table, 0, sizeof(posix_thread_table));
    posix_threads_initialized = true;
    stub_task_create_rc = 0;
    stub_sem_create_rc = EOS_KERN_OK;
    stub_sem_wait_rc = EOS_KERN_OK;
    stub_sem_deletes = 0;
    stub_sem_posts = 0;
    stub_current = 0;
}

/* The six calls that used EINVAL above the old define block: each one is a
 * compile error away from the #270 failure. */
static void test_attr_null_arguments_return_einval(void) {
    pthread_attr_t attr;
    struct sched_param sp = { .sched_priority = 3 };
    size_t stack = 0;

    assert(pthread_attr_init(NULL) == EINVAL);
    assert(pthread_attr_setschedparam(NULL, &sp) == EINVAL);
    assert(pthread_attr_setschedparam(&attr, NULL) == EINVAL);
    assert(pthread_attr_getschedparam(NULL, &sp) == EINVAL);
    assert(pthread_attr_getschedparam(&attr, NULL) == EINVAL);
    assert(pthread_attr_setstacksize(NULL, 1024) == EINVAL);
    assert(pthread_attr_getstacksize(NULL, &stack) == EINVAL);
    assert(pthread_attr_getstacksize(&attr, NULL) == EINVAL);
    PASS("pthread_attr_* reject NULL with EINVAL");
}

static void test_attr_init_defaults(void) {
    pthread_attr_t attr;
    memset(&attr, 0xAA, sizeof(attr));
    assert(pthread_attr_init(&attr) == 0);
    assert(attr.priority == 5);
    assert(attr.stack_size == 1024);
    assert(attr.detach_state == PTHREAD_CREATE_JOINABLE);
    assert(pthread_attr_destroy(&attr) == 0);
    PASS("pthread_attr_init defaults");
}

static void test_attr_roundtrip(void) {
    pthread_attr_t attr;
    struct sched_param sp = { .sched_priority = 9 };
    struct sched_param out = { .sched_priority = 0 };
    size_t stack = 0;

    assert(pthread_attr_init(&attr) == 0);
    assert(pthread_attr_setschedparam(&attr, &sp) == 0);
    assert(pthread_attr_getschedparam(&attr, &out) == 0);
    assert(out.sched_priority == 9);
    assert(pthread_attr_setstacksize(&attr, 4096) == 0);
    assert(pthread_attr_getstacksize(&attr, &stack) == 0);
    assert(stack == 4096);
    PASS("pthread_attr_* round-trip priority and stack size");
}

/* PTHREAD_STACK_MIN is the other EINVAL above the old block. */
static void test_attr_stacksize_below_minimum(void) {
    pthread_attr_t attr;
    assert(pthread_attr_init(&attr) == 0);
    assert(pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN - 1) == EINVAL);
    assert(attr.stack_size == 1024);   /* rejected, so unchanged */
    assert(pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN) == 0);
    PASS("pthread_attr_setstacksize rejects below PTHREAD_STACK_MIN");
}

static int   routine_arg = 0;
static void *dummy_routine(void *arg) { return arg; }

static void test_create_null_arguments(void) {
    reset();
    pthread_t th;
    assert(pthread_create(NULL, NULL, dummy_routine, NULL) == EINVAL);
    assert(pthread_create(&th, NULL, NULL, NULL) == EINVAL);
    PASS("pthread_create rejects NULL thread/start with EINVAL");
}

static void test_create_passes_attributes_to_the_kernel(void) {
    reset();
    pthread_attr_t attr;
    assert(pthread_attr_init(&attr) == 0);
    assert(pthread_attr_setstacksize(&attr, 2048) == 0);
    struct sched_param sp = { .sched_priority = 2 };
    assert(pthread_attr_setschedparam(&attr, &sp) == 0);

    pthread_t th = 0xFF;
    stub_task_create_rc = 4;
    assert(pthread_create(&th, &attr, dummy_routine, &routine_arg) == 0);
    assert(th == 0);                       /* first free slot */
    assert(stub_last_priority == 2);
    assert(stub_last_stack == 2048);
    assert(posix_thread_table[0].active);
    assert(posix_thread_table[0].start_routine == dummy_routine);
    assert(posix_thread_table[0].arg == &routine_arg);
    /* The kernel is handed the wrapper, never the user routine: the wrapper
     * is what records the return value and signals a joiner, so a
     * pthread_create() that passed `start` straight through would lose both
     * and still satisfy every other assertion here. */
    assert(stub_entry == posix_thread_wrapper);
    assert(stub_entry_arg == (void *)(uintptr_t)0);   /* the slot index */
    PASS("pthread_create forwards priority, stack, argument and the wrapper");
}

/* Run the entry the kernel was given, which is what a started task does. */
static void test_wrapper_records_retval_and_signals_the_joiner(void) {
    reset();
    pthread_t th = 0;
    assert(pthread_create(&th, NULL, dummy_routine, &routine_arg) == 0);
    assert(stub_entry != NULL);

    stub_entry(stub_entry_arg);

    /* dummy_routine returns its own argument. */
    assert(posix_thread_table[0].retval == &routine_arg);
    assert(stub_sem_posts == 1);            /* joiner released */
    assert(posix_thread_table[0].active);    /* joinable: slot held for join */

    void *rv = NULL;
    assert(pthread_join(th, &rv) == 0);
    assert(rv == &routine_arg);
    PASS("the task entry records the return value and releases a joiner");
}

/* A detached thread has no joiner, so the wrapper itself frees the slot. */
static void test_wrapper_frees_a_detached_slot(void) {
    reset();
    pthread_attr_t attr;
    assert(pthread_attr_init(&attr) == 0);
    attr.detach_state = PTHREAD_CREATE_DETACHED;

    pthread_t th = 0;
    assert(pthread_create(&th, &attr, dummy_routine, &routine_arg) == 0);
    stub_entry(stub_entry_arg);
    assert(!posix_thread_table[0].active);
    assert(stub_sem_deletes == 1);
    PASS("the task entry frees a detached thread's slot");
}

static void test_create_reports_eagain_when_the_kernel_refuses(void) {
    reset();
    pthread_t th = 0;
    stub_task_create_rc = -1;
    assert(pthread_create(&th, NULL, dummy_routine, NULL) == EAGAIN);
    assert(!posix_thread_table[0].active);  /* slot released again */
    assert(stub_sem_deletes == 1);          /* join semaphore cleaned up */

    reset();
    stub_sem_create_rc = -1;
    assert(pthread_create(&th, NULL, dummy_routine, NULL) == EAGAIN);
    PASS("pthread_create reports EAGAIN on kernel refusal");
}

static void test_join_unknown_thread_is_esrch(void) {
    reset();
    assert(pthread_join(3, NULL) == ESRCH);
    assert(pthread_join((pthread_t)(EOS_MAX_TASKS), NULL) == ESRCH);
    PASS("pthread_join of an unknown thread -> ESRCH");
}

static void test_join_detached_thread_is_einval(void) {
    reset();
    posix_thread_table[2].active = true;
    posix_thread_table[2].detached = true;
    assert(pthread_join(2, NULL) == EINVAL);
    PASS("pthread_join of a detached thread -> EINVAL");
}

static void test_join_returns_the_retval(void) {
    reset();
    posix_thread_table[1].active = true;
    posix_thread_table[1].detached = false;
    posix_thread_table[1].retval = &routine_arg;
    void *rv = NULL;
    assert(pthread_join(1, &rv) == 0);
    assert(rv == &routine_arg);
    assert(!posix_thread_table[1].active);
    PASS("pthread_join hands back the thread's return value");
}

static void test_join_reports_esrch_when_the_wait_fails(void) {
    reset();
    posix_thread_table[1].active = true;
    stub_sem_wait_rc = -1;
    assert(pthread_join(1, NULL) == ESRCH);
    PASS("pthread_join -> ESRCH when the join semaphore wait fails");
}

static void test_self_and_yield(void) {
    reset();
    posix_thread_table[5].active = true;
    posix_thread_table[5].eos_handle = 9;
    stub_current = 9;
    assert(pthread_self() == 5);
    stub_current = 77;                 /* not a pthread */
    assert(pthread_self() == 0);
    assert(pthread_yield() == 0);
    PASS("pthread_self maps the current task, pthread_yield returns 0");
}

int main(void) {
    printf("=== EoS POSIX threads tests ===\n");
    test_attr_null_arguments_return_einval();
    test_attr_init_defaults();
    test_attr_roundtrip();
    test_attr_stacksize_below_minimum();
    test_create_null_arguments();
    test_create_passes_attributes_to_the_kernel();
    test_wrapper_records_retval_and_signals_the_joiner();
    test_wrapper_frees_a_detached_slot();
    test_create_reports_eagain_when_the_kernel_refuses();
    test_join_unknown_thread_is_esrch();
    test_join_detached_thread_is_einval();
    test_join_returns_the_retval();
    test_join_reports_esrch_when_the_wait_fails();
    test_self_and_yield();
    printf("\n=== ALL %d POSIX THREAD TESTS PASSED ===\n", passed);
    return 0;
}
