// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file posix_signals.c
 * @brief Limited POSIX signal implementation for EoS
 */

#include <eos/posix_signals.h>
#include <eos/kernel.h>
#include <eos/hal.h>
#include <string.h>

#ifndef EINVAL
#define EINVAL 22
#endif
#ifndef ESRCH
#define ESRCH   3
#endif

/* ============================================================
 * Per-task signal state
 * ============================================================ */

typedef struct {
    sighandler_t handlers[EOS_POSIX_NSIG];
    uint32_t     pending;   /* bitmask of pending signals */
} posix_signal_state_t;

static posix_signal_state_t sig_state[EOS_MAX_TASKS];

/* Alarm timer handle per task (one alarm active at a time) */
static eos_swtimer_handle_t alarm_timers[EOS_MAX_TASKS];
static bool                 alarm_active[EOS_MAX_TASKS];

static bool sig_initialized = false;

static bool is_supported_signal(int signum) {
    return signum == SIGUSR1 || signum == SIGUSR2 ||
           signum == SIGALRM || signum == SIGTERM;
}

void eos_posix_signals_init(void) {
    memset(sig_state, 0, sizeof(sig_state));
    memset(alarm_active, 0, sizeof(alarm_active));

    for (int t = 0; t < EOS_MAX_TASKS; t++) {
        for (int s = 0; s < EOS_POSIX_NSIG; s++) {
            sig_state[t].handlers[s] = SIG_DFL;
        }
    }
    sig_initialized = true;
}

static void ensure_init(void) {
    if (!sig_initialized) eos_posix_signals_init();
}

/* ============================================================
 * signal() / sigaction()
 * ============================================================ */

sighandler_t signal(int signum, sighandler_t handler) {
    ensure_init();
    if (!is_supported_signal(signum)) return SIG_ERR;

    uint8_t tid = eos_task_get_current();
    if (tid >= EOS_MAX_TASKS) return SIG_ERR;

    sighandler_t old = sig_state[tid].handlers[signum];
    sig_state[tid].handlers[signum] = handler;
    return old;
}

int sigaction(int signum, const struct sigaction *act,
              struct sigaction *oldact) {
    ensure_init();
    if (!is_supported_signal(signum)) return -1;

    uint8_t tid = eos_task_get_current();
    if (tid >= EOS_MAX_TASKS) return -1;

    if (oldact) {
        oldact->sa_handler = sig_state[tid].handlers[signum];
        oldact->sa_flags   = 0;
        oldact->sa_mask    = 0;
    }
    if (act) {
        sig_state[tid].handlers[signum] = act->sa_handler;
    }
    return 0;
}

/* ============================================================
 * raise() / kill()
 * ============================================================ */

static void deliver_signal(uint8_t tid, int sig) {
    sighandler_t h = sig_state[tid].handlers[sig];
    if (h == SIG_IGN) return;

    if (h == SIG_DFL) {
        /* Default action for SIGTERM: delete the task */
        if (sig == SIGTERM) {
            eos_task_delete(tid);
        }
        return;
    }

    /* Call user handler */
    h(sig);
}

int raise(int sig) {
    ensure_init();
    if (!is_supported_signal(sig)) return -1;

    uint8_t tid = eos_task_get_current();
    deliver_signal(tid, sig);
    return 0;
}

int kill(uint8_t tid, int sig) {
    ensure_init();
    if (!is_supported_signal(sig)) return -1;
    if (tid >= EOS_MAX_TASKS) return -1;

    /* If target is current task, deliver immediately */
    if (tid == eos_task_get_current()) {
        deliver_signal(tid, sig);
        return 0;
    }

    /* Set pending flag; delivered at next yield/check */
    eos_irq_disable();
    sig_state[tid].pending |= (1U << sig);
    eos_irq_enable();
    return 0;
}

/* ============================================================
 * Pending signal processing
 * ============================================================ */

void eos_posix_signals_check(void) {
    if (!sig_initialized) return;

    uint8_t tid = eos_task_get_current();
    if (tid >= EOS_MAX_TASKS) return;

    eos_irq_disable();
    uint32_t pending = sig_state[tid].pending;
    sig_state[tid].pending = 0;
    eos_irq_enable();

    if (pending == 0) return;

    for (int sig = 0; sig < EOS_POSIX_NSIG; sig++) {
        if (pending & (1U << sig)) {
            deliver_signal(tid, sig);
        }
    }
}

/* ============================================================
 * alarm()
 * ============================================================ */

static void alarm_callback(eos_swtimer_handle_t handle, void *ctx) {
    (void)handle;
    uint8_t tid = (uint8_t)(uintptr_t)ctx;

    /* Set SIGALRM pending for the target task */
    eos_irq_disable();
    sig_state[tid].pending |= (1U << SIGALRM);
    eos_irq_enable();

    alarm_active[tid] = false;
}

unsigned int alarm(unsigned int seconds) {
    ensure_init();

    uint8_t tid = eos_task_get_current();
    if (tid >= EOS_MAX_TASKS) return 0;

    unsigned int remaining = 0;

    /* Cancel any existing alarm */
    if (alarm_active[tid]) {
        eos_swtimer_stop(alarm_timers[tid]);
        eos_swtimer_delete(alarm_timers[tid]);
        alarm_active[tid] = false;
    }

    if (seconds == 0) return remaining;

    eos_swtimer_handle_t th;
    if (eos_swtimer_create(&th, "posix_alarm", seconds * 1000, false,
                           alarm_callback, (void *)(uintptr_t)tid) != EOS_KERN_OK) {
        return 0;
    }

    alarm_timers[tid] = th;
    alarm_active[tid] = true;
    eos_swtimer_start(th);
    return remaining;
}
