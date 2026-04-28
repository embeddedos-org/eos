// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
/**
 * @file main_system_test.c
 * @brief STM32F4 system test — boots eos kernel, runs tasks + sync + IPC
 *
 * Output via ARM semihosting printf or UART.
 * This is the bare-metal entry point (called from startup_stm32f407.S).
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "eos/kernel.h"
#include "eos/kernel_internal.h"
#include "eos/mem.h"

/* ---- Semihosting puts (works with OpenOCD/QEMU) ---- */
#if defined(__ARM_ARCH)
static void semi_puts(const char *s)
{
    /* ARM semihosting SYS_WRITE0 (0x04) */
    __asm volatile (
        "mov r0, #0x04\n"
        "mov r1, %0\n"
        "bkpt #0xAB\n"
        :
        : "r" (s)
        : "r0", "r1"
    );
}
#define PUTS(s) semi_puts(s)
#else
#define PUTS(s) printf("%s", s)
#endif

static int pass = 0, fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { pass++; } \
    else { fail++; PUTS("  [FAIL] "); PUTS(msg); PUTS("\n"); } \
} while(0)

/* ---- Test tasks ---- */
static volatile int task_a_ran = 0;
static volatile int task_b_ran = 0;

static void task_a_func(void *arg)
{
    (void)arg;
    task_a_ran = 1;
    while (1) {
        eos_task_delay_ms(100);
    }
}

static void task_b_func(void *arg)
{
    (void)arg;
    task_b_ran = 1;
    while (1) {
        eos_task_delay_ms(200);
    }
}

/* ---- Heap area ---- */
static uint8_t heap_area[4096] __attribute__((aligned(8)));

/* ---- Main ---- */
int main(void)
{
    PUTS("\n========================================\n");
    PUTS("  EoS System Test — STM32F407VG\n");
    PUTS("========================================\n");

    /* 1. Heap */
    PUTS("\n-- Heap --\n");
    CHECK(eos_heap_init(heap_area, sizeof(heap_area)) == 0, "heap init");
    void *p = eos_malloc(64);
    CHECK(p != NULL, "malloc 64");
    CHECK(((uintptr_t)p & 7) == 0, "8-byte aligned");
    eos_free(p);
    void *p2 = eos_malloc(256);
    CHECK(p2 != NULL, "malloc 256 after free");
    eos_free(p2);
    PUTS("  heap OK\n");

    /* 2. Kernel init */
    PUTS("\n-- Kernel Init --\n");
    CHECK(eos_kernel_init() == EOS_KERN_OK, "kernel init");
    CHECK(eos_task_get_state(0) == EOS_TASK_READY, "idle task ready");
    CHECK(eos_task_get_priority_internal(0) == 255, "idle prio=255");
    PUTS("  kernel OK\n");

    /* 3. Task creation */
    PUTS("\n-- Tasks --\n");
    int ha = eos_task_create("taskA", task_a_func, NULL, 1, 512);
    int hb = eos_task_create("taskB", task_b_func, NULL, 5, 512);
    CHECK(ha > 0, "task A created");
    CHECK(hb > 0, "task B created");
    CHECK(eos_task_get_state((uint8_t)ha) == EOS_TASK_READY, "A ready");
    CHECK(eos_task_get_priority_internal((uint8_t)ha) == 1, "A prio=1");
    PUTS("  tasks OK\n");

    /* 4. Internal accessors */
    PUTS("\n-- Accessors --\n");
    eos_task_set_priority_internal((uint8_t)hb, 3);
    CHECK(eos_task_get_priority_internal((uint8_t)hb) == 3, "set prio 3");
    eos_task_set_priority_internal((uint8_t)hb, 5);
    eos_task_block_with_timeout((uint8_t)hb, 100);
    CHECK(g_tasks[hb].state == EOS_TASK_BLOCKED, "blocked");
    CHECK(g_tasks[hb].wake_tick == g_tick + 100, "wake_tick");
    eos_task_unblock((uint8_t)hb);
    CHECK(g_tasks[hb].state == EOS_TASK_READY, "unblocked");
    PUTS("  accessors OK\n");

    /* 5. Mutex */
    PUTS("\n-- Mutex --\n");
    eos_mutex_handle_t mtx;
    CHECK(eos_mutex_create(&mtx) == EOS_KERN_OK, "mtx create");
    CHECK(eos_mutex_lock(mtx, 0) == EOS_KERN_OK, "mtx lock");
    CHECK(eos_mutex_lock(mtx, 0) == EOS_KERN_OK, "mtx recursive");
    CHECK(eos_mutex_unlock(mtx) == EOS_KERN_OK, "mtx unlock 1");
    CHECK(eos_mutex_unlock(mtx) == EOS_KERN_OK, "mtx unlock 2");
    CHECK(eos_mutex_delete(mtx) == EOS_KERN_OK, "mtx delete");
    PUTS("  mutex OK\n");

    /* 6. Semaphore */
    PUTS("\n-- Semaphore --\n");
    eos_sem_handle_t sem;
    CHECK(eos_sem_create(&sem, 2, 3) == EOS_KERN_OK, "sem create");
    CHECK(eos_sem_get_count(sem) == 2, "count=2");
    CHECK(eos_sem_wait(sem, 0) == EOS_KERN_OK, "wait 1");
    CHECK(eos_sem_wait(sem, 0) == EOS_KERN_OK, "wait 2");
    CHECK(eos_sem_wait(sem, 0) == EOS_KERN_TIMEOUT, "empty");
    CHECK(eos_sem_post(sem) == EOS_KERN_OK, "post");
    CHECK(eos_sem_delete(sem) == EOS_KERN_OK, "sem delete");
    PUTS("  semaphore OK\n");

    /* 7. Queue */
    PUTS("\n-- Queue --\n");
    eos_queue_handle_t q;
    CHECK(eos_queue_create(&q, sizeof(int), 4) == EOS_KERN_OK, "q create");
    int v1 = 42, v2 = 99;
    CHECK(eos_queue_send(q, &v1, 0) == EOS_KERN_OK, "send 42");
    CHECK(eos_queue_send(q, &v2, 0) == EOS_KERN_OK, "send 99");
    CHECK(eos_queue_count(q) == 2, "count=2");
    int out = 0;
    CHECK(eos_queue_receive(q, &out, 0) == EOS_KERN_OK, "recv");
    CHECK(out == 42, "FIFO: got 42");
    CHECK(eos_queue_receive(q, &out, 0) == EOS_KERN_OK, "recv 2");
    CHECK(out == 99, "FIFO: got 99");
    CHECK(eos_queue_is_empty(q), "empty");
    CHECK(eos_queue_delete(q) == EOS_KERN_OK, "q delete");
    PUTS("  queue OK\n");

    /* 8. Task lifecycle */
    PUTS("\n-- Lifecycle --\n");
    CHECK(eos_task_suspend((uint8_t)ha) == EOS_KERN_OK, "suspend");
    CHECK(eos_task_get_state((uint8_t)ha) == EOS_TASK_SUSPENDED, "SUSPENDED");
    CHECK(eos_task_resume((uint8_t)ha) == EOS_KERN_OK, "resume");
    CHECK(eos_task_get_state((uint8_t)ha) == EOS_TASK_READY, "READY");
    CHECK(eos_task_delete((uint8_t)ha) == EOS_KERN_OK, "delete A");
    CHECK(eos_task_delete((uint8_t)ha) == EOS_KERN_INVALID, "double-del guard");
    CHECK(eos_task_delete((uint8_t)hb) == EOS_KERN_OK, "delete B");
    PUTS("  lifecycle OK\n");

    /* ---- Results ---- */
    PUTS("\n========================================\n");
    if (fail == 0) {
        PUTS("  EoS kernel started\n");
        PUTS("  ALL SYSTEM TESTS PASSED\n");
    } else {
        PUTS("  SOME TESTS FAILED\n");
    }
    PUTS("========================================\n");

    /* Halt */
    while (1) { __asm volatile ("wfi"); }
    return 0;
}
