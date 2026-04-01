// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file posix_time.h
 * @brief POSIX Time APIs for EoS
 *
 * Maps clock_gettime, timer_create/settime/delete, sleep, usleep,
 * and nanosleep onto EoS HAL tick and software timers.
 */

#ifndef EOS_POSIX_TIME_H
#define EOS_POSIX_TIME_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Clock IDs */
#define CLOCK_MONOTONIC  1
#define CLOCK_REALTIME   0

struct timespec {
    uint32_t tv_sec;
    uint32_t tv_nsec;
};

/* Timer ID — maps to eos_swtimer_handle_t */
typedef uint8_t timer_t;

struct itimerspec {
    struct timespec it_interval;  /* timer period */
    struct timespec it_value;     /* initial expiration */
};

/* sigevent (simplified) */
#define SIGEV_NONE    0
#define SIGEV_SIGNAL  1

struct sigevent {
    int    sigev_notify;
    int    sigev_signo;
    void (*sigev_notify_function)(union sigval);
};

union sigval {
    int   sival_int;
    void *sival_ptr;
};

/**
 * Get the current time for the specified clock.
 * Only CLOCK_MONOTONIC is supported (from eos_get_tick_ms).
 */
int clock_gettime(int clk_id, struct timespec *tp);

/**
 * Create a POSIX timer backed by an EoS software timer.
 */
int timer_create(int clockid, struct sigevent *sevp, timer_t *timerid);

/**
 * Arm or disarm a timer.
 */
int timer_settime(timer_t timerid, int flags,
                  const struct itimerspec *new_value,
                  struct itimerspec *old_value);

/**
 * Delete a POSIX timer.
 */
int timer_delete(timer_t timerid);

/**
 * Sleep for the specified number of seconds.
 */
unsigned int sleep(unsigned int seconds);

/**
 * Sleep for the specified number of microseconds.
 */
int usleep(uint32_t usec);

/**
 * High-resolution sleep (resolution limited to 1 ms on EoS).
 */
int nanosleep(const struct timespec *req, struct timespec *rem);

#ifdef __cplusplus
}
#endif

#endif /* EOS_POSIX_TIME_H */
