// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/*
 * 64-bit time (T64 slice 1, #269): the RTC timestamp and the EoS POSIX
 * timespec are 64-bit.
 *
 * Two boundaries are pinned here:
 *   - 2038: the RTC conversion for 2038-01-19 03:14:07/08 passes INT32_MAX
 *     (2148693247 under the formula), which still fits the old uint32_t
 *     return, so the old 32-bit eos_rtc_get_unix_timestamp() would NOT
 *     have wrapped there; it would have wrapped only at 2107-01-01,
 *     which passes the old UINT32_MAX return.
 *   - 49.7 days: the full 32-bit millisecond tick range (0xFFFFFFFF ms)
 *     round-trips through clock_gettime()/timespec exactly, and a timespec
 *     past 49.7 days is representable — the tick counter itself stays
 *     32-bit in this slice, so the ms conversion is still its consumer.
 *
 * posix_time.c is #included with its POSIX names renamed (it defines
 * clock_gettime/sleep/usleep/nanosleep/timer_*, which would otherwise
 * replace the host libc's), the way test_posix_io.c does. The kernel/HAL
 * calls it needs are stubbed below, so the tick is under test control
 * and eos_posix — off by default — is not needed. The RTC conversion is
 * tested through the static-inline eos_rtc_time_to_unix() in
 * hal_extended.h; EOS_ENABLE_RTC is defined here because it defaults off.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

/* RTC is off by default (eos_config.h); the conversion helper lives in the
 * #if EOS_ENABLE_RTC section of hal_extended.h. */
#define EOS_ENABLE_RTC 1
#include <eos/hal_extended.h>

#define clock_gettime eos_test_clock_gettime
#define timer_create  eos_test_timer_create
#define timer_settime eos_test_timer_settime
#define timer_delete  eos_test_timer_delete
#define sleep         eos_test_sleep
#define usleep        eos_test_usleep
#define nanosleep     eos_test_nanosleep
#include "../services/posix/src/posix_time.c"

/* ---- Stubs for the kernel/HAL calls posix_time.c needs ---- */

static uint32_t stub_tick_ms = 0;
static uint32_t stub_last_delay_ms = 0;
static uint32_t stub_last_period_ms = 0;
static bool stub_last_auto_reload = false;

uint32_t eos_get_tick_ms(void) { return stub_tick_ms; }

void eos_task_delay_ms(uint32_t ms) { stub_last_delay_ms = ms; }

int eos_swtimer_create(eos_swtimer_handle_t *out, const char *name,
                       uint32_t period_ms, bool auto_reload,
                       eos_swtimer_callback_t callback, void *ctx) {
    (void)name; (void)callback; (void)ctx;
    stub_last_period_ms = period_ms;
    stub_last_auto_reload = auto_reload;
    *out = 0;
    return EOS_KERN_OK;
}

int eos_swtimer_start(eos_swtimer_handle_t handle) {
    (void)handle; return EOS_KERN_OK;
}

int eos_swtimer_stop(eos_swtimer_handle_t handle) {
    (void)handle; return EOS_KERN_OK;
}

int eos_swtimer_delete(eos_swtimer_handle_t handle) {
    (void)handle; return EOS_KERN_OK;
}

/* ---- timespec width ---- */

static void test_timespec_is_64_bit(void) {
    assert(sizeof(struct timespec) == 16);
    assert(sizeof(((struct timespec){0}).tv_sec) == 8);
    assert(sizeof(((struct timespec){0}).tv_nsec) == 8);

    /* 5,000,000,000 s is the year 2128: past the old uint32_t tv_sec, which
     * would have truncated this to 705032704. */
    struct timespec ts = { .tv_sec = 5000000000LL, .tv_nsec = 999999999 };
    assert(ts.tv_sec == 5000000000LL);
    assert(ts.tv_nsec == 999999999);
    struct timespec copy = ts;
    assert(copy.tv_sec == ts.tv_sec && copy.tv_nsec == ts.tv_nsec);
    printf("[PASS] timespec is 64-bit\n");
}

/* ---- RTC conversion: 2038 and 2106 boundaries ---- */

static void test_rtc_2038_boundary(void) {
    /* The conversion is the simplified formula (30-day months, no leap
     * years), so these values are NOT true Unix time — they pin the formula
     * computed in 64 bits. 2000-01-01 is 948758400 here vs 946684800 true. */
    eos_rtc_time_t t2000 = { .year = 2000, .month = 1, .day = 1,
                             .hour = 0, .minute = 0, .second = 0, .weekday = 6 };
    assert(eos_rtc_time_to_unix(&t2000) == 948758400ULL);

    /* 2038-01-19 03:14:07 is INT32_MAX as true Unix time; under the formula
     * it is 2148693247 — past INT32_MAX either way, so a signed 32-bit
     * time_t could not have held it. */
    eos_rtc_time_t t2038 = { .year = 2038, .month = 1, .day = 19,
                             .hour = 3, .minute = 14, .second = 7, .weekday = 2 };
    uint64_t ts = eos_rtc_time_to_unix(&t2038);
    assert(ts == 2148693247ULL);
    assert(ts > 2147483647ULL); /* INT32_MAX */

    /* One second past the 2038 wall. */
    t2038.second = 8;
    assert(eos_rtc_time_to_unix(&t2038) == 2148693248ULL);

    /* 2107-01-01 passes UINT32_MAX (4294967295): the old uint32_t return
     * type wrapped here. */
    eos_rtc_time_t t2107 = { .year = 2107, .month = 1, .day = 1,
                             .hour = 0, .minute = 0, .second = 0, .weekday = 5 };
    assert(eos_rtc_time_to_unix(&t2107) == 4323110400ULL);
    assert(eos_rtc_time_to_unix(&t2107) > 4294967295ULL); /* UINT32_MAX */
    printf("[PASS] RTC conversion passes the 2038 and 2106 boundaries\n");
}

/* ---- clock_gettime over the full 32-bit tick range ---- */

static void test_clock_gettime_full_tick_range(void) {
    struct timespec ts;

    /* The largest tick value: 0xFFFFFFFF ms = 49.71 days. */
    stub_tick_ms = 0xFFFFFFFFU;
    assert(clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
    assert(ts.tv_sec == 4294967);
    assert(ts.tv_nsec == 295000000);
    assert(timespec_to_ms(&ts) == 0xFFFFFFFFU);

    /* Just short of the wrap. */
    stub_tick_ms = 0xFFFFFF00U;
    assert(clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
    assert(ts.tv_sec == 4294967);
    assert(ts.tv_nsec == 40000000);
    assert(timespec_to_ms(&ts) == 0xFFFFFF00U);

    stub_tick_ms = 0;
    assert(clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
    assert(ts.tv_sec == 0 && ts.tv_nsec == 0);
    printf("[PASS] clock_gettime round-trips the full 32-bit tick range\n");
}

/* ---- a timespec past the 49.7-day tick is representable ---- */

static void test_timespec_beyond_tick_range(void) {
    /* 5,000,000 s is ~57.9 days — past the tick's 49.7-day wrap. The type
     * now holds it; the millisecond conversion is still the tick-domain
     * consumer (its wrap is the later T64 tick slice, not this one). */
    struct timespec ts = { .tv_sec = 5000000, .tv_nsec = 0 };
    assert(ts.tv_sec == 5000000);
    assert(ts.tv_nsec == 0);
    /* The ms conversion wraps, definedly: 5,000,000,000 ms mod 2^32. */
    assert(timespec_to_ms(&ts) == 705032704U);
    printf("[PASS] timespec holds values past the 49.7-day tick\n");
}

/* ---- sleeps and timers take the widened timespec ---- */

static void test_sleep_and_timer_take_64bit_timespec(void) {
    struct timespec req = { .tv_sec = 5, .tv_nsec = 500000000 };
    struct timespec rem;
    assert(nanosleep(&req, &rem) == 0);
    assert(stub_last_delay_ms == 5500);
    assert(rem.tv_sec == 0 && rem.tv_nsec == 0);

    assert(sleep(2) == 0);
    assert(stub_last_delay_ms == 2000);
    assert(usleep(1500) == 0);
    assert(stub_last_delay_ms == 1);
    assert(usleep(500) == 0);
    assert(stub_last_delay_ms == 1); /* minimum 1 ms granularity */

    /* A negative timespec is not a delay: it clamps to 0 ms, never a
     * huge wrapped delay. */
    struct timespec neg = { .tv_sec = -1, .tv_nsec = 0 };
    assert(timespec_to_ms(&neg) == 0);
    assert(nanosleep(&neg, &rem) == 0);
    assert(stub_last_delay_ms == 0);

    timer_t id;
    assert(timer_create(CLOCK_MONOTONIC, NULL, &id) == 0);
    struct itimerspec spec = { .it_interval = { .tv_sec = 2, .tv_nsec = 0 },
                               .it_value = { .tv_sec = 5, .tv_nsec = 0 } };
    assert(timer_settime(id, 0, &spec, NULL) == 0);
    assert(stub_last_period_ms == 2000);
    assert(stub_last_auto_reload);
    assert(timer_delete(id) == 0);
    printf("[PASS] sleep and timers take the 64-bit timespec\n");
}

int main(void) {
    test_timespec_is_64_bit();
    test_rtc_2038_boundary();
    test_clock_gettime_full_tick_range();
    test_timespec_beyond_tick_range();
    test_sleep_and_timer_take_64bit_timespec();
    printf("All test_posix_time tests passed.\n");
    return 0;
}
