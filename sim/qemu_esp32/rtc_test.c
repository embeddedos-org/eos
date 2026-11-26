// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
/**
 * @file rtc_test.c
 * @brief On-board check of the ESP32 touch pads, light sleep and deep sleep.
 *
 * - Every pad is read and must give a count; repeated readings of a pad
 *   must agree within 3% (T1 and T2 are reported, not judged: on a DevKit
 *   the BOOT button and the LED sit on those pads).
 * - The measurement must respond to a load without a finger: T0's internal
 *   pull-down (in its RTC pad register) drains the pad as it charges, so
 *   the count must move by more than 10%.
 * - Finger detection needs a person: the test watches T0 (GPIO4) for 5 s and
 *   passes if the count drops below 70% of its idle value; with nobody
 *   there it prints NOT RUN, not PASS.
 *
 * - Light sleep for 200 ms: the RTC timer must show 200 ms passed (+-10%)
 *   while CCOUNT advanced by under a tenth of that (the CPU really
 *   stopped), and the console must still work (the PLL and UART0 divider
 *   came back).
 * - Deep sleep for 1 s: the test stores a tag, its failure count and the RTC
 *   time in RTC slow memory, sleeps, and on the next boot requires reset
 *   reason 5 (DEEPSLEEP_RESET), the timer as wake-up cause, the tag intact
 *   and about 1 s on the RTC timer; then it reports for both boots.
 *
 * Prints one CHECK line per item and "RTC TEST PASSED" or "RTC TEST FAILED".
 * Board only: QEMU does not model the touch sensor.
 */
#include <stdbool.h>
#include <stdint.h>
#include "board_esp32.h"
#include "hal_esp32_internal.h"

static int s_failures;

/* Survives deep sleep. */
#define RTC_TAG 0x45305354U   /* "E0ST" */
static uint32_t s_tag EOS_ESP32_RTC_NOINIT;
static uint32_t s_saved_failures EOS_ESP32_RTC_NOINIT;
static uint64_t s_sleep_start EOS_ESP32_RTC_NOINIT;

static void put(const char *s) { eos_esp32_console_puts(s); }
static void put_u(uint32_t v) { eos_esp32_console_put_u32(v); }

static void check(bool ok)
{
    put(ok ? "  PASS\n" : "  FAIL\n");
    if (!ok) s_failures++;
}

static uint32_t touch_avg(uint8_t pad, uint32_t n, uint16_t *lo, uint16_t *hi)
{
    uint32_t sum = 0;
    *lo = 0xFFFF;
    *hi = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint16_t v = 0;
        if (eos_esp32_touch_read(pad, &v) != 0) return 0;
        sum += v;
        if (v < *lo) *lo = v;
        if (v > *hi) *hi = v;
    }
    return sum / n;
}

static void test_touch_pads(void)
{
    put("CHECK touch pads read and stable (5 reads):");
    bool ok = eos_esp32_touch_init() == 0;
    for (uint8_t pad = 0; ok && pad < 10U; pad++) {
        uint16_t lo, hi;
        uint32_t avg = touch_avg(pad, 5, &lo, &hi);
        put(" T");
        put_u(pad);
        put("=");
        put_u(avg);
        if (pad == 1U || pad == 2U) {
            put("(info)");
            continue;
        }
        if (avg == 0U || (uint32_t)(hi - lo) * 100U > avg * 3U) {
            put("(bad)");
            ok = false;
        }
    }
    check(ok);
}

static void test_touch_load(void)
{
    put("CHECK touch T0 count moves with the pad's pull-down on:");
    uint16_t lo, hi;
    uint32_t idle = touch_avg(0, 4, &lo, &hi);
    esp32_rtc_pad_pull(4, false, true);
    uint32_t loaded = touch_avg(0, 4, &lo, &hi);
    esp32_rtc_pad_pull(4, false, false);
    uint32_t back = touch_avg(0, 4, &lo, &hi);
    put(" idle ");
    put_u(idle);
    put(", pulled down ");
    put_u(loaded);
    put(", released ");
    put_u(back);
    uint32_t diff = loaded > idle ? loaded - idle : idle - loaded;
    uint32_t drift = back > idle ? back - idle : idle - back;
    check(idle != 0U && diff * 10U > idle && drift * 100U < idle * 3U);
}

static void test_touch_finger(void)
{
    uint16_t lo, hi;
    uint32_t idle = touch_avg(0, 4, &lo, &hi);
    put("touch_test: touch GPIO4 (T0) now, 5 s\n");
    uint32_t min = idle;
    uint32_t t0 = eos_esp32_ccount(), ms = 0;
    while (ms < 5000U) {
        uint16_t v;
        if (eos_esp32_touch_read(0, &v) == 0 && v < min) min = v;
        uint32_t now = eos_esp32_ccount();
        while (now - t0 >= EOS_ESP32_CPU_MHZ * 1000U) { t0 += EOS_ESP32_CPU_MHZ * 1000U; ms++; }
    }
    put("CHECK touch T0 finger: idle ");
    put_u(idle);
    put(", lowest ");
    put_u(min);
    if (min * 10U < idle * 7U) {
        check(true);
    } else {
        put("  NOT RUN (no touch seen)\n");
    }
}

static void test_light_sleep(void)
{
    put("CHECK light sleep 200 ms:");
    uint32_t hz = eos_esp32_rtc_slow_hz();
    uint64_t r0 = eos_esp32_rtc_time();
    uint32_t c0 = eos_esp32_ccount();
    int rc = eos_esp32_light_sleep_us(200000U);
    uint32_t cycles = eos_esp32_ccount() - c0;
    uint64_t r1 = eos_esp32_rtc_time();
    uint32_t ms = hz ? (uint32_t)((r1 - r0) * 1000U / hz) : 0U;
    uint32_t cpu_ms = cycles / (EOS_ESP32_CPU_MHZ * 1000U);
    put(" rc ");
    put(rc == 0 ? "0" : "-1");
    put(", RTC timer ");
    put_u(ms);
    put(" ms (slow clock ");
    put_u(hz);
    put(" Hz), CPU ran ");
    put_u(cpu_ms);
    put(" ms of it");
    check(rc == 0 && ms >= 180U && ms <= 220U && cpu_ms < 20U);
}

static void deep_sleep_phase(void)
{
    s_tag = RTC_TAG;
    s_saved_failures = (uint32_t)s_failures;
    s_sleep_start = eos_esp32_rtc_time();
    put("rtc_test: deep sleep 1 s\n");
    eos_esp32_deep_sleep_us(1000000U);
    put("CHECK deep sleep entered");
    check(false);
}

static void after_deep_sleep(void)
{
    uint32_t hz = eos_esp32_rtc_slow_hz();
    uint64_t now = eos_esp32_rtc_time();
    uint32_t ms = hz ? (uint32_t)((now - s_sleep_start) * 1000U / hz) : 0U;
    uint32_t reason = eos_esp32_reset_reason(), cause = eos_esp32_wakeup_cause();
    s_failures = (int)s_saved_failures;
    put("CHECK deep sleep 1 s: reset reason ");
    put_u(reason);
    put(", wake cause ");
    put_u(cause);
    put(", RTC memory kept, ");
    put_u(ms);
    put(" ms from sleep to this boot");
    /* The boot itself (ROM, image load) adds a few tens of ms. */
    check(reason == EOS_ESP32_RESET_DEEPSLEEP && (cause & 8U) != 0U && ms >= 950U && ms <= 1300U);
}

int main(void)
{
    if (s_tag == RTC_TAG && eos_esp32_reset_reason() == EOS_ESP32_RESET_DEEPSLEEP) {
        s_tag = 0;
        put("rtc_test: woke from deep sleep\n");
        after_deep_sleep();
    } else {
        s_tag = 0;
        put("rtc_test: starting\n");
        test_touch_pads();
        test_touch_load();
        test_touch_finger();
        test_light_sleep();
        deep_sleep_phase();
    }
    put(s_failures == 0 ? "RTC TEST PASSED\n" : "RTC TEST FAILED\n");
    for (;;) {
        __asm volatile ("waiti 0");
    }
}
