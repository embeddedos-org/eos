// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file os_services_test_hook.h
 * @brief Internal test seam for the watchdog clock -- not a public API.
 *
 * This header is deliberately not under services/os/include: nothing that
 * consumes the library should see it, and eos/os_services.h does not
 * declare it.
 *
 * The watchdog measures elapsed time, so a test that asserts it has *not*
 * expired is a race against the scheduler: if the process is descheduled
 * for longer than the timeout, eos_watchdog_check() expires the watchdog
 * correctly and the test fails. Lengthening the timeout widens that window
 * without closing it. Substituting the clock closes it.
 *
 * Pass NULL to restore the real monotonic clock.
 */

#ifndef EOS_OS_SERVICES_TEST_HOOK_H
#define EOS_OS_SERVICES_TEST_HOOK_H

#include <stdint.h>

/** Replace the watchdog's clock, or restore the real one with NULL. */
void eos_watchdog_set_test_clock(uint64_t (*fn)(void));

#endif /* EOS_OS_SERVICES_TEST_HOOK_H */
