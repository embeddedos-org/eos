// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file posix_time.c
 * @brief POSIX Time implementation for EoS
 */

#include <eos/posix_time.h>
#include <eos/kernel.h>
#include <eos/hal.h>

#ifndef EINVAL
#define EINVAL 22
#endif
#ifndef ENOMEM
#define ENOMEM 12
#endif
#ifndef ENOSYS
#define ENOSYS 38
#endif

/* ============================================================
 * Helpers
 * ============================================================ */

static void ms_to_timespec(uint32_t ms, struct timespec *ts) {
    ts->tv_sec  = ms / 1000;
    ts->tv_nsec = (ms % 1000) * 1000000U;
}

static uint32_t timespec_to_ms(const struct timespec *ts) {
    return ts->tv_sec * 1000 + ts->tv_nsec / 1000000U;
}

/* ============================================================
 * clock_gettime
 * ============================================================ */

int clock_gettime(int clk_id, struct timespec *tp) {
    if (!tp) return -1;

    if (clk_id != CLOCK_MONOTONIC && clk_id != CLOCK_REALTIME) return -1;

    uint32_t ms = eos_get_tick_ms();
    ms_to_timespec(ms, tp);
    return 0;
}

/* ============================================================
 * POSIX timer → EoS software timer
 * ============================================================ */

#define POSIX_TIMER_MAX  EOS_MAX_TIMERS

typedef struct {
    bool                  active;
    eos_swtimer_handle_t  eos_handle;
    struct sigevent       sevp;
    struct itimerspec     spec;
} posix_timer_entry_t;

static posix_timer_entry_t posix_timers[POSIX_TIMER_MAX];
static bool posix_timers_initialized = false;

static void posix_timers_init_once(void) {
    if (!posix_timers_initialized) {
        for (int i = 0; i < POSIX_TIMER_MAX; i++) {
            posix_timers[i].active = false;
        }
        posix_timers_initialized = true;
    }
}

static void posix_timer_callback(eos_swtimer_handle_t handle, void *ctx) {
    (void)handle;
    posix_timer_entry_t *te = (posix_timer_entry_t *)ctx;

    if (te->sevp.sigev_notify == SIGEV_SIGNAL) {
        /* Raise signal in current context — limited, best-effort */
        /* In a full implementation this would target a specific task */
    }

    if (te->sevp.sigev_notify_function) {
        union sigval sv;
        sv.sival_int = 0;
        te->sevp.sigev_notify_function(sv);
    }
}

int timer_create(int clockid, struct sigevent *sevp, timer_t *timerid) {
    (void)clockid;
    if (!timerid) return -1;

    posix_timers_init_once();

    int slot = -1;
    for (int i = 0; i < POSIX_TIMER_MAX; i++) {
        if (!posix_timers[i].active) { slot = i; break; }
    }
    if (slot < 0) return -1;

    posix_timer_entry_t *te = &posix_timers[slot];

    if (sevp) {
        te->sevp = *sevp;
    } else {
        te->sevp.sigev_notify          = SIGEV_NONE;
        te->sevp.sigev_signo           = 0;
        te->sevp.sigev_notify_function = NULL;
    }

    /* Create the underlying EoS timer (initially stopped, 1ms placeholder) */
    eos_swtimer_handle_t sh;
    if (eos_swtimer_create(&sh, "posix_tmr", 1, false,
                           posix_timer_callback, te) != EOS_KERN_OK) {
        return -1;
    }

    te->eos_handle = sh;
    te->active     = true;
    *timerid       = (timer_t)slot;
    return 0;
}

int timer_settime(timer_t timerid, int flags,
                  const struct itimerspec *new_value,
                  struct itimerspec *old_value) {
    (void)flags;
    if (timerid >= POSIX_TIMER_MAX) return -1;

    posix_timer_entry_t *te = &posix_timers[timerid];
    if (!te->active) return -1;

    if (old_value) {
        *old_value = te->spec;
    }

    /* Stop current timer */
    eos_swtimer_stop(te->eos_handle);

    if (!new_value) return 0;

    te->spec = *new_value;

    uint32_t initial_ms  = timespec_to_ms(&new_value->it_value);
    uint32_t interval_ms = timespec_to_ms(&new_value->it_interval);

    if (initial_ms == 0) {
        /* Disarm */
        return 0;
    }

    /* Delete old timer and create new one with correct period */
    eos_swtimer_delete(te->eos_handle);

    bool auto_reload = (interval_ms > 0);
    uint32_t period  = auto_reload ? interval_ms : initial_ms;

    eos_swtimer_handle_t sh;
    if (eos_swtimer_create(&sh, "posix_tmr", period, auto_reload,
                           posix_timer_callback, te) != EOS_KERN_OK) {
        te->active = false;
        return -1;
    }

    te->eos_handle = sh;
    eos_swtimer_start(sh);
    return 0;
}

int timer_delete(timer_t timerid) {
    if (timerid >= POSIX_TIMER_MAX) return -1;

    posix_timer_entry_t *te = &posix_timers[timerid];
    if (!te->active) return -1;

    eos_swtimer_stop(te->eos_handle);
    eos_swtimer_delete(te->eos_handle);
    te->active = false;
    return 0;
}

/* ============================================================
 * sleep / usleep / nanosleep
 * ============================================================ */

unsigned int sleep(unsigned int seconds) {
    eos_task_delay_ms(seconds * 1000);
    return 0;
}

int usleep(uint32_t usec) {
    uint32_t ms = usec / 1000;
    if (ms == 0 && usec > 0) ms = 1; /* minimum 1ms granularity */
    eos_task_delay_ms(ms);
    return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
    if (!req) return -1;

    uint32_t ms = timespec_to_ms(req);
    if (ms == 0 && (req->tv_sec > 0 || req->tv_nsec > 0)) ms = 1;

    eos_task_delay_ms(ms);

    if (rem) {
        rem->tv_sec  = 0;
        rem->tv_nsec = 0;
    }
    return 0;
}
