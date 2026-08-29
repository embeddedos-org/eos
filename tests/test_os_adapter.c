// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_os_adapter.c
 * @brief Contract tests for the OS adapter layer.
 *
 * The adapter registry has always held the EoS native adapter and nothing else,
 * so the abstraction had never been checked against a second implementation. An
 * interface with one implementation is a guess about what varies.
 *
 * These tests exercise the registry itself, then run the full vtable contract
 * against the POSIX adapter — which, unlike the native one on a host build,
 * actually schedules threads, so blocking, timeouts and wakeups can be observed
 * rather than assumed.
 */

#include "eos/os_adapter.h"

#include <stdio.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define CHECK(cond, msg)                                          \
    do {                                                          \
        tests_run++;                                              \
        if (!(cond)) {                                            \
            printf("  [FAIL] %s (line %d)\n", (msg), __LINE__);   \
            return 1;                                             \
        }                                                         \
        tests_passed++;                                           \
    } while (0)

extern const eos_os_adapter_t eos_native_adapter;
#ifdef EOS_HAVE_POSIX_ADAPTER
extern const eos_os_adapter_t eos_posix_adapter;
#endif

static int test_registry(void)
{
    printf("-- adapter registry --\n");

    eos_os_adapter_init();

    const eos_os_adapter_t *active = eos_os_adapter_get_active();
    CHECK(active != NULL, "an adapter is active after init");
    CHECK(strcmp(active->name, "eos") == 0, "native adapter is the default");

    CHECK(eos_os_adapter_register(NULL) != 0, "NULL adapter rejected");
    CHECK(eos_os_adapter_set_active("nonexistent") != 0, "unknown name rejected");
    CHECK(eos_os_adapter_set_active(NULL) != 0, "NULL name rejected");

#ifdef EOS_HAVE_POSIX_ADAPTER
    CHECK(eos_os_adapter_register(&eos_posix_adapter) == 0, "posix adapter registers");
    CHECK(eos_os_adapter_set_active("posix") == 0, "switch to posix");
    CHECK(strcmp(eos_os_adapter_get_active()->name, "posix") == 0, "posix is active");
    CHECK(eos_os_adapter_set_active("eos") == 0, "switch back to native");
    CHECK(strcmp(eos_os_adapter_get_active()->name, "eos") == 0, "native is active");
#endif

    printf("  [PASS] registry\n");
    return 0;
}

#ifdef EOS_HAVE_POSIX_ADAPTER

/* ---- The vtable must be complete. A NULL entry is a silent no-op at runtime,
 *      which is how an adapter passes review while doing nothing. ---- */
static int test_vtable_is_complete(void)
{
    printf("-- posix vtable completeness --\n");
    const eos_os_adapter_t *a = &eos_posix_adapter;

    CHECK(a->name != NULL, "name");
    CHECK(a->task_create && a->task_delete && a->task_suspend && a->task_resume,
          "task lifecycle");
    CHECK(a->task_yield && a->task_delay_ms && a->task_get_current && a->task_get_name,
          "task accessors");
    CHECK(a->mutex_create && a->mutex_lock && a->mutex_unlock && a->mutex_delete,
          "mutex");
    CHECK(a->sem_create && a->sem_wait && a->sem_post && a->sem_delete && a->sem_get_count,
          "semaphore");
    CHECK(a->queue_create && a->queue_send && a->queue_receive && a->queue_delete,
          "queue");
    CHECK(a->queue_count && a->queue_is_full && a->queue_is_empty, "queue accessors");
    CHECK(a->timer_create && a->timer_start && a->timer_stop && a->timer_delete,
          "timer");
    CHECK(a->get_tick_ms && a->delay_ms && a->irq_disable && a->irq_enable, "system");

    printf("  [PASS] vtable complete\n");
    return 0;
}

static volatile int g_task_ran = 0;
static void counting_task(void *arg)
{
    (void)arg;
    g_task_ran++;
}

static int test_tasks_actually_run(void)
{
    printf("-- posix tasks --\n");
    const eos_os_adapter_t *a = &eos_posix_adapter;

    g_task_ran = 0;
    const int h = a->task_create("worker", counting_task, NULL, 5, 65536);
    CHECK(h >= 0, "task created");
    CHECK(strcmp(a->task_get_name((uint8_t)h), "worker") == 0, "task name recorded");

    /* task_delete joins, so after it returns the entry function has run. */
    CHECK(a->task_delete((uint8_t)h) == 0, "task deleted");
    CHECK(g_task_ran == 1, "entry function actually executed");

    CHECK(a->task_create(NULL, NULL, NULL, 5, 4096) < 0, "NULL entry rejected");
    CHECK(a->task_delete(200) != 0, "bogus handle rejected");
    CHECK(strcmp(a->task_get_name(200), "invalid") == 0, "bogus name is 'invalid'");

    printf("  [PASS] tasks run and clean up\n");
    return 0;
}

static int test_mutex(void)
{
    printf("-- posix mutex --\n");
    const eos_os_adapter_t *a = &eos_posix_adapter;

    uint8_t m = 0;
    CHECK(a->mutex_create(&m) == 0, "created");
    CHECK(a->mutex_lock(m, EOS_OSA_WAIT_FOREVER) == 0, "locked");
    /* Recursive, matching the EoS mutex which counts nesting. */
    CHECK(a->mutex_lock(m, EOS_OSA_NO_WAIT) == 0, "recursive re-lock");
    CHECK(a->mutex_unlock(m) == 0, "inner unlock");
    CHECK(a->mutex_unlock(m) == 0, "outer unlock");
    CHECK(a->mutex_delete(m) == 0, "deleted");
    CHECK(a->mutex_lock(m, EOS_OSA_NO_WAIT) != 0, "deleted mutex rejects lock");
    CHECK(a->mutex_create(NULL) != 0, "NULL out rejected");

    printf("  [PASS] mutex\n");
    return 0;
}

static int test_semaphore_ceiling(void)
{
    printf("-- posix semaphore --\n");
    const eos_os_adapter_t *a = &eos_posix_adapter;

    uint8_t s = 0;
    CHECK(a->sem_create(&s, 0, 2) == 0, "created 0/2");
    CHECK(a->sem_get_count(s) == 0, "starts empty");

    /* A wait on an empty semaphore with no timeout must fail, not block. */
    CHECK(a->sem_wait(s, EOS_OSA_NO_WAIT) != 0, "empty NO_WAIT fails");

    CHECK(a->sem_post(s) == 0, "post 1");
    CHECK(a->sem_post(s) == 0, "post 2");
    CHECK(a->sem_get_count(s) == 2, "count is 2");

    /* POSIX semaphores have no ceiling of their own. The adapter enforces the
     * one the caller asked for, or the counting invariant would silently not
     * hold on this backend while holding on the native one. */
    CHECK(a->sem_post(s) != 0, "post beyond max refused");
    CHECK(a->sem_get_count(s) == 2, "count unchanged after refusal");

    CHECK(a->sem_wait(s, EOS_OSA_NO_WAIT) == 0, "take 1");
    CHECK(a->sem_wait(s, EOS_OSA_NO_WAIT) == 0, "take 2");
    CHECK(a->sem_wait(s, EOS_OSA_NO_WAIT) != 0, "empty again");

    CHECK(a->sem_create(&s, 3, 2) != 0, "initial > max rejected");
    CHECK(a->sem_delete(s) == 0, "deleted");

    printf("  [PASS] semaphore honours its ceiling\n");
    return 0;
}

static int test_queue(void)
{
    printf("-- posix queue --\n");
    const eos_os_adapter_t *a = &eos_posix_adapter;

    uint8_t q = 0;
    CHECK(a->queue_create(&q, sizeof(uint32_t), 3) == 0, "created");
    CHECK(a->queue_is_empty(q), "starts empty");
    CHECK(!a->queue_is_full(q), "not full");

    for (uint32_t i = 1; i <= 3u; i++) {
        CHECK(a->queue_send(q, &i, EOS_OSA_NO_WAIT) == 0, "send");
    }
    CHECK(a->queue_count(q) == 3, "count is 3");
    CHECK(a->queue_is_full(q), "full at capacity");

    /* A send to a full queue with no timeout must fail rather than block. */
    const uint32_t overflow = 99;
    CHECK(a->queue_send(q, &overflow, EOS_OSA_NO_WAIT) != 0, "full NO_WAIT send fails");

    /* And with a timeout it must fail after roughly that long, not hang. */
    const uint32_t before = a->get_tick_ms();
    CHECK(a->queue_send(q, &overflow, 50) != 0, "full timed send times out");
    const uint32_t elapsed = a->get_tick_ms() - before;
    CHECK(elapsed >= 40u, "timed send actually waited");

    /* FIFO order. */
    for (uint32_t i = 1; i <= 3u; i++) {
        uint32_t got = 0;
        CHECK(a->queue_receive(q, &got, EOS_OSA_NO_WAIT) == 0, "receive");
        CHECK(got == i, "FIFO order preserved");
    }
    CHECK(a->queue_is_empty(q), "empty again");

    uint32_t sink = 0;
    CHECK(a->queue_receive(q, &sink, EOS_OSA_NO_WAIT) != 0, "empty receive fails");
    CHECK(a->queue_create(&q, 0, 4) != 0, "zero item size rejected");
    CHECK(a->queue_delete(q) == 0, "deleted");

    printf("  [PASS] queue FIFO, bounds and timeouts\n");
    return 0;
}

static volatile int g_timer_fires = 0;
static void timer_cb(uint8_t handle, void *ctx)
{
    (void)handle; (void)ctx;
    g_timer_fires++;
}

static int test_timers(void)
{
    printf("-- posix timers --\n");
    const eos_os_adapter_t *a = &eos_posix_adapter;

    /* One-shot: fires once and stays quiet. */
    g_timer_fires = 0;
    uint8_t t = 0;
    CHECK(a->timer_create(&t, "oneshot", 30, false, timer_cb, NULL) == 0, "created");
    CHECK(a->timer_start(t) == 0, "started");
    a->delay_ms(150);
    CHECK(g_timer_fires == 1, "one-shot fired exactly once");
    CHECK(a->timer_delete(t) == 0, "deleted");

    /* Auto-reload: fires repeatedly, and stops when told to. */
    g_timer_fires = 0;
    CHECK(a->timer_create(&t, "repeat", 25, true, timer_cb, NULL) == 0, "created");
    CHECK(a->timer_start(t) == 0, "started");
    a->delay_ms(160);
    const int during = g_timer_fires;
    CHECK(during >= 3, "auto-reload fired repeatedly");
    CHECK(a->timer_stop(t) == 0, "stopped");
    a->delay_ms(80);
    CHECK(g_timer_fires <= during + 1, "no further fires after stop");
    CHECK(a->timer_delete(t) == 0, "deleted");

    CHECK(a->timer_create(NULL, "x", 10, false, timer_cb, NULL) != 0, "NULL out rejected");
    CHECK(a->timer_create(&t, "x", 0, false, timer_cb, NULL) != 0, "zero period rejected");

    printf("  [PASS] one-shot and auto-reload timers\n");
    return 0;
}

static int test_tick_is_monotonic(void)
{
    printf("-- posix tick --\n");
    const eos_os_adapter_t *a = &eos_posix_adapter;

    const uint32_t t0 = a->get_tick_ms();
    a->delay_ms(60);
    const uint32_t t1 = a->get_tick_ms();

    CHECK(t1 - t0 >= 50u, "tick advanced across a delay");
    CHECK(t1 - t0 < 5000u, "tick advanced by a sane amount");

    /* No-ops by design on a hosted process — a user-space program cannot mask
     * interrupts. They must at least be callable without crashing. */
    a->irq_disable();
    a->irq_enable();
    tests_run++; tests_passed++;

    printf("  [PASS] monotonic tick\n");
    return 0;
}

#endif /* EOS_HAVE_POSIX_ADAPTER */

/* Mock port layer, matching tests/test_kernel.c. The native adapter links the
 * EoS kernel, which expects a port; none of the tests below schedule native
 * tasks, so no-ops are sufficient and honest. */
uint32_t eos_port_enter_critical(void) { return 0; }
void eos_port_exit_critical(uint32_t state) { (void)state; }
void eos_port_yield(void) { }
void eos_port_start_scheduler(void) { }
void eos_port_start_first_task(void) { }
uint32_t *eos_port_init_stack(uint32_t *s, void (*e)(void *), void *a)
{
    (void)e; (void)a; return s - 17;
}

int main(void)
{
    printf("=== EoS OS Adapter Tests ===\n");

    if (test_registry()) return 1;

#ifdef EOS_HAVE_POSIX_ADAPTER
    if (test_vtable_is_complete()) return 1;
    if (test_tasks_actually_run()) return 1;
    if (test_mutex()) return 1;
    if (test_semaphore_ceiling()) return 1;
    if (test_queue()) return 1;
    if (test_timers()) return 1;
    if (test_tick_is_monotonic()) return 1;
#else
    printf("  [SKIP] POSIX adapter not built on this platform\n");
#endif

    printf("\n=== %d/%d checks passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
