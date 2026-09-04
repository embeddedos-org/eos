// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_pwm.c
 * @brief Unit tests for PWM (Pulse Width Modulation) peripheral
 */

#include <eos/hal_extended.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int tests_run = 0;
static int tests_passed = 0;

/* Every channel is deinitialized before each test so a test's outcome never
 * depends on what a previous test left initialized. */
static void setup(void)
{
    for (uint8_t i = 0; i < 16; i++) {
        eos_pwm_deinit(i);
    }
}

#define TEST(name) \
    static void name(void); \
    static void run_##name(void) { \
        printf("  %-50s ", #name); \
        name(); \
        tests_passed++; \
        printf("[PASS]\n"); \
    } \
    static void name(void)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while(0)

/* ============================================================
 * Basic PWM API Tests
 * ============================================================ */

TEST(test_pwm_init_invalid_channel)
{
    setup();
    /* Test invalid channel numbers */
    eos_pwm_config_t cfg = {
        .channel = 16,  /* Invalid - max is 15 */
        .frequency_hz = 1000,
        .duty_pct_x10 = 500  /* 50.0% */
    };
    ASSERT(eos_pwm_init(&cfg) == -1);

    cfg.channel = 255;  /* Way out of range */
    ASSERT(eos_pwm_init(&cfg) == -1);
}

TEST(test_pwm_init_null_config)
{
    setup();
    /* Test NULL configuration */
    ASSERT(eos_pwm_init(NULL) == -1);
}

TEST(test_pwm_init_valid)
{
    setup();
    /* Test valid initialization for channel 0 */
    eos_pwm_config_t cfg = {
        .channel = 0,
        .frequency_hz = 1000,
        .duty_pct_x10 = 500  /* 50.0% */
    };
    ASSERT(eos_pwm_init(&cfg) == 0);
    eos_pwm_deinit(0);

    /* Test valid initialization for channel 15 */
    cfg.channel = 15;
    ASSERT(eos_pwm_init(&cfg) == 0);
    eos_pwm_deinit(15);
}

TEST(test_pwm_init_multiple_channels)
{
    setup();
    /* Test initializing multiple channels */
    for (uint8_t i = 0; i < 4; i++) {
        eos_pwm_config_t cfg = {
            .channel = i,
            .frequency_hz = 1000 + (i * 100),
            .duty_pct_x10 = 250 + (i * 100)
        };
        ASSERT(eos_pwm_init(&cfg) == 0);
    }

    /* Clean up */
    for (uint8_t i = 0; i < 4; i++) {
        eos_pwm_deinit(i);
    }
}

TEST(test_pwm_init_all_channels)
{
    setup();
    /* Every channel 0-15 must be independently initializable and usable.
     * Regression coverage for the STM32F4 array being sized to 4 instead
     * of 16 (eos#136 review finding 1) -- this only protects the backend
     * actually compiled and linked into this test binary; see PR body for
     * the STM32F4 coverage gap. */
    for (uint8_t ch = 0; ch < 16; ch++) {
        eos_pwm_config_t cfg = {
            .channel = ch,
            .frequency_hz = 1000,
            .duty_pct_x10 = 500
        };
        ASSERT(eos_pwm_init(&cfg) == 0);
    }

    /* Read back through the API: every channel must independently accept
     * duty/freq updates and start/stop without disturbing its neighbours. */
    for (uint8_t ch = 0; ch < 16; ch++) {
        ASSERT(eos_pwm_set_duty(ch, 250) == 0);
        ASSERT(eos_pwm_set_freq(ch, 2000) == 0);
        ASSERT(eos_pwm_start(ch) == 0);
        ASSERT(eos_pwm_stop(ch) == 0);
    }

    for (uint8_t ch = 0; ch < 16; ch++) {
        eos_pwm_deinit(ch);
    }
}

TEST(test_pwm_deinit_invalid_channel)
{
    setup();
    /* Test deinit on invalid channel - should not crash */
    eos_pwm_deinit(16);
    eos_pwm_deinit(255);
    /* Just ensure it doesn't crash */
}

TEST(test_pwm_set_duty_invalid_channel)
{
    setup();
    /* Test setting duty on invalid channel */
    ASSERT(eos_pwm_set_duty(16, 500) == -1);
    ASSERT(eos_pwm_set_duty(255, 500) == -1);
}

TEST(test_pwm_set_duty_invalid_value)
{
    setup();
    /* Test setting invalid duty cycle values */
    eos_pwm_config_t cfg = {
        .channel = 0,
        .frequency_hz = 1000,
        .duty_pct_x10 = 500
    };
    ASSERT(eos_pwm_init(&cfg) == 0);

    /* Duty cycle > 100% */
    ASSERT(eos_pwm_set_duty(0, 1001) == -1);
    ASSERT(eos_pwm_set_duty(0, 5000) == -1);

    eos_pwm_deinit(0);
}

TEST(test_pwm_set_duty_valid)
{
    setup();
    /* Test setting valid duty cycles */
    eos_pwm_config_t cfg = {
        .channel = 0,
        .frequency_hz = 1000,
        .duty_pct_x10 = 500
    };
    ASSERT(eos_pwm_init(&cfg) == 0);

    /* Test various duty cycles */
    ASSERT(eos_pwm_set_duty(0, 0) == 0);      /* 0% */
    ASSERT(eos_pwm_set_duty(0, 250) == 0);    /* 25% */
    ASSERT(eos_pwm_set_duty(0, 500) == 0);    /* 50% */
    ASSERT(eos_pwm_set_duty(0, 750) == 0);    /* 75% */
    ASSERT(eos_pwm_set_duty(0, 1000) == 0);   /* 100% */

    eos_pwm_deinit(0);
}

TEST(test_pwm_set_duty_not_initialized)
{
    setup();
    /* Test setting duty on uninitialized channel */
    ASSERT(eos_pwm_set_duty(0, 500) == -1);
}

TEST(test_pwm_set_freq_invalid_channel)
{
    setup();
    /* Test setting frequency on invalid channel */
    ASSERT(eos_pwm_set_freq(16, 1000) == -1);
    ASSERT(eos_pwm_set_freq(255, 1000) == -1);
}

TEST(test_pwm_set_freq_invalid_value)
{
    setup();
    /* Test setting invalid frequency values */
    eos_pwm_config_t cfg = {
        .channel = 0,
        .frequency_hz = 1000,
        .duty_pct_x10 = 500
    };
    ASSERT(eos_pwm_init(&cfg) == 0);

    /* Zero frequency */
    ASSERT(eos_pwm_set_freq(0, 0) == -1);

    eos_pwm_deinit(0);
}

TEST(test_pwm_set_freq_valid)
{
    setup();
    /* Test setting valid frequencies */
    eos_pwm_config_t cfg = {
        .channel = 0,
        .frequency_hz = 1000,
        .duty_pct_x10 = 500
    };
    ASSERT(eos_pwm_init(&cfg) == 0);

    /* Test various frequencies */
    ASSERT(eos_pwm_set_freq(0, 100) == 0);    /* 100 Hz */
    ASSERT(eos_pwm_set_freq(0, 1000) == 0);   /* 1 kHz */
    ASSERT(eos_pwm_set_freq(0, 10000) == 0);  /* 10 kHz */
    ASSERT(eos_pwm_set_freq(0, 50000) == 0);  /* 50 kHz */

    eos_pwm_deinit(0);
}

TEST(test_pwm_set_freq_not_initialized)
{
    setup();
    /* Test setting frequency on uninitialized channel */
    ASSERT(eos_pwm_set_freq(0, 1000) == -1);
}

TEST(test_pwm_start_invalid_channel)
{
    setup();
    /* Test starting invalid channel */
    ASSERT(eos_pwm_start(16) == -1);
    ASSERT(eos_pwm_start(255) == -1);
}

TEST(test_pwm_start_stop_cycle)
{
    setup();
    /* Test start/stop cycle */
    eos_pwm_config_t cfg = {
        .channel = 0,
        .frequency_hz = 1000,
        .duty_pct_x10 = 500
    };
    ASSERT(eos_pwm_init(&cfg) == 0);

    /* Start PWM */
    ASSERT(eos_pwm_start(0) == 0);

    /* Stop PWM */
    ASSERT(eos_pwm_stop(0) == 0);

    /* Start again */
    ASSERT(eos_pwm_start(0) == 0);

    /* Stop again */
    ASSERT(eos_pwm_stop(0) == 0);

    eos_pwm_deinit(0);
}

TEST(test_pwm_start_not_initialized)
{
    setup();
    /* Test starting uninitialized channel */
    ASSERT(eos_pwm_start(0) == -1);
}

TEST(test_pwm_stop_invalid_channel)
{
    setup();
    /* Test stopping invalid channel */
    ASSERT(eos_pwm_stop(16) == -1);
    ASSERT(eos_pwm_stop(255) == -1);
}

TEST(test_pwm_stop_not_initialized)
{
    setup();
    /* Test stopping uninitialized channel */
    ASSERT(eos_pwm_stop(0) == -1);
}

TEST(test_pwm_stop_when_not_running)
{
    setup();
    /* eos_pwm_stop() is idempotent: stopping a channel that is initialized
     * but not running succeeds, matching hal_extended.h's documented
     * contract for eos_pwm_stop(). */
    eos_pwm_config_t cfg = {
        .channel = 0,
        .frequency_hz = 1000,
        .duty_pct_x10 = 500
    };
    ASSERT(eos_pwm_init(&cfg) == 0);
    ASSERT(eos_pwm_stop(0) == 0);

    eos_pwm_deinit(0);
}

/* ============================================================
 * PWM Configuration Tests
 * ============================================================ */

TEST(test_pwm_different_frequencies)
{
    setup();
    /* Test multiple channels with different frequencies */
    eos_pwm_config_t cfg[] = {
        { .channel = 0, .frequency_hz = 100, .duty_pct_x10 = 500 },
        { .channel = 1, .frequency_hz = 1000, .duty_pct_x10 = 500 },
        { .channel = 2, .frequency_hz = 10000, .duty_pct_x10 = 500 },
    };

    for (int i = 0; i < 3; i++) {
        ASSERT(eos_pwm_init(&cfg[i]) == 0);
    }

    for (int i = 0; i < 3; i++) {
        eos_pwm_deinit(i);
    }
}

TEST(test_pwm_different_duty_cycles)
{
    setup();
    /* Test multiple channels with different duty cycles */
    eos_pwm_config_t cfg[] = {
        { .channel = 0, .frequency_hz = 1000, .duty_pct_x10 = 0 },    /* 0% */
        { .channel = 1, .frequency_hz = 1000, .duty_pct_x10 = 250 },  /* 25% */
        { .channel = 2, .frequency_hz = 1000, .duty_pct_x10 = 500 },  /* 50% */
        { .channel = 3, .frequency_hz = 1000, .duty_pct_x10 = 750 },  /* 75% */
        { .channel = 4, .frequency_hz = 1000, .duty_pct_x10 = 1000 }, /* 100% */
    };

    for (int i = 0; i < 5; i++) {
        ASSERT(eos_pwm_init(&cfg[i]) == 0);
    }

    for (int i = 0; i < 5; i++) {
        eos_pwm_deinit(i);
    }
}

TEST(test_pwm_reinitialize_channel)
{
    setup();
    /* Test reinitializing a channel */
    eos_pwm_config_t cfg1 = {
        .channel = 0,
        .frequency_hz = 1000,
        .duty_pct_x10 = 500
    };
    ASSERT(eos_pwm_init(&cfg1) == 0);
    eos_pwm_deinit(0);

    /* Reinitialize with different parameters */
    eos_pwm_config_t cfg2 = {
        .channel = 0,
        .frequency_hz = 2000,
        .duty_pct_x10 = 750
    };
    ASSERT(eos_pwm_init(&cfg2) == 0);
    eos_pwm_deinit(0);
}

/* ============================================================
 * PWM Edge Case Tests
 * ============================================================ */

TEST(test_pwm_high_frequency)
{
    setup();
    /* Test high frequency PWM */
    eos_pwm_config_t cfg = {
        .channel = 0,
        .frequency_hz = 100000,  /* 100 kHz */
        .duty_pct_x10 = 500
    };
    ASSERT(eos_pwm_init(&cfg) == 0);
    eos_pwm_deinit(0);
}

TEST(test_pwm_low_frequency)
{
    setup();
    /* Test low frequency PWM */
    eos_pwm_config_t cfg = {
        .channel = 0,
        .frequency_hz = 1,  /* 1 Hz */
        .duty_pct_x10 = 500
    };
    ASSERT(eos_pwm_init(&cfg) == 0);
    eos_pwm_deinit(0);
}

TEST(test_pwm_duty_cycle_boundaries)
{
    setup();
    /* Test duty cycle at boundaries */
    eos_pwm_config_t cfg = {
        .channel = 0,
        .frequency_hz = 1000,
        .duty_pct_x10 = 500
    };
    ASSERT(eos_pwm_init(&cfg) == 0);

    /* Test boundary values */
    ASSERT(eos_pwm_set_duty(0, 1) == 0);      /* 0.1% */
    ASSERT(eos_pwm_set_duty(0, 999) == 0);    /* 99.9% */
    ASSERT(eos_pwm_set_duty(0, 1000) == 0);   /* 100% */

    eos_pwm_deinit(0);
}

TEST(test_pwm_init_rejects_zero_frequency)
{
    setup();
    /* eos_pwm_set_freq() has always rejected 0; eos_pwm_init() must reject
     * it too instead of accepting a config that later divides by it. */
    eos_pwm_config_t cfg = {
        .channel = 0,
        .frequency_hz = 0,
        .duty_pct_x10 = 500
    };
    ASSERT(eos_pwm_init(&cfg) == -1);
}

TEST(test_pwm_init_rejects_duty_over_limit)
{
    setup();
    /* eos_pwm_set_duty() has always rejected > 1000; eos_pwm_init() must
     * reject it too instead of programming an out-of-range duty cycle. */
    eos_pwm_config_t cfg = {
        .channel = 0,
        .frequency_hz = 1000,
        .duty_pct_x10 = 1001
    };
    ASSERT(eos_pwm_init(&cfg) == -1);

    cfg.duty_pct_x10 = 65535;
    ASSERT(eos_pwm_init(&cfg) == -1);
}

/* ============================================================
 * Main Test Runner
 * ============================================================ */

int main(void)
{
    printf("=== EoS: PWM Unit Tests ===\n\n");

    /* Basic API tests */
    run_test_pwm_init_invalid_channel();
    run_test_pwm_init_null_config();
    run_test_pwm_init_valid();
    run_test_pwm_init_multiple_channels();
    run_test_pwm_init_all_channels();
    run_test_pwm_deinit_invalid_channel();
    run_test_pwm_set_duty_invalid_channel();
    run_test_pwm_set_duty_invalid_value();
    run_test_pwm_set_duty_valid();
    run_test_pwm_set_duty_not_initialized();
    run_test_pwm_set_freq_invalid_channel();
    run_test_pwm_set_freq_invalid_value();
    run_test_pwm_set_freq_valid();
    run_test_pwm_set_freq_not_initialized();
    run_test_pwm_start_invalid_channel();
    run_test_pwm_start_stop_cycle();
    run_test_pwm_start_not_initialized();
    run_test_pwm_stop_invalid_channel();
    run_test_pwm_stop_not_initialized();
    run_test_pwm_stop_when_not_running();

    /* Configuration tests */
    run_test_pwm_different_frequencies();
    run_test_pwm_different_duty_cycles();
    run_test_pwm_reinitialize_channel();

    /* Edge case tests */
    run_test_pwm_high_frequency();
    run_test_pwm_low_frequency();
    run_test_pwm_duty_cycle_boundaries();
    run_test_pwm_init_rejects_zero_frequency();
    run_test_pwm_init_rejects_duty_over_limit();

    tests_run = 28;
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
