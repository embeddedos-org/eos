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

static void test_sensor_read(void) {
    eos_sensor_init();
    eos_sensor_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    cfg.id = 0; cfg.name = "t";
    eos_sensor_register(&cfg);
    eos_sensor_reading_t r;
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
    eos_sensor_deinit();
    printf("[PASS] sensor filter\n");
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
    test_sensor_read();
    test_sensor_calibrate();
    test_sensor_filter();
    test_sensor_null();
    printf("=== ALL SENSOR TESTS PASSED (6/6) ===\n");
    return 0;
}