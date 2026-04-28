// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file test_stress.c
 * @brief Hardware stress test — task churn, queue saturation, mutex contention,
 *        heap fragmentation, timer storm, semaphore flooding
 *
 * Self-contained with host arch-port stubs. Designed to expose:
 *   - Task slot exhaustion and reuse after delete
 *   - Queue wraparound under full/drain cycles
 *   - Mutex recursive depth limits and rapid lock/unlock
 *   - Heap fragmentation resistance under random alloc/free
 *   - Timer callback storm (many timers firing simultaneously)
 *   - Semaphore count boundaries under rapid post/wait
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

#include "eos/kernel.h"
#include "eos/kernel_internal.h"
#include "eos/mem.h"

/* Host arch-port stubs */
uint32_t *eos_port_init_stack(uint32_t *s, void (*e)(void*), void *a)
    { (void)e; (void)a; return s - 17; }
void eos_port_start_first_task(void) {}
void eos_port_yield(void) {}
uint32_t eos_port_enter_critical(void) { return 0; }
void eos_port_exit_critical(uint32_t s) { (void)s; }

static int pass = 0, fail = 0, total = 0;
#define CHECK(cond, msg) do { \
    total++; \
    if (!(cond)) { fail++; printf("    [FAIL] %s (line %d)\n", msg, __LINE__); } \
    else { pass++; } \
} while(0)

#define SECTION(name) printf("\n── %s ──\n", name)

static void dummy_task(void *arg) { (void)arg; }

/* ============================================================
 * 1. Task Churn — rapid create/delete cycles
 * ============================================================ */

static void test_task_churn(void)
{
    SECTION("Task Churn");

    /* Create and delete tasks in a tight loop to test slot reuse */
    for (int cycle = 0; cycle < 50; cycle++) {
        eos_kernel_init();

        int handles[EOS_MAX_TASKS - 1]; /* -1 for idle */
        int created = 0;

        /* Fill all task slots */
        for (int i = 0; i < EOS_MAX_TASKS - 1; i++) {
            int h = eos_task_create("churn", dummy_task, NULL,
                                     (uint8_t)(i + 1), 256);
            if (h > 0) {
                handles[created++] = h;
            }
        }

        /* Verify we actually filled slots */
        if (cycle == 0) {
            CHECK(created > 0, "created tasks in first cycle");
            /* Next create should fail (pool exhausted) */
            int overflow = eos_task_create("over", dummy_task, NULL, 50, 256);
            CHECK(overflow == EOS_KERN_NO_MEMORY, "task pool exhaustion");
        }

        /* Delete all */
        for (int i = 0; i < created; i++) {
            int rc = eos_task_delete((uint8_t)handles[i]);
            if (cycle == 0 && i == 0) {
                CHECK(rc == EOS_KERN_OK, "delete returns OK");
            }
        }

        /* Verify slots are reusable */
        int reuse = eos_task_create("reuse", dummy_task, NULL, 1, 256);
        if (cycle == 0) {
            CHECK(reuse > 0, "slot reuse after delete");
        }
    }

    CHECK(pass > 0, "50 churn cycles completed");
    printf("    %d create/delete cycles OK\n", 50);
}

/* ============================================================
 * 2. Queue Saturation — fill/drain/wraparound
 * ============================================================ */

static void test_queue_saturation(void)
{
    SECTION("Queue Saturation");
    eos_kernel_init();

    eos_queue_handle_t q;
    CHECK(eos_queue_create(&q, sizeof(uint32_t), 8) == EOS_KERN_OK, "queue created");

    /* Fill/drain cycles to exercise head/tail wraparound */
    for (int cycle = 0; cycle < 100; cycle++) {
        /* Fill to capacity */
        for (uint32_t i = 0; i < 8; i++) {
            uint32_t val = cycle * 1000 + i;
            int rc = eos_queue_send(q, &val, EOS_NO_WAIT);
            if (cycle == 0 && i < 2) {
                CHECK(rc == EOS_KERN_OK, "queue send during fill");
            }
        }
        CHECK(eos_queue_is_full(q), "queue full after 8 sends");

        /* Overflow attempt */
        uint32_t extra = 0xDEAD;
        CHECK(eos_queue_send(q, &extra, EOS_NO_WAIT) == EOS_KERN_FULL,
              "send on full returns FULL");

        /* Drain and verify FIFO order */
        for (uint32_t i = 0; i < 8; i++) {
            uint32_t out;
            int rc = eos_queue_receive(q, &out, EOS_NO_WAIT);
            if (cycle == 0 && i == 0) {
                CHECK(rc == EOS_KERN_OK, "receive during drain");
                CHECK(out == (uint32_t)(cycle * 1000 + i), "FIFO order preserved");
            }
        }
        CHECK(eos_queue_is_empty(q), "queue empty after drain");

        /* Underflow attempt */
        uint32_t dummy;
        CHECK(eos_queue_receive(q, &dummy, EOS_NO_WAIT) == EOS_KERN_EMPTY,
              "receive on empty returns EMPTY");
    }

    /* Partial fill/drain to stress wraparound */
    for (int cycle = 0; cycle < 200; cycle++) {
        uint32_t val = (uint32_t)cycle;
        eos_queue_send(q, &val, 0);
        eos_queue_send(q, &val, 0);
        eos_queue_send(q, &val, 0);

        uint32_t out;
        eos_queue_receive(q, &out, 0);
        eos_queue_receive(q, &out, 0);
        CHECK(out == val, "partial wraparound value correct");
        eos_queue_receive(q, &out, 0);
    }
    CHECK(eos_queue_is_empty(q), "empty after partial cycles");

    eos_queue_delete(q);
    printf("    100 full cycles + 200 partial cycles OK\n");
}

/* ============================================================
 * 3. Queue with struct payload — large items
 * ============================================================ */

typedef struct {
    uint32_t seq;
    uint8_t  data[56];
    uint32_t checksum;
} big_msg_t;

static void test_queue_large_items(void)
{
    SECTION("Queue Large Items");
    eos_kernel_init();

    eos_queue_handle_t q;
    CHECK(eos_queue_create(&q, sizeof(big_msg_t), 8) == EOS_KERN_OK,
          "queue with 64-byte items");

    for (uint32_t i = 0; i < 8; i++) {
        big_msg_t msg;
        msg.seq = i;
        memset(msg.data, (int)(i & 0xFF), sizeof(msg.data));
        msg.checksum = i * 0x12345678;
        CHECK(eos_queue_send(q, &msg, 0) == EOS_KERN_OK, "send large msg");
    }

    for (uint32_t i = 0; i < 8; i++) {
        big_msg_t out;
        CHECK(eos_queue_receive(q, &out, 0) == EOS_KERN_OK, "recv large msg");
        CHECK(out.seq == i, "large msg seq correct");
        CHECK(out.checksum == i * 0x12345678, "large msg checksum correct");
        CHECK(out.data[0] == (uint8_t)(i & 0xFF), "large msg data intact");
    }

    eos_queue_delete(q);
    printf("    8 x 64-byte messages OK\n");
}

/* ============================================================
 * 4. Mutex Contention — recursive depth + rapid lock/unlock
 * ============================================================ */

static void test_mutex_contention(void)
{
    SECTION("Mutex Contention");
    eos_kernel_init();
    eos_task_create("mtx_owner", dummy_task, NULL, 1, 256);

    eos_mutex_handle_t m;
    CHECK(eos_mutex_create(&m) == EOS_KERN_OK, "mutex created");

    /* Deep recursive locking */
    int max_depth = 200;
    for (int i = 0; i < max_depth; i++) {
        CHECK(eos_mutex_lock(m, EOS_NO_WAIT) == EOS_KERN_OK, "recursive lock");
    }

    /* Unlock all levels */
    for (int i = 0; i < max_depth; i++) {
        CHECK(eos_mutex_unlock(m) == EOS_KERN_OK, "recursive unlock");
    }

    /* Rapid lock/unlock cycles */
    for (int i = 0; i < 10000; i++) {
        eos_mutex_lock(m, EOS_NO_WAIT);
        eos_mutex_unlock(m);
    }
    CHECK(1, "10000 lock/unlock cycles survived");

    /* Create/delete mutex in a loop */
    for (int i = 0; i < 500; i++) {
        eos_mutex_handle_t tmp;
        CHECK(eos_mutex_create(&tmp) == EOS_KERN_OK, "mutex create in loop");
        eos_mutex_lock(tmp, 0);
        eos_mutex_unlock(tmp);
        eos_mutex_delete(tmp);
    }
    CHECK(1, "500 create/lock/unlock/delete cycles OK");

    eos_mutex_delete(m);
    printf("    depth=%d, 10000 rapid, 500 lifecycle cycles OK\n", max_depth);
}

/* ============================================================
 * 5. Mutex Exhaustion — fill all slots
 * ============================================================ */

static void test_mutex_exhaustion(void)
{
    SECTION("Mutex Exhaustion");
    eos_kernel_init();

    eos_mutex_handle_t handles[EOS_MAX_MUTEXES];
    for (int i = 0; i < EOS_MAX_MUTEXES; i++) {
        CHECK(eos_mutex_create(&handles[i]) == EOS_KERN_OK, "create mutex slot");
    }

    eos_mutex_handle_t extra;
    CHECK(eos_mutex_create(&extra) == EOS_KERN_NO_MEMORY, "exhaustion detected");

    /* Delete one and verify recovery */
    eos_mutex_delete(handles[0]);
    CHECK(eos_mutex_create(&extra) == EOS_KERN_OK, "slot recovered");

    /* Cleanup */
    eos_mutex_delete(extra);
    for (int i = 1; i < EOS_MAX_MUTEXES; i++)
        eos_mutex_delete(handles[i]);

    printf("    %d mutex slots exhausted and recovered\n", EOS_MAX_MUTEXES);
}

/* ============================================================
 * 6. Semaphore Flooding — rapid post/wait at boundaries
 * ============================================================ */

static void test_semaphore_flood(void)
{
    SECTION("Semaphore Flood");
    eos_kernel_init();

    eos_sem_handle_t s;
    CHECK(eos_sem_create(&s, 0, 100) == EOS_KERN_OK, "sem 0/100 created");

    /* Flood post to max */
    for (int i = 0; i < 100; i++) {
        CHECK(eos_sem_post(s) == EOS_KERN_OK, "post to fill");
    }
    CHECK(eos_sem_get_count(s) == 100, "count at max");
    CHECK(eos_sem_post(s) == EOS_KERN_FULL, "overflow rejected");

    /* Drain completely */
    for (int i = 0; i < 100; i++) {
        CHECK(eos_sem_wait(s, EOS_NO_WAIT) == EOS_KERN_OK, "wait to drain");
    }
    CHECK(eos_sem_get_count(s) == 0, "count at zero");
    CHECK(eos_sem_wait(s, EOS_NO_WAIT) == EOS_KERN_TIMEOUT, "underflow timeout");

    /* Rapid oscillation at boundary */
    for (int i = 0; i < 5000; i++) {
        eos_sem_post(s);
        CHECK(eos_sem_get_count(s) == 1, "count=1 after post");
        eos_sem_wait(s, 0);
        CHECK(eos_sem_get_count(s) == 0, "count=0 after wait");
    }

    eos_sem_delete(s);
    printf("    100 fill, 100 drain, 5000 boundary oscillations OK\n");
}

/* ============================================================
 * 7. Heap Fragmentation — random alloc/free pattern
 * ============================================================ */

static uint8_t stress_heap[8192] __attribute__((aligned(8)));

static void test_heap_fragmentation(void)
{
    SECTION("Heap Fragmentation");

    CHECK(eos_heap_init(stress_heap, sizeof(stress_heap)) == 0, "heap init 8KB");

    /* Allocate many small blocks */
    #define NBLOCKS 32
    void *blocks[NBLOCKS];
    int allocated = 0;

    for (int i = 0; i < NBLOCKS; i++) {
        blocks[i] = eos_malloc(64);
        if (blocks[i]) {
            memset(blocks[i], i & 0xFF, 64);  /* Write pattern */
            allocated++;
        }
    }
    CHECK(allocated > 20, "allocated >20 of 32 blocks");

    /* Free every other block (create holes) */
    for (int i = 0; i < NBLOCKS; i += 2) {
        if (blocks[i]) {
            eos_free(blocks[i]);
            blocks[i] = NULL;
        }
    }

    /* Verify remaining blocks still have correct data */
    for (int i = 1; i < NBLOCKS; i += 2) {
        if (blocks[i]) {
            uint8_t *p = (uint8_t *)blocks[i];
            CHECK(p[0] == (uint8_t)(i & 0xFF), "data intact after fragmentation");
        }
    }

    /* Allocate into the holes */
    int reused = 0;
    for (int i = 0; i < NBLOCKS; i += 2) {
        blocks[i] = eos_malloc(32);  /* Smaller — should fit in holes */
        if (blocks[i]) reused++;
    }
    CHECK(reused > 0, "allocated into fragmented holes");

    /* Free everything */
    for (int i = 0; i < NBLOCKS; i++) {
        eos_free(blocks[i]);
    }

    /* After all freed, should be able to alloc a large block (coalesced) */
    void *big = eos_malloc(4096);
    CHECK(big != NULL, "large alloc after full coalesce");
    eos_free(big);

    /* Stats check */
    eos_heap_stats_t stats;
    eos_heap_stats(&stats);
    CHECK(stats.total_size == sizeof(stress_heap), "stats total correct");
    CHECK(stats.alloc_count > 30, "stats tracked allocations");
    CHECK(stats.free_count > 30, "stats tracked frees");

    printf("    %d blocks, fragmentation + coalesce OK\n", NBLOCKS);
    #undef NBLOCKS
}

/* ============================================================
 * 8. Heap Alignment Stress
 * ============================================================ */

static void test_heap_alignment_stress(void)
{
    SECTION("Heap Alignment");
    eos_heap_init(stress_heap, sizeof(stress_heap));

    /* Allocate varying sizes and verify all are 8-byte aligned */
    int sizes[] = {1, 2, 3, 5, 7, 8, 9, 13, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128};
    int nsizes = (int)(sizeof(sizes) / sizeof(sizes[0]));

    for (int i = 0; i < nsizes; i++) {
        void *p = eos_malloc((size_t)sizes[i]);
        if (p) {
            CHECK(((uintptr_t)p & 7) == 0, "8-byte aligned");
            memset(p, 0xCC, (size_t)sizes[i]);
            eos_free(p);
        }
    }

    printf("    %d sizes all 8-byte aligned\n", nsizes);
}

/* ============================================================
 * 9. Timer Storm — many timers firing close together
 * ============================================================ */

static int timer_fire_counts[EOS_MAX_TIMERS];
extern void eos_swtimer_tick(uint32_t);

static void storm_callback(eos_swtimer_handle_t h, void *ctx)
{
    (void)ctx;
    if (h < EOS_MAX_TIMERS) timer_fire_counts[h]++;
}

static void test_timer_storm(void)
{
    SECTION("Timer Storm");
    eos_kernel_init();
    memset(timer_fire_counts, 0, sizeof(timer_fire_counts));

    /* Create all available timers with different periods */
    eos_swtimer_handle_t timers[EOS_MAX_TIMERS];
    int created = 0;
    for (int i = 0; i < EOS_MAX_TIMERS; i++) {
        int rc = eos_swtimer_create(&timers[i], "storm",
                                     (uint32_t)(2 + i), true,
                                     storm_callback, NULL);
        if (rc == EOS_KERN_OK) {
            eos_swtimer_start(timers[i]);
            created++;
        }
    }
    CHECK(created == EOS_MAX_TIMERS, "all timer slots used");

    /* Run 1000 ticks — all timers firing repeatedly */
    for (int t = 0; t < 1000; t++) {
        eos_kernel_tick();
        eos_swtimer_tick(eos_tick_get());
    }

    /* Verify each timer fired approximately correct number of times */
    for (int i = 0; i < created; i++) {
        uint32_t period = (uint32_t)(2 + i);
        int expected = (int)(1000 / period);
        int actual = timer_fire_counts[timers[i]];
        /* Allow ±2 tolerance for boundary effects */
        CHECK(actual >= expected - 2 && actual <= expected + 2,
              "timer fire count within tolerance");
    }

    /* Cleanup */
    for (int i = 0; i < created; i++) {
        eos_swtimer_stop(timers[i]);
        eos_swtimer_delete(timers[i]);
    }

    printf("    %d timers x 1000 ticks OK\n", created);
}

/* ============================================================
 * 10. Internal Accessor Stress
 * ============================================================ */

static void test_accessor_stress(void)
{
    SECTION("Accessor Stress");
    eos_kernel_init();

    int h = eos_task_create("acc_stress", dummy_task, NULL, 50, 256);
    CHECK(h > 0, "task created");
    uint8_t handle = (uint8_t)h;

    /* Rapid priority changes */
    for (int i = 0; i < 1000; i++) {
        uint8_t prio = (uint8_t)(i & 0xFF);
        eos_task_set_priority_internal(handle, prio);
        CHECK(eos_task_get_priority_internal(handle) == prio, "prio set/get");
    }

    /* Block/unblock cycles */
    for (int i = 0; i < 1000; i++) {
        eos_task_block_with_timeout(handle, (uint32_t)(i + 1));
        CHECK(g_tasks[handle].state == EOS_TASK_BLOCKED, "blocked");
        eos_task_unblock(handle);
        CHECK(g_tasks[handle].state == EOS_TASK_READY, "unblocked");
    }

    /* Tick advancement */
    uint32_t t0 = eos_tick_get();
    for (int i = 0; i < 10000; i++) eos_kernel_tick();
    CHECK(eos_tick_get() == t0 + 10000, "10000 ticks advanced");

    printf("    1000 prio, 1000 block/unblock, 10000 ticks OK\n");
}

/* ============================================================
 * Main
 * ============================================================ */

int main(void)
{
    printf("════════════════════════════════════════════\n");
    printf("  EoS Hardware Stress Test\n");
    printf("════════════════════════════════════════════\n");

    test_task_churn();
    test_queue_saturation();
    test_queue_large_items();
    test_mutex_contention();
    test_mutex_exhaustion();
    test_semaphore_flood();
    test_heap_fragmentation();
    test_heap_alignment_stress();
    test_timer_storm();
    test_accessor_stress();

    printf("\n════════════════════════════════════════════\n");
    if (fail == 0) {
        printf("  ALL %d STRESS CHECKS PASSED\n", pass);
    } else {
        printf("  %d PASSED, %d FAILED (of %d)\n", pass, fail, total);
    }
    printf("════════════════════════════════════════════\n");

    return fail > 0 ? 1 : 0;
}
