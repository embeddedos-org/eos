// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/*
 * Regressions in the OS compatibility layers (POSIX, VxWorks, Linux IPC),
 * run against the real kernel with a no-op port.
 *
 * The layers are opt-in (EOS_ENABLE_POSIX / _VXWORKS / _LINUX_IPC, all OFF
 * by default), so nothing else in the default build compiles them. The
 * VxWorks and Linux IPC sources are compiled into this suite directly (their
 * names do not collide with libc). The POSIX sources define sem_init,
 * mq_open, timer_settime, alarm and friends, so they are #included with
 * those names renamed, the way test_posix_io.c and test_posix_time.c do.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ---- POSIX layer, renamed away from the host libc ---- */

/* posix_sync.c defines pthread_* types that clash with the host's, so it
 * lives in its own unit, test_compat_posix_sync.c. */
int compat_posix_sem_init_and_trywait(void);

#define mq_open     t_mq_open
#define mq_close    t_mq_close
#define mq_send     t_mq_send
#define mq_receive  t_mq_receive
#define mq_unlink   t_mq_unlink
#define pipe        t_pipe
#define shm_open    t_shm_open
#define shm_unlink  t_shm_unlink
#include "../services/posix/src/posix_ipc.c"

#define clock_gettime t_clock_gettime
#define timer_create  t_timer_create
#define timer_settime t_timer_settime
#define timer_delete  t_timer_delete
#define sleep         t_sleep
#define usleep        t_usleep
#define nanosleep     t_nanosleep
#define timespec      t_timespec
#define itimerspec    t_itimerspec
#define sigevent      t_sigevent
#define sigval        t_sigval
#define timer_t       t_timer_t
#include "../services/posix/src/posix_time.c"

#undef sigaction
#define signal       t_signal
#define sigaction    t_sigaction
#define raise        t_raise
#define kill         t_kill
#define alarm        t_alarm
#define sighandler_t t_sighandler_t
#include "../services/posix/src/posix_signals.c"

#include "eos/kernel_internal.h"
#include "eos/vxworks_sem.h"
#include "eos/vxworks_task.h"
#include "eos/linux_ipc.h"
#include "eos/linux_sysv_ipc.h"

void eos_swtimer_tick(uint32_t current_tick);   /* kernel/src/task.c */

/* ---- No-op port: the kernel runs, nothing context-switches ---- */

uint32_t eos_port_enter_critical(void) { return 0; }
void eos_port_exit_critical(uint32_t c) { (void)c; }
uint32_t *eos_port_init_stack(uint32_t *top, void (*e)(void *), void *a)
{
    (void)e; (void)a;
    return top;
}
void eos_port_start_first_task(void) {}
void eos_port_start_scheduler(void) {}
void eos_port_yield(void) {}

static void advance_to(uint32_t ms)
{
    g_tick = ms;
    eos_swtimer_tick(ms);
}

/* Every wrapper used to pass a maximum above INT32_MAX, which
 * eos_sem_create() refuses, so none of them could create a semaphore. */
static void test_semaphore_wrappers_create(void)
{
    eos_kernel_init();

    SEM_ID vs = semCCreate(0, 3);
    assert(vs != SEM_ID_NULL);
    assert(semTake(vs, 0) == OK);

    assert(compat_posix_sem_init_and_trywait() == 0);

    assert(eos_linux_ipc_init() == 0);
    assert(eos_eventfd_create(0, 0) >= 0);

    assert(eos_sysv_ipc_init() == 0);
    int id = eos_semget(42, 1, EOS_IPC_CREAT);
    assert(id >= 0);
    eos_semun_t arg;
    memset(&arg, 0, sizeof(arg));
    arg.val = 5;
    /* SETVAL deletes and recreates: it used to destroy the semaphore. */
    assert(eos_semctl(id, 0, EOS_SETVAL, arg) == 0);
    assert(eos_semctl(id, 0, EOS_GETVAL, arg) == 5);
    printf("[PASS] semaphore wrappers create semaphores\n");
}

static int vx_body(int a, int b, int c, int d, int e,
                   int f, int g, int h, int i, int j)
{
    (void)a; (void)b; (void)c; (void)d; (void)e;
    (void)f; (void)g; (void)h; (void)i; (void)j;
    return 0;
}

/* VxWorks and EoS both run the lowest number first; the mapping used to
 * invert it, putting a VxWorks priority-0 task at the idle priority. */
static void test_vxworks_priority_order(void)
{
    eos_kernel_init();
    TASK_ID hi = taskSpawn("tHigh", 0, 0, 2048, vx_body, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    TASK_ID lo = taskSpawn("tLow", 250, 0, 2048, vx_body, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    assert(hi != TASK_ID_ERROR && lo != TASK_ID_ERROR);
    uint8_t p_hi = eos_task_get_priority_internal((eos_task_handle_t)1);
    uint8_t p_lo = eos_task_get_priority_internal((eos_task_handle_t)2);
    assert(p_hi == 0 && p_lo == 250);
    assert(p_hi < eos_task_get_priority_internal(0)); /* idle */
    printf("[PASS] VxWorks priority order\n");
}

static void test_mq_names_and_short_messages(void)
{
    eos_kernel_init();
    struct mq_attr a = {0, 4, 64, 0};

    /* A name the registry cannot store whole is refused. It used to be
     * stored truncated: every open created a new queue, and it could never
     * be unlinked. */
    const char *too_long = "/sensor_events_q";            /* 16 chars */
    assert(strlen(too_long) == EOS_POSIX_MQ_NAME_MAX);
    assert(t_mq_open(too_long, EOS_O_CREAT | EOS_O_RDWR, 0, &a) == (mqd_t)-1);

    const char *longest = "/sensor_event_q";              /* 15 chars */
    mqd_t q1 = t_mq_open(longest, EOS_O_CREAT | EOS_O_RDWR, 0, &a);
    assert(q1 >= 0);
    assert(t_mq_open(longest, EOS_O_RDWR) == q1);
    assert(t_mq_unlink(longest) == 0);

    /* A short message is padded with zeros, not with whatever follows it
     * in the caller's memory (the queue copies a whole 64-byte item). */
    mqd_t q = t_mq_open("/q", EOS_O_CREAT | EOS_O_RDWR, 0, &a);
    assert(q >= 0);
    char buf[64];
    memset(buf, 'X', sizeof(buf));
    memcpy(buf, "hi", 3);
    assert(t_mq_send(q, buf, 3, 0) == 0);
    char out[64];
    memset(out, 0x55, sizeof(out));
    assert(t_mq_receive(q, out, sizeof(out), NULL) >= 0);
    assert(strcmp(out, "hi") == 0);
    for (size_t i = 3; i < sizeof(out); i++)
        assert(out[i] == 0);
    assert(t_mq_unlink("/q") == 0);

    /* shm names follow the same rule. */
    assert(t_shm_open("/sixteen_chars_x", EOS_O_CREAT, 0) == -1);
    printf("[PASS] mq names and short messages\n");
}

static void task_entry(void *arg) { (void)arg; }

static int g_fired;
static void count_fire(union t_sigval v) { (void)v; g_fired++; }

/* First expiry at it_value, then every it_interval. It used to fire first
 * at it_interval. */
static void test_timer_settime_first_expiry(void)
{
    eos_kernel_init();
    g_tick = 0;
    struct t_sigevent ev;
    memset(&ev, 0, sizeof(ev));
    ev.sigev_notify = SIGEV_NONE;
    ev.sigev_notify_function = count_fire;
    t_timer_t id;
    assert(t_timer_create(CLOCK_MONOTONIC, &ev, &id) == 0);

    struct t_itimerspec sp;
    memset(&sp, 0, sizeof(sp));
    sp.it_value.tv_sec = 5;
    sp.it_interval.tv_sec = 1;
    g_fired = 0;
    assert(t_timer_settime(id, 0, &sp, NULL) == 0);
    advance_to(1000);
    assert(g_fired == 0);
    advance_to(4999);
    assert(g_fired == 0);
    advance_to(5000);
    assert(g_fired == 1);
    advance_to(6000);
    assert(g_fired == 2);
    advance_to(7000);
    assert(g_fired == 3);

    /* Disarm stops the periodic phase too. */
    struct t_itimerspec off;
    memset(&off, 0, sizeof(off));
    assert(t_timer_settime(id, 0, &off, NULL) == 0);
    advance_to(9000);
    assert(g_fired == 3);
    assert(t_timer_delete(id) == 0);
    printf("[PASS] timer_settime first expiry at it_value\n");
}

/* A fired alarm frees its timer. Each one used to leak a timer, so after
 * EOS_MAX_TIMERS alarms alarm() silently stopped arming. */
static void test_alarm_does_not_leak_timers(void)
{
    eos_kernel_init();
    g_tick = 0;
    eos_task_handle_t t = (eos_task_handle_t)eos_task_create("a", task_entry, NULL, 5, 512);
    eos_task_set_current_internal(t);

    uint32_t now = 0;
    for (int i = 0; i < 3 * EOS_MAX_TIMERS; i++) {
        t_alarm(1);
        assert(alarm_active[t]);
        now += 2000;
        advance_to(now);
        assert(!alarm_active[t]);
        assert(sig_state[t].pending & (1U << SIGALRM));
        sig_state[t].pending = 0;
    }
    printf("[PASS] alarm does not leak timers\n");
}

int main(void)
{
    printf("=== EoS compat-layer regressions ===\n");
    test_semaphore_wrappers_create();
    test_vxworks_priority_order();
    test_mq_names_and_short_messages();
    test_timer_settime_first_expiry();
    test_alarm_does_not_leak_timers();
    printf("=== ALL COMPAT REGRESSION TESTS PASSED ===\n");
    return 0;
}
