// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
/**
 * @file smp_test.c
 * @brief The kernel on both ESP32 CPUs (build with -DEOS_SMP_CORES=2).
 *
 * Phase 1, 1 s, run by the tick alone:
 *  - Two "racer" tasks pinned to CPU 0 and CPU 1 increment one shared
 *    counter with no lock. Updates are lost only if both CPUs run tasks at
 *    the same time, so lost updates are the evidence of real parallelism
 *    (a one-CPU kernel would lose none: a time slice never splits an
 *    increment's window this often).
 *  - Two "locker" tasks, unpinned, increment another counter inside an
 *    eos_mutex: it must come out exact.
 *  - Three "spinner" tasks, unpinned, run context_test's register,
 *    zero-overhead-loop and CALL4/8/12 window checks; they migrate between
 *    CPUs, so a context saved on one CPU is restored on the other.
 *  - Every task records which CPUs it ran on: pinned ones only on theirs.
 * Phase 2: a task on CPU 0 and one on CPU 1 bounce a semaphore 1000 times
 *    (each wake-up crosses CPUs through the cross-CPU interrupt).
 * Phase 3: two unpinned tasks bounce semaphores 20000 times. Each block is
 *    undone by the other CPU, often before the blocking CPU has switched
 *    away; a scheduler that then let the other CPU pick the task up would
 *    run it on both CPUs, on one stack. Each task marks itself inside its
 *    loop body and checks the mark was clear.
 * Phase 4 widens that window on purpose: a task marks itself BLOCKED (the
 *    kernel's internal call), keeps running for 20 us, then yields; a task
 *    on the other CPU unblocks it inside that window and reschedules.
 *    What this does not show: with the scheduler's "still executing on
 *    another CPU" check removed, phases 3 and 4 still passed on the board,
 *    because the unblock's cross-CPU interrupt makes the first CPU save the
 *    task's context before the second can take it. The check stays as the
 *    guarantee; no test here reaches the case it covers.
 *
 * Prints CHECK lines and "SMP TEST PASSED" or "SMP TEST FAILED". Runs in
 * Espressif's QEMU (which models both CPUs) and on a board.
 */
#include <stdbool.h>
#include <stdint.h>
#include "eos/arch.h"
#include "eos/kernel.h"
#include "eos/kernel_internal.h"
#include "board_esp32.h"

#if EOS_SMP_CORES != 2
#error "smp_test.c needs -DEOS_SMP_CORES=2"
#endif

#define RACE_ROUNDS   200000U
#define LOCK_ROUNDS   20000U
#define SPINNERS      3
#define DEPTH         24U
#define PINGS         1000U

extern uint32_t eos_test_regcheck(uint32_t seed, uint32_t rounds);
extern uint32_t eos_test_win4(uint32_t n, uint32_t x);
extern uint32_t eos_test_win12(uint32_t n, uint32_t x);

static volatile uint32_t s_race, s_race_done[2], s_race_cpus[2];
static volatile uint32_t s_locked, s_lock_done, s_lock_cpus[2];
static volatile uint32_t s_spin_rounds[SPINNERS], s_spin_err[SPINNERS], s_spin_cpus[SPINNERS];
static eos_mutex_handle_t s_mutex;
static eos_sem_handle_t s_ping, s_pong;
static volatile uint32_t s_pinged, s_ping_cpus[2];

static void put(const char *s) { eos_esp32_console_puts(s); }
static void put_u(uint32_t v) { eos_esp32_console_put_u32(v); }

static int s_failures;
static void check(bool ok)
{
    put(ok ? "  PASS\n" : "  FAIL\n");
    if (!ok) s_failures++;
}

static uint32_t cpu_bit(void) { return 1U << eos_kernel_cpu_id(); }

/* ---- racers: unlocked increments from both CPUs ----------------------- */
static void racer(void *arg)
{
    uint32_t id = (uint32_t)(uintptr_t)arg;
    for (uint32_t i = 0; i < RACE_ROUNDS; i++) {
        uint32_t v = s_race;           /* read ... */
        __asm volatile ("" ::: "memory");
        s_race = v + 1U;               /* ... write: not atomic on purpose */
        s_race_cpus[id] |= cpu_bit();
    }
    s_race_done[id] = 1;
    for (;;) eos_task_delay_ms(1000);
}

/* ---- lockers: the same under a mutex ------------------------------------ */
static void locker(void *arg)
{
    uint32_t id = (uint32_t)(uintptr_t)arg;
    for (uint32_t i = 0; i < LOCK_ROUNDS; i++) {
        if (eos_mutex_lock(s_mutex, EOS_WAIT_FOREVER) != EOS_KERN_OK) break;
        uint32_t v = s_locked;
        __asm volatile ("" ::: "memory");
        s_locked = v + 1U;
        eos_mutex_unlock(s_mutex);
        s_lock_cpus[id] |= cpu_bit();
    }
    uint32_t crit = eos_port_enter_critical();
    s_lock_done++;
    eos_port_exit_critical(crit);
    for (;;) eos_task_delay_ms(1000);
}

/* ---- spinners: context integrity across migration ------------------------ */
static uint32_t mix(uint32_t n, uint32_t x, uint32_t k)
{
    return (x ^ (n * 0x9E3779B9U)) * (2U * k + 1U) + (k << n % 13U);
}
static uint32_t deep(uint32_t n, uint32_t x);
static uint32_t (*volatile s_deep)(uint32_t, uint32_t) = deep;
static uint32_t deep(uint32_t n, uint32_t x)
{
    uint32_t a = mix(n, x, 1), b = mix(n, x, 2), c = mix(n, x, 3), d = mix(n, x, 4);
    if (n == 0U) return a ^ b ^ c ^ d;
    uint32_t r = s_deep(n - 1U, a + b);
    return r * 3U + a + (b ^ c) - d;
}

static void spinner(void *arg)
{
    uint32_t id = (uint32_t)(uintptr_t)arg;
    uint32_t seed = 0x2000U * (id + 1U) + 7U + 9U * id;
    /* First results are the reference; window or frame damage later shows
     * as a difference (context_test.c checks them against C references). */
    uint32_t want = deep(DEPTH, seed), want4 = eos_test_win4(DEPTH, seed),
             want12 = eos_test_win12(DEPTH, seed);
    for (;;) {
        uint32_t r = eos_test_regcheck(seed, 20);
        if (r == 0U && deep(DEPTH, seed) != want) r = 4;
        if (r == 0U && eos_test_win4(DEPTH, seed) != want4) r = 5;
        if (r == 0U && eos_test_win12(DEPTH, seed) != want12) r = 6;
        if (r != 0U && s_spin_err[id] == 0U) s_spin_err[id] = r;
        s_spin_rounds[id]++;
        s_spin_cpus[id] |= cpu_bit();
    }
}

/* ---- ping-pong across CPUs ------------------------------------------------ */
static void pinger(void *arg)
{
    (void)arg;
    for (uint32_t i = 0; i < PINGS; i++) {
        eos_sem_post(s_ping);
        if (eos_sem_wait(s_pong, 1000) != EOS_KERN_OK) break;
        s_pinged++;
        s_ping_cpus[0] |= cpu_bit();
    }
    for (;;) eos_task_delay_ms(1000);
}

static void ponger(void *arg)
{
    (void)arg;
    for (;;) {
        if (eos_sem_wait(s_ping, EOS_WAIT_FOREVER) != EOS_KERN_OK) continue;
        s_ping_cpus[1] |= cpu_bit();
        eos_sem_post(s_pong);
    }
}

/* ---- storm: block/unblock across CPUs with no pinning ---------------------- */
#define STORM 20000U
static eos_sem_handle_t s_sa, s_sb;
static volatile uint32_t s_storm[2], s_inside[2], s_double[2], s_storm_cpus[2];

static void stormer(void *arg)
{
    uint32_t id = (uint32_t)(uintptr_t)arg;
    eos_sem_handle_t mine = id ? s_sb : s_sa, other = id ? s_sa : s_sb;
    for (uint32_t i = 0; i < STORM; i++) {
        if (id == 0U) eos_sem_post(other);
        if (eos_sem_wait(mine, 2000) != EOS_KERN_OK) break;
        /* A token, not the CPU number: a task may be preempted here and
         * resume on the other CPU, which is fine; a second instance of it
         * running at the same time would overwrite the token. */
        uint32_t token = (eos_esp32_ccount() << 1) | 1U;
        if (s_inside[id] != 0U) s_double[id]++;
        s_inside[id] = token;
        s_storm_cpus[id] |= cpu_bit();
        for (volatile int k = 0; k < 20; k++) {
        }
        if (s_inside[id] != token) s_double[id]++;
        s_inside[id] = 0;
        s_storm[id]++;
        if (id == 1U) eos_sem_post(other);
    }
    for (;;) eos_task_delay_ms(1000);
}

/* ---- widened window: READY while still executing ----------------------- */
#define WINDOWS 2000U
static volatile uint32_t s_win_rounds, s_win_double, s_win_token, s_win_go;
static volatile eos_task_handle_t s_win_task;

static void window_blocker(void *arg)
{
    (void)arg;
    s_win_task = eos_task_get_current();
    for (uint32_t i = 0; i < WINDOWS; i++) {
        uint32_t token = (eos_esp32_ccount() << 1) | 1U;
        if (s_win_token != 0U) s_win_double++;
        s_win_token = token;
        uint32_t crit = eos_port_enter_critical();
        eos_task_block_with_timeout(eos_task_get_current(), EOS_WAIT_FOREVER);
        /* "Unblock me": inside the lock, so a tick that switches this task
         * away right after the section still leaves someone to wake it. */
        s_win_go = 1;
        eos_port_exit_critical(crit);
        uint32_t t0 = eos_esp32_ccount();
        while (eos_esp32_ccount() - t0 < EOS_ESP32_CPU_MHZ * 20U) {
        }
        if (s_win_token != token) s_win_double++;
        s_win_token = 0;
        s_win_rounds++;
        eos_port_yield();                     /* the switch-away, late */
    }
    for (;;) eos_task_delay_ms(1000);
}

static void window_waker(void *arg)
{
    (void)arg;
    for (;;) {
        while (!s_win_go) {
        }
        s_win_go = 0;
        uint32_t crit = eos_port_enter_critical();
        if (eos_task_get_state(s_win_task) == EOS_TASK_BLOCKED) eos_task_unblock(s_win_task);
        eos_port_exit_critical(crit);
        eos_task_yield();                     /* this CPU picks a task now */
    }
}

static void put_cpus(uint32_t bits)
{
    put(bits == 1U ? "CPU0" : bits == 2U ? "CPU1" : bits == 3U ? "both" : "none");
}

static void monitor(void *arg)
{
    (void)arg;
    eos_task_delay_ms(1500);

    uint32_t total = 2U * RACE_ROUNDS, got = s_race;
    put("CHECK racers pinned to CPU0/CPU1 ran on ");
    put_cpus(s_race_cpus[0]);
    put("/");
    put_cpus(s_race_cpus[1]);
    put(", unlocked count ");
    put_u(got);
    put(" of ");
    put_u(total);
    put(" (");
    put_u(total - got);
    put(" lost: both CPUs ran at once)");
    check(s_race_done[0] && s_race_done[1] && s_race_cpus[0] == 1U &&
          s_race_cpus[1] == 2U && got < total);

    put("CHECK mutex across CPUs: ");
    put_u(s_locked);
    put(" of ");
    put_u(2U * LOCK_ROUNDS);
    put(", lockers ran on ");
    put_cpus(s_lock_cpus[0] | s_lock_cpus[1]);
    check(s_lock_done == 2U && s_locked == 2U * LOCK_ROUNDS);

    bool ok = true;
    put("CHECK spinners (registers, loop, CALL4/8/12 windows) migrating:");
    for (int i = 0; i < SPINNERS; i++) {
        put(" t");
        put_u((uint32_t)i);
        put(" rounds=");
        put_u(s_spin_rounds[i]);
        put(" err=");
        put_u(s_spin_err[i]);
        put(" on ");
        put_cpus(s_spin_cpus[i]);
        if (s_spin_err[i] != 0U || s_spin_rounds[i] < 10U) ok = false;
    }
    uint32_t any = s_spin_cpus[0] | s_spin_cpus[1] | s_spin_cpus[2];
    check(ok && any == 3U);

    eos_task_handle_t a = (eos_task_handle_t)eos_task_create("ping", pinger, NULL, 3, 1024);
    eos_task_handle_t b = (eos_task_handle_t)eos_task_create("pong", ponger, NULL, 3, 1024);
    bool pinned = eos_task_set_core_mask(a, 1U) == EOS_KERN_OK &&
                  eos_task_set_core_mask(b, 2U) == EOS_KERN_OK;
    uint32_t t0 = eos_tick_get();
    while (s_pinged < PINGS && eos_tick_get() - t0 < 3000U) eos_task_delay_ms(10);
    uint32_t ms = eos_tick_get() - t0;
    put("CHECK semaphore ping-pong CPU0<->CPU1: ");
    put_u(s_pinged);
    put(" of ");
    put_u(PINGS);
    put(" round trips in ");
    put_u(ms);
    put(" ms, ping on ");
    put_cpus(s_ping_cpus[0]);
    put(", pong on ");
    put_cpus(s_ping_cpus[1]);
    check(pinned && s_pinged == PINGS && s_ping_cpus[0] == 1U && s_ping_cpus[1] == 2U);

    if (eos_sem_create(&s_sa, 0, 1) != EOS_KERN_OK || eos_sem_create(&s_sb, 0, 1) != EOS_KERN_OK) {
        s_failures++;
    }
    eos_task_create("storm0", stormer, (void *)0, 2, 1024);
    eos_task_create("storm1", stormer, (void *)1, 2, 1024);
    t0 = eos_tick_get();
    while ((s_storm[0] < STORM || s_storm[1] < STORM) && eos_tick_get() - t0 < 8000U) {
        eos_task_delay_ms(20);
    }
    put("CHECK unpinned block/unblock storm: ");
    put_u(s_storm[0]);
    put("+");
    put_u(s_storm[1]);
    put(" of ");
    put_u(2U * STORM);
    put(" in ");
    put_u(eos_tick_get() - t0);
    put(" ms, ran on ");
    put_cpus(s_storm_cpus[0] | s_storm_cpus[1]);
    put(", a task on two CPUs at once: ");
    put_u(s_double[0] + s_double[1]);
    check(s_storm[0] == STORM && s_storm[1] == STORM && s_double[0] + s_double[1] == 0U);

    int wb = eos_task_create("winb", window_blocker, NULL, 2, 1024);
    int ww = eos_task_create("winw", window_waker, NULL, 2, 1024);
    bool wpin = wb >= 0 && ww >= 0 &&
                eos_task_set_core_mask((eos_task_handle_t)ww, 2U) == EOS_KERN_OK;
    t0 = eos_tick_get();
    while (s_win_rounds < WINDOWS && eos_tick_get() - t0 < 5000U) eos_task_delay_ms(20);
    put("CHECK READY-but-still-running window, widened to 20 us: ");
    put_u(s_win_rounds);
    put(" of ");
    put_u(WINDOWS);
    put(" rounds, a task on two CPUs at once: ");
    put_u(s_win_double);
    check(wpin && s_win_rounds == WINDOWS && s_win_double == 0U);
    if (ww >= 0) eos_task_delete((eos_task_handle_t)ww);

    put("CHECK API: idle tasks protected, bad masks refused");
    bool api = eos_task_delete(0) == EOS_KERN_INVALID && eos_task_delete(1) == EOS_KERN_INVALID &&
               eos_task_suspend(1) == EOS_KERN_INVALID &&
               eos_task_set_core_mask(a, 4U) == EOS_KERN_INVALID &&
               eos_task_set_core_mask(0, 3U) == EOS_KERN_INVALID;
    check(api);

    put(s_failures == 0 ? "SMP TEST PASSED\n" : "SMP TEST FAILED\n");
    for (;;) eos_task_delay_ms(1000);
}

int main(void)
{
    put("smp_test: starting kernel on 2 CPUs\n");
    if (eos_kernel_init() != EOS_KERN_OK) return 1;
    if (eos_mutex_create(&s_mutex) != EOS_KERN_OK) return 1;
    if (eos_sem_create(&s_ping, 0, 1) != EOS_KERN_OK) return 1;
    if (eos_sem_create(&s_pong, 0, 1) != EOS_KERN_OK) return 1;
    int r0 = eos_task_create("race0", racer, (void *)0, 5, 1024);
    int r1 = eos_task_create("race1", racer, (void *)1, 5, 1024);
    if (r0 < 0 || r1 < 0) return 1;
    if (eos_task_set_core_mask((eos_task_handle_t)r0, 1U) != EOS_KERN_OK) return 1;
    if (eos_task_set_core_mask((eos_task_handle_t)r1, 2U) != EOS_KERN_OK) return 1;
    for (uintptr_t i = 0; i < 2; i++) {
        if (eos_task_create("lock", locker, (void *)i, 5, 1024) < 0) return 1;
    }
    for (uintptr_t i = 0; i < SPINNERS; i++) {
        if (eos_task_create("spin", spinner, (void *)i, 6, 3072) < 0) return 1;
    }
    if (eos_task_create("monitor", monitor, NULL, 1, 2048) < 0) return 1;
    eos_kernel_start();
    return 1;
}
