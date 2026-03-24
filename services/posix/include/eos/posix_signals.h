// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file posix_signals.h
 * @brief Limited POSIX signal support for EoS
 *
 * Provides a minimal signal framework for inter-task notification
 * using EoS kernel primitives.
 */

#ifndef EOS_POSIX_SIGNALS_H
#define EOS_POSIX_SIGNALS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Supported signal numbers */
#define SIGUSR1   10
#define SIGUSR2   12
#define SIGALRM   14
#define SIGTERM   15

#define EOS_POSIX_NSIG       16   /* signal numbers 0..15 */
#define EOS_POSIX_SIG_COUNT   4   /* number of supported signals */

#define SIG_DFL  ((sighandler_t)0)
#define SIG_IGN  ((sighandler_t)1)
#define SIG_ERR  ((sighandler_t)-1)

typedef void (*sighandler_t)(int);

/* sigaction structure */
struct sigaction {
    sighandler_t sa_handler;
    uint32_t     sa_flags;
    uint32_t     sa_mask;
};

#define SA_RESTART  0x10000000

/**
 * Install a signal handler (simplified BSD-style).
 */
sighandler_t signal(int signum, sighandler_t handler);

/**
 * Install a signal handler via sigaction.
 */
int sigaction(int signum, const struct sigaction *act,
              struct sigaction *oldact);

/**
 * Send a signal to the calling task.
 */
int raise(int sig);

/**
 * Send a signal to a specific task (pthread_t / eos task handle).
 */
int kill(uint8_t tid, int sig);

/**
 * Schedule a SIGALRM after `seconds` seconds.
 * Returns previous alarm time remaining, or 0.
 */
unsigned int alarm(unsigned int seconds);

/**
 * Process pending signals for the current task.
 * Called internally at yield points.
 */
void eos_posix_signals_check(void);

/**
 * Initialize the signal subsystem (call once at startup).
 */
void eos_posix_signals_init(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_POSIX_SIGNALS_H */
