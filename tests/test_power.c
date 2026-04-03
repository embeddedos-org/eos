// SPDX-License-Identifier: MIT
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define EOS_ENABLE_POWER 1
#include "eos/eos_config.h"
#include "eos/power.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

static void test_power_init(void) {
    eos_power_config_t cfg = { .adc_channel = 0, .vbat_full_mv = 4200, .vbat_empty_mv = 3000, .divider_ratio_x10 = 20 };
    assert(eos_power_init(&cfg) == 0);
    eos_power_deinit();
    PASS("power init/deinit");
}
static void test_power_init_null(void) {
    assert(eos_power_init(NULL) != 0);
    PASS("power init null");
}
static void test_power_mode_default(void) {
    eos_power_config_t cfg = { .adc_channel = 0, .vbat_full_mv = 4200, .vbat_empty_mv = 3000, .divider_ratio_x10 = 20 };
    eos_power_init(&cfg);
    assert(eos_power_get_mode() == EOS_POWER_RUN);
    eos_power_deinit();
    PASS("power default mode RUN");
}
static void test_power_enter_sleep(void) {
    eos_power_config_t cfg = { .adc_channel = 0, .vbat_full_mv = 4200, .vbat_empty_mv = 3000, .divider_ratio_x10 = 20 };
    eos_power_init(&cfg);
    assert(eos_power_enter_mode(EOS_POWER_SLEEP) == 0);
    assert(eos_power_get_mode() == EOS_POWER_SLEEP);
    eos_power_deinit();
    PASS("power enter sleep");
}
static void test_power_enter_deep_sleep(void) {
    eos_power_config_t cfg = { .adc_channel = 0, .vbat_full_mv = 4200, .vbat_empty_mv = 3000, .divider_ratio_x10 = 20 };
    eos_power_init(&cfg);
    assert(eos_power_enter_mode(EOS_POWER_DEEP_SLEEP) == 0);
    assert(eos_power_get_mode() == EOS_POWER_DEEP_SLEEP);
    eos_power_deinit();
    PASS("power enter deep sleep");
}
static void test_power_wake_sources(void) {
    eos_power_config_t cfg = { .adc_channel = 0, .vbat_full_mv = 4200, .vbat_empty_mv = 3000, .divider_ratio_x10 = 20 };
    eos_power_init(&cfg);
    uint32_t mask = EOS_WAKE_GPIO | EOS_WAKE_TIMER | EOS_WAKE_RTC;
    assert(eos_power_set_wake_sources(mask) == 0);
    eos_power_deinit();
    PASS("power set wake sources");
}
static void test_power_battery_mv(void) {
    eos_power_config_t cfg = { .adc_channel = 0, .vbat_full_mv = 4200, .vbat_empty_mv = 3000, .divider_ratio_x10 = 20 };
    eos_power_init(&cfg);
    uint32_t mv = eos_power_get_battery_mv();
    (void)mv;
    PASS("power battery mv");
}
static void test_power_battery_pct(void) {
    eos_power_config_t cfg = { .adc_channel = 0, .vbat_full_mv = 4200, .vbat_empty_mv = 3000, .divider_ratio_x10 = 20 };
    eos_power_init(&cfg);
    uint8_t pct = eos_power_get_battery_pct();
    assert(pct <= 100);
    eos_power_deinit();
    PASS("power battery pct <= 100");
}
static void test_power_battery_info(void) {
    eos_power_config_t cfg = { .adc_channel = 0, .vbat_full_mv = 4200, .vbat_empty_mv = 3000, .divider_ratio_x10 = 20 };
    eos_power_init(&cfg);
    eos_battery_info_t info;
    int r = eos_power_get_battery_info(&info);
    assert(r == 0);
    assert(info.percentage <= 100);
    eos_power_deinit();
    PASS("power battery info");
}
static void test_power_battery_info_null(void) {
    eos_power_config_t cfg = { .adc_channel = 0, .vbat_full_mv = 4200, .vbat_empty_mv = 3000, .divider_ratio_x10 = 20 };
    eos_power_init(&cfg);
    assert(eos_power_get_battery_info(NULL) != 0);
    eos_power_deinit();
    PASS("power battery info null");
}
static void test_power_periph_enable(void) {
    eos_power_config_t cfg = { .adc_channel = 0, .vbat_full_mv = 4200, .vbat_empty_mv = 3000, .divider_ratio_x10 = 20 };
    eos_power_init(&cfg);
    assert(eos_power_enable_peripheral(EOS_PERIPH_UART) == 0);
    assert(eos_power_is_peripheral_enabled(EOS_PERIPH_UART) == true);
    eos_power_deinit();
    PASS("power periph enable");
}
static void test_power_periph_disable(void) {
    eos_power_config_t cfg = { .adc_channel = 0, .vbat_full_mv = 4200, .vbat_empty_mv = 3000, .divider_ratio_x10 = 20 };
    eos_power_init(&cfg);
    eos_power_enable_peripheral(EOS_PERIPH_SPI);
    assert(eos_power_disable_peripheral(EOS_PERIPH_SPI) == 0);
    assert(eos_power_is_peripheral_enabled(EOS_PERIPH_SPI) == false);
    eos_power_deinit();
    PASS("power periph disable");
}
static void test_power_periph_invalid(void) {
    eos_power_config_t cfg = { .adc_channel = 0, .vbat_full_mv = 4200, .vbat_empty_mv = 3000, .divider_ratio_x10 = 20 };
    eos_power_init(&cfg);
    assert(eos_power_enable_peripheral(EOS_PERIPH_MAX) != 0);
    eos_power_deinit();
    PASS("power periph invalid id");
}
static void test_power_cpu_freq(void) {
    eos_power_config_t cfg = { .adc_channel = 0, .vbat_full_mv = 4200, .vbat_empty_mv = 3000, .divider_ratio_x10 = 20 };
    eos_power_init(&cfg);
    assert(eos_power_set_cpu_freq(168000000) == 0);
    assert(eos_power_get_cpu_freq() == 168000000);
    eos_power_deinit();
    PASS("power cpu freq");
}
int main(void) {
    printf("=== EoS Power Management Tests ===\n");
    test_power_init();
    test_power_init_null();
    test_power_mode_default();
    test_power_enter_sleep();
    test_power_enter_deep_sleep();
    test_power_wake_sources();
    test_power_battery_mv();
    test_power_battery_pct();
    test_power_battery_info();
    test_power_battery_info_null();
    test_power_periph_enable();
    test_power_periph_disable();
    test_power_periph_invalid();
    test_power_cpu_freq();
    printf("\n=== ALL %d POWER TESTS PASSED ===\n", passed);
    return 0;
}
