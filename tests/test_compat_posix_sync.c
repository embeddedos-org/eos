// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/*
 * Part of test_compat_regressions: posix_sync.c in a unit of its own,
 * because its pthread_* types clash with the host headers the rest of the
 * suite includes. sem_init is renamed away from the host libc.
 */

#define sem_init     t_sem_init
#define sem_destroy  t_sem_destroy
#define sem_wait     t_sem_wait
#define sem_trywait  t_sem_trywait
#define sem_post     t_sem_post
#define sem_getvalue t_sem_getvalue
#define sem_t        t_sem_t
#include "../services/posix/src/posix_sync.c"

int compat_posix_sem_init_and_trywait(void);

/* sem_init() used to pass a maximum above INT32_MAX, which eos_sem_create()
 * refuses, so it always failed. Returns 0 when a semaphore initialised to 1
 * can be taken once. */
int compat_posix_sem_init_and_trywait(void)
{
    t_sem_t s;
    if (t_sem_init(&s, 0, 1) != 0) return -1;
    if (t_sem_trywait(&s) != 0) return -2;
    return 0;
}
