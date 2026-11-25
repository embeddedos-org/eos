// SPDX-License-Identifier: MIT
/*
 * Power management framework tests.
 *
 * Two halves, because the framework has two honest answers (#288):
 *
 *   - With no board backend registered, every call that needs hardware
 *     returns EOS_POWER_ENOTSUP and changes nothing. The earlier version of
 *     power.c returned 0 from eos_power_enter_mode(EOS_POWER_DEEP_SLEEP) with
 *     the core still running, and this suite asserted that 0 -- so the lie
 *     was green. The no-backend cases below are what makes it red.
 *
 *   - With a backend registered, each call reaches exactly the backend
 *     operation it names, with the arguments it was given, and the framework
 *     keeps only state the backend confirmed.
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define EOS_ENABLE_POWER 1
#include "eos/eos_config.h"
#include "eos/power.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

static const eos_power_config_t default_cfg = {
    .adc_channel = 3, .vbat_full_mv = 4200, .vbat_empty_mv = 3000,
    .divider_ratio_x10 = 20
};

/* ============================================================
 * Fake board backend — records what the framework asked it to do
 * ============================================================ */

static struct {
    int              enter_calls;
    eos_power_mode_t last_mode;
    int              enter_rc;
    uint32_t         report_wake_source;

    int      wake_calls;
    uint32_t last_wake_mask;
    int      wake_rc;

    int      battery_calls;
    uint8_t  last_adc_channel;
    uint32_t report_pin_mv;
    int      battery_rc;

    int                 periph_calls;
    eos_peripheral_id_t last_periph;
    bool                last_periph_enable;
    int                 periph_rc;

    int      freq_calls;
    uint32_t last_freq_hz;
    uint32_t report_actual_hz;
    int      freq_rc;
} fake;

static void fake_reset(void)
{
    memset(&fake, 0, sizeof(fake));
    fake.report_actual_hz = 1;  /* distinguishable from "untouched" */
}

static int fake_enter_mode(eos_power_mode_t mode, uint32_t *wake_source_out)
{
    fake.enter_calls++;
    fake.last_mode = mode;
    /* A real backend is inside the sleep right here; the framework must have
     * published the mode before calling, for an interrupt to observe. */
    assert(eos_power_get_mode() == mode);
    if (wake_source_out) *wake_source_out = fake.report_wake_source;
    return fake.enter_rc;
}

static int fake_set_wake_sources(uint32_t wake_mask)
{
    fake.wake_calls++;
    fake.last_wake_mask = wake_mask;
    return fake.wake_rc;
}

static int fake_read_battery_mv(uint8_t adc_channel, uint32_t *mv_out)
{
    fake.battery_calls++;
    fake.last_adc_channel = adc_channel;
    if (mv_out) *mv_out = fake.report_pin_mv;
    return fake.battery_rc;
}

static int fake_set_peripheral(eos_peripheral_id_t periph, bool enable)
{
    fake.periph_calls++;
    fake.last_periph = periph;
    fake.last_periph_enable = enable;
    return fake.periph_rc;
}

static int fake_set_cpu_freq(uint32_t freq_hz, uint32_t *actual_hz_out)
{
    fake.freq_calls++;
    fake.last_freq_hz = freq_hz;
    if (actual_hz_out) *actual_hz_out = fake.report_actual_hz;
    return fake.freq_rc;
}

static const eos_power_backend_t fake_backend = {
    .name              = "fake",
    .enter_mode        = fake_enter_mode,
    .set_wake_sources  = fake_set_wake_sources,
    .read_battery_mv   = fake_read_battery_mv,
    .read_battery_info = NULL,   /* no fuel gauge on this fake board */
    .set_peripheral    = fake_set_peripheral,
    .set_cpu_freq      = fake_set_cpu_freq,
};

static void with_backend(void)
{
    fake_reset();
    eos_power_register_backend(&fake_backend);
    assert(eos_power_get_backend() == &fake_backend);
    assert(eos_power_init(&default_cfg) == EOS_POWER_OK);
}

static void no_backend(void)
{
    fake_reset();
    eos_power_register_backend(NULL);
    assert(eos_power_get_backend() == NULL);
    assert(eos_power_init(&default_cfg) == EOS_POWER_OK);
}

static void teardown(void)
{
    eos_power_deinit();
    eos_power_register_backend(NULL);
}

/* ============================================================
 * Lifecycle
 * ============================================================ */

static void test_power_init(void) {
    assert(eos_power_init(&default_cfg) == EOS_POWER_OK);
    eos_power_deinit();
    PASS("power init/deinit");
}
static void test_power_init_null(void) {
    assert(eos_power_init(NULL) == EOS_POWER_EINVAL);
    PASS("power init null");
}
static void test_power_mode_default(void) {
    no_backend();
    assert(eos_power_get_mode() == EOS_POWER_RUN);
    teardown();
    PASS("power default mode RUN");
}

/* ============================================================
 * No backend: ENOTSUP, and no state moves (#288)
 * ============================================================ */

static void test_sleep_without_backend_is_not_supported(void) {
    no_backend();
    assert(eos_power_enter_mode(EOS_POWER_SLEEP) == EOS_POWER_ENOTSUP);
    assert(eos_power_get_mode() == EOS_POWER_RUN);
    teardown();
    PASS("enter sleep without backend -> ENOTSUP, still RUN");
}
static void test_deep_sleep_without_backend_is_not_supported(void) {
    no_backend();
    assert(eos_power_enter_mode(EOS_POWER_DEEP_SLEEP) == EOS_POWER_ENOTSUP);
    assert(eos_power_get_mode() == EOS_POWER_RUN);
    teardown();
    PASS("enter deep sleep without backend -> ENOTSUP, still RUN");
}
static void test_shutdown_without_backend_is_not_supported(void) {
    no_backend();
    assert(eos_power_enter_mode(EOS_POWER_SHUTDOWN) == EOS_POWER_ENOTSUP);
    assert(eos_power_get_mode() == EOS_POWER_RUN);
    teardown();
    PASS("enter shutdown without backend -> ENOTSUP, still RUN");
}
static void test_run_mode_needs_no_backend(void) {
    no_backend();
    assert(eos_power_enter_mode(EOS_POWER_RUN) == EOS_POWER_OK);
    assert(eos_power_get_mode() == EOS_POWER_RUN);
    teardown();
    PASS("enter RUN without backend -> OK");
}
static void test_wake_sources_without_backend(void) {
    no_backend();
    assert(eos_power_set_wake_sources(EOS_WAKE_RTC) == EOS_POWER_ENOTSUP);
    assert(eos_power_get_wake_source() == 0);
    teardown();
    PASS("set wake sources without backend -> ENOTSUP");
}
static void test_battery_without_backend_is_unknown(void) {
    no_backend();
    /* Was vbat_full_mv / 100% on every board with no battery sense. */
    assert(eos_power_get_battery_mv() == 0);
    assert(eos_power_get_battery_pct() == 0);
    eos_battery_info_t info;
    assert(eos_power_get_battery_info(&info) == EOS_POWER_ENOTSUP);
    teardown();
    PASS("battery without backend -> unknown, ENOTSUP");
}
static void test_periph_without_backend(void) {
    no_backend();
    assert(eos_power_disable_peripheral(EOS_PERIPH_UART) == EOS_POWER_ENOTSUP);
    /* Nothing was gated, so the shadow must not claim it was. */
    assert(eos_power_is_peripheral_enabled(EOS_PERIPH_UART) == true);
    assert(eos_power_enable_peripheral(EOS_PERIPH_UART) == EOS_POWER_ENOTSUP);
    teardown();
    PASS("peripheral gating without backend -> ENOTSUP");
}
static void test_cpu_freq_without_backend(void) {
    no_backend();
    assert(eos_power_set_cpu_freq(168000000) == EOS_POWER_ENOTSUP);
    assert(eos_power_get_cpu_freq() == 0);
    teardown();
    PASS("set cpu freq without backend -> ENOTSUP");
}

/* ============================================================
 * Argument validation
 * ============================================================ */

static void test_power_enter_invalid_mode(void) {
    with_backend();
    assert(eos_power_enter_mode((eos_power_mode_t)(EOS_POWER_SHUTDOWN + 1))
           == EOS_POWER_EINVAL);
    assert(fake.enter_calls == 0);
    assert(eos_power_get_mode() == EOS_POWER_RUN);
    teardown();
    PASS("power enter invalid mode");
}
static void test_power_calls_before_init(void) {
    eos_power_register_backend(&fake_backend);
    fake_reset();
    eos_power_deinit();
    assert(eos_power_enter_mode(EOS_POWER_SLEEP) == EOS_POWER_EINVAL);
    assert(eos_power_set_wake_sources(EOS_WAKE_RTC) == EOS_POWER_EINVAL);
    assert(eos_power_set_cpu_freq(1000000) == EOS_POWER_EINVAL);
    assert(eos_power_get_battery_mv() == 0);
    assert(fake.enter_calls == 0 && fake.wake_calls == 0 && fake.freq_calls == 0);
    teardown();
    PASS("power calls before init -> EINVAL, backend untouched");
}
static void test_power_periph_invalid(void) {
    with_backend();
    assert(eos_power_enable_peripheral(EOS_PERIPH_MAX) == EOS_POWER_EINVAL);
    assert(eos_power_disable_peripheral(EOS_PERIPH_MAX) == EOS_POWER_EINVAL);
    assert(eos_power_is_peripheral_enabled(EOS_PERIPH_MAX) == false);

    eos_peripheral_id_t negative_id = (eos_peripheral_id_t)-1;
    assert(eos_power_enable_peripheral(negative_id) == EOS_POWER_EINVAL);
    assert(eos_power_disable_peripheral(negative_id) == EOS_POWER_EINVAL);
    assert(eos_power_is_peripheral_enabled(negative_id) == false);
    assert(fake.periph_calls == 0);
    teardown();
    PASS("power periph invalid id");
}
static void test_power_battery_info_null(void) {
    with_backend();
    assert(eos_power_get_battery_info(NULL) == EOS_POWER_EINVAL);
    teardown();
    PASS("power battery info null");
}

/* ============================================================
 * With a backend: the work is delegated and the result is kept
 * ============================================================ */

static void test_enter_sleep_delegates(void) {
    with_backend();
    fake.report_wake_source = EOS_WAKE_RTC;
    assert(eos_power_enter_mode(EOS_POWER_DEEP_SLEEP) == EOS_POWER_OK);
    assert(fake.enter_calls == 1);
    assert(fake.last_mode == EOS_POWER_DEEP_SLEEP);
    /* The call returned, so the core is running again. */
    assert(eos_power_get_mode() == EOS_POWER_RUN);
    assert(eos_power_get_wake_source() == EOS_WAKE_RTC);
    teardown();
    PASS("enter deep sleep delegates and reports wake source");
}
static void test_enter_sleep_backend_failure_propagates(void) {
    with_backend();
    fake.enter_rc = -5;
    fake.report_wake_source = EOS_WAKE_GPIO;
    assert(eos_power_enter_mode(EOS_POWER_SLEEP) == -5);
    assert(eos_power_get_mode() == EOS_POWER_RUN);
    /* No sleep happened, so nothing woke the part. */
    assert(eos_power_get_wake_source() == 0);
    teardown();
    PASS("backend sleep failure propagates, no wake source invented");
}
static void test_wake_sources_delegate(void) {
    with_backend();
    uint32_t mask = EOS_WAKE_GPIO | EOS_WAKE_TIMER | EOS_WAKE_RTC;
    assert(eos_power_set_wake_sources(mask) == EOS_POWER_OK);
    assert(fake.wake_calls == 1);
    assert(fake.last_wake_mask == mask);
    teardown();
    PASS("set wake sources delegates the exact mask");
}
static void test_wake_sources_failure_propagates(void) {
    with_backend();
    fake.wake_rc = -7;
    assert(eos_power_set_wake_sources(EOS_WAKE_BLE) == -7);
    teardown();
    PASS("set wake sources failure propagates");
}
static void test_battery_mv_applies_divider(void) {
    with_backend();
    fake.report_pin_mv = 2100;   /* half of 4200 through a 2.0:1 divider */
    assert(eos_power_get_battery_mv() == 4200);
    assert(fake.battery_calls == 1);
    assert(fake.last_adc_channel == default_cfg.adc_channel);
    teardown();
    PASS("battery mv reads the configured channel and applies the divider");
}
static void test_battery_pct_tracks_voltage(void) {
    with_backend();
    fake.report_pin_mv = 1800;              /* 3600 mV battery */
    assert(eos_power_get_battery_mv() == 3600);
    uint8_t pct = eos_power_get_battery_pct();
    assert(pct == 50);                       /* (3600-3000)/1200 */
    assert(pct <= 100);
    teardown();
    PASS("battery pct tracks a mid-scale reading");
}
static void test_battery_read_failure_is_unknown(void) {
    with_backend();
    fake.report_pin_mv = 2100;
    fake.battery_rc = -1;
    assert(eos_power_get_battery_mv() == 0);
    assert(eos_power_get_battery_pct() == 0);
    teardown();
    PASS("failed battery read reports unknown, not full");
}
static void test_battery_info_derived_from_voltage(void) {
    with_backend();
    fake.report_pin_mv = 2100;
    eos_battery_info_t info;
    assert(eos_power_get_battery_info(&info) == EOS_POWER_OK);
    assert(info.voltage_mv == 4200);
    assert(info.percentage == 100);
    /* No gauge on this backend: these stay zero instead of being made up. */
    assert(info.charging == false);
    assert(info.charger_connected == false);
    assert(info.current_ma == 0);
    assert(info.temperature_c == 0);
    teardown();
    PASS("battery info derived from voltage leaves gauge fields zero");
}
static void test_periph_enable_disable_delegate(void) {
    with_backend();
    assert(eos_power_disable_peripheral(EOS_PERIPH_SPI) == EOS_POWER_OK);
    assert(fake.periph_calls == 1);
    assert(fake.last_periph == EOS_PERIPH_SPI);
    assert(fake.last_periph_enable == false);
    assert(eos_power_is_peripheral_enabled(EOS_PERIPH_SPI) == false);

    assert(eos_power_enable_peripheral(EOS_PERIPH_SPI) == EOS_POWER_OK);
    assert(fake.last_periph_enable == true);
    assert(eos_power_is_peripheral_enabled(EOS_PERIPH_SPI) == true);
    teardown();
    PASS("peripheral gating delegates and tracks applied state");
}
static void test_periph_failure_does_not_move_shadow(void) {
    with_backend();
    fake.periph_rc = -3;
    assert(eos_power_disable_peripheral(EOS_PERIPH_WIFI) == -3);
    assert(eos_power_is_peripheral_enabled(EOS_PERIPH_WIFI) == true);
    teardown();
    PASS("failed peripheral gating leaves the recorded state alone");
}
static void test_cpu_freq_records_what_was_reached(void) {
    with_backend();
    fake.report_actual_hz = 160000000;   /* PLL could not hit 168 MHz */
    assert(eos_power_set_cpu_freq(168000000) == EOS_POWER_OK);
    assert(fake.freq_calls == 1);
    assert(fake.last_freq_hz == 168000000);
    assert(eos_power_get_cpu_freq() == 160000000);
    teardown();
    PASS("cpu freq records the frequency the backend reached");
}
static void test_cpu_freq_failure_propagates(void) {
    with_backend();
    fake.report_actual_hz = 160000000;
    fake.freq_rc = -9;
    assert(eos_power_set_cpu_freq(168000000) == -9);
    assert(eos_power_get_cpu_freq() == 0);
    teardown();
    PASS("failed cpu freq change propagates and records nothing");
}

int main(void) {
    printf("=== EoS Power Management Tests ===\n");
    test_power_init();
    test_power_init_null();
    test_power_mode_default();

    test_sleep_without_backend_is_not_supported();
    test_deep_sleep_without_backend_is_not_supported();
    test_shutdown_without_backend_is_not_supported();
    test_run_mode_needs_no_backend();
    test_wake_sources_without_backend();
    test_battery_without_backend_is_unknown();
    test_periph_without_backend();
    test_cpu_freq_without_backend();

    test_power_enter_invalid_mode();
    test_power_calls_before_init();
    test_power_periph_invalid();
    test_power_battery_info_null();

    test_enter_sleep_delegates();
    test_enter_sleep_backend_failure_propagates();
    test_wake_sources_delegate();
    test_wake_sources_failure_propagates();
    test_battery_mv_applies_divider();
    test_battery_pct_tracks_voltage();
    test_battery_read_failure_is_unknown();
    test_battery_info_derived_from_voltage();
    test_periph_enable_disable_delegate();
    test_periph_failure_does_not_move_shadow();
    test_cpu_freq_records_what_was_reached();
    test_cpu_freq_failure_propagates();

    printf("\n=== ALL %d POWER TESTS PASSED ===\n", passed);
    return 0;
}
