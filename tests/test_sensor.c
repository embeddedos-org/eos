// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eos/sensor.h"

static void test_sensor_init(void) {
    assert(eos_sensor_init() == 0);
    assert(eos_sensor_get_count() == 0);
    eos_sensor_deinit();
    printf("[PASS] sensor init\n");
}

static void test_sensor_register(void) {
    eos_sensor_init();
    eos_sensor_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    cfg.id = 0; cfg.name = "temp"; cfg.type = EOS_SENSOR_TEMPERATURE;
    assert(eos_sensor_register(&cfg) == 0);
    assert(eos_sensor_get_count() == 1);
    assert(eos_sensor_register(&cfg) == -1);
    assert(eos_sensor_unregister(0) == 0);
    assert(eos_sensor_get_count() == 0);
    eos_sensor_deinit();
    printf("[PASS] sensor register\n");
}

static void test_sensor_register_invalid_filter_config(void) {
    eos_sensor_init();

    eos_sensor_config_t bad_window_cfg; memset(&bad_window_cfg, 0, sizeof(bad_window_cfg));
    bad_window_cfg.id = 0;
    bad_window_cfg.filter = EOS_FILTER_AVERAGE;
    bad_window_cfg.filter_window = EOS_SENSOR_FILTER_MAX_WINDOW + 1;
    assert(eos_sensor_register(&bad_window_cfg) == -1);
    assert(eos_sensor_get_count() == 0);

    eos_sensor_config_t bad_filter_cfg; memset(&bad_filter_cfg, 0, sizeof(bad_filter_cfg));
    bad_filter_cfg.id = 1;
    bad_filter_cfg.filter = (eos_filter_type_t)99;
    bad_filter_cfg.filter_window = 3;
    assert(eos_sensor_register(&bad_filter_cfg) == -1);
    assert(eos_sensor_get_count() == 0);

    eos_sensor_deinit();
    printf("[PASS] sensor register invalid filter config\n");
}

static void test_sensor_read(void) {
    eos_sensor_init();
    eos_sensor_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    cfg.id = 0; cfg.name = "t";
    eos_sensor_register(&cfg);
    eos_sensor_reading_t r;
    (void)r;
    assert(eos_sensor_read(0, &r) == 0);
    assert(r.valid);
    eos_sensor_deinit();
    printf("[PASS] sensor read\n");
}

static void test_sensor_calibrate(void) {
    eos_sensor_init();
    eos_sensor_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    cfg.id = 0;
    eos_sensor_register(&cfg);
    eos_sensor_calib_t cal = { .offset = 10.0f, .scale = 2.0f, .calibrated = true };
    assert(eos_sensor_calibrate(0, &cal) == 0);
    eos_sensor_calib_t out;
    (void)out;
    assert(eos_sensor_get_calibration(0, &out) == 0);
    assert(out.calibrated);
    eos_sensor_deinit();
    printf("[PASS] sensor calibrate\n");
}

static void test_sensor_filter(void) {
    eos_sensor_init();
    eos_sensor_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    cfg.id = 0;
    eos_sensor_register(&cfg);
    assert(eos_sensor_set_filter(0, EOS_FILTER_AVERAGE, 8) == 0);
    eos_sensor_reading_t r;
    for (int i = 0; i < 10; i++)
        eos_sensor_read_filtered(0, &r);
    assert(r.value > 5.4f && r.value < 5.6f);
    eos_sensor_deinit();
    printf("[PASS] sensor filter\n");
}

static void test_sensor_filter_window(void) {
    eos_sensor_init();
    eos_sensor_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    cfg.id = 0;
    eos_sensor_register(&cfg);
    assert(eos_sensor_set_filter(0, EOS_FILTER_AVERAGE, 3) == 0);

    eos_sensor_reading_t r;
    for (int i = 0; i < 5; i++)
        eos_sensor_read_filtered(0, &r);

    assert(r.value > 2.9f && r.value < 3.1f);

    eos_sensor_config_t info;
    assert(eos_sensor_get_info(0, &info) == 0);
    assert(info.filter_window == 3);

    eos_sensor_deinit();
    printf("[PASS] sensor filter window\n");
}

static void test_sensor_filter_window_one(void) {
    eos_sensor_init();
    eos_sensor_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    cfg.id = 0;
    eos_sensor_register(&cfg);
    assert(eos_sensor_set_filter(0, EOS_FILTER_AVERAGE, 1) == 0);

    eos_sensor_reading_t r;
    for (int i = 0; i < 5; i++)
        eos_sensor_read_filtered(0, &r);

    assert(r.value > 3.9f && r.value < 4.1f);

    eos_sensor_deinit();
    printf("[PASS] sensor filter window one\n");
}

static void test_sensor_filter_default_max(void) {
    eos_sensor_init();
    eos_sensor_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    cfg.id = 0;
    eos_sensor_register(&cfg);
    assert(eos_sensor_set_filter(0, EOS_FILTER_AVERAGE, 0) == 0);

    eos_sensor_reading_t r;
    for (int i = 0; i < 10; i++)
        eos_sensor_read_filtered(0, &r);

    assert(r.value > 4.4f && r.value < 4.6f);

    eos_sensor_config_t info;
    assert(eos_sensor_get_info(0, &info) == 0);
    assert(info.filter_window == EOS_SENSOR_FILTER_MAX_WINDOW);

    eos_sensor_deinit();
    printf("[PASS] sensor filter default max\n");
}

static void test_sensor_filter_exact_max(void) {
    eos_sensor_init();
    eos_sensor_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    cfg.id = 0;
    eos_sensor_register(&cfg);
    assert(eos_sensor_set_filter(0, EOS_FILTER_AVERAGE, EOS_SENSOR_FILTER_MAX_WINDOW) == 0);

    eos_sensor_reading_t r;
    for (int i = 0; i < 10; i++)
        eos_sensor_read_filtered(0, &r);

    assert(r.value > 4.4f && r.value < 4.6f);

    eos_sensor_config_t info;
    assert(eos_sensor_get_info(0, &info) == 0);
    assert(info.filter_window == EOS_SENSOR_FILTER_MAX_WINDOW);

    eos_sensor_deinit();
    printf("[PASS] sensor filter exact max\n");
}

static void test_sensor_filter_invalid_window(void) {
    eos_sensor_init();
    eos_sensor_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    cfg.id = 0;
    eos_sensor_register(&cfg);

    assert(eos_sensor_set_filter(0, EOS_FILTER_AVERAGE, EOS_SENSOR_FILTER_MAX_WINDOW + 1) == -1);

    eos_sensor_config_t info;
    assert(eos_sensor_get_info(0, &info) == 0);
    assert(info.filter == EOS_FILTER_NONE);
    assert(info.filter_window == 0);

    eos_sensor_deinit();
    printf("[PASS] sensor filter invalid window\n");
}

static void test_sensor_filter_invalid_type(void) {
    eos_sensor_init();
    eos_sensor_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    cfg.id = 0;
    eos_sensor_register(&cfg);

    assert(eos_sensor_set_filter(0, (eos_filter_type_t)99, 3) == -1);

    eos_sensor_config_t info;
    assert(eos_sensor_get_info(0, &info) == 0);
    assert(info.filter == EOS_FILTER_NONE);
    assert(info.filter_window == 0);

    eos_sensor_deinit();
    printf("[PASS] sensor filter invalid type\n");
}

static void test_sensor_null(void) {
    assert(eos_sensor_register(NULL) == -1);
    assert(eos_sensor_read(0, NULL) == -1);
    assert(eos_sensor_calibrate(0, NULL) == -1);
    printf("[PASS] sensor null\n");
}

int main(void) {
    printf("=== EoS Sensor Tests ===\n");
    test_sensor_init();
    test_sensor_register();
    test_sensor_register_invalid_filter_config();
    test_sensor_read();
    test_sensor_calibrate();
    test_sensor_filter();
    test_sensor_filter_window();
    test_sensor_filter_window_one();
    test_sensor_filter_default_max();
    test_sensor_filter_exact_max();
    test_sensor_filter_invalid_window();
    test_sensor_filter_invalid_type();
    test_sensor_null();
    printf("=== ALL SENSOR TESTS PASSED (13/13) ===\n");
    return 0;
}
