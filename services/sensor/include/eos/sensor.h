// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file sensor.h
 * @brief EoS Sensor Framework — Unified sensor API
 *
 * Provides a uniform interface for registering, reading, calibrating,
 * and filtering sensor data from any type of sensor.
 */

#ifndef EOS_SENSOR_H
#define EOS_SENSOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <eos/eos_config.h>

#if EOS_ENABLE_SENSOR

#ifdef __cplusplus
extern "C" {
#endif

#define EOS_SENSOR_MAX 16

typedef enum {
    EOS_SENSOR_TEMPERATURE  = 0,
    EOS_SENSOR_HUMIDITY     = 1,
    EOS_SENSOR_PRESSURE     = 2,
    EOS_SENSOR_LIGHT        = 3,
    EOS_SENSOR_PROXIMITY    = 4,
    EOS_SENSOR_ACCELEROMETER = 5,
    EOS_SENSOR_GYROSCOPE    = 6,
    EOS_SENSOR_MAGNETOMETER = 7,
    EOS_SENSOR_HEART_RATE   = 8,
    EOS_SENSOR_DUST         = 9,
    EOS_SENSOR_GAS          = 10,
    EOS_SENSOR_VOLTAGE      = 11,
    EOS_SENSOR_CURRENT      = 12,
    EOS_SENSOR_CUSTOM       = 13,
} eos_sensor_type_t;

typedef enum {
    EOS_FILTER_NONE     = 0,
    EOS_FILTER_AVERAGE  = 1,
    EOS_FILTER_MEDIAN   = 2,
    EOS_FILTER_LOWPASS  = 3,
    EOS_FILTER_KALMAN   = 4,
} eos_filter_type_t;

typedef struct {
    float value;
    float min;
    float max;
    uint32_t timestamp_ms;
    bool valid;
} eos_sensor_reading_t;

typedef struct {
    float offset;
    float scale;
    bool  calibrated;
} eos_sensor_calib_t;

typedef int (*eos_sensor_read_fn)(uint8_t id, float *raw_value);

typedef struct {
    uint8_t            id;
    const char        *name;
    eos_sensor_type_t  type;
    eos_filter_type_t  filter;
    uint8_t            filter_window;
    uint32_t           sample_interval_ms;
    eos_sensor_read_fn read_fn;
} eos_sensor_config_t;

int  eos_sensor_init(void);
void eos_sensor_deinit(void);

int  eos_sensor_register(const eos_sensor_config_t *cfg);
int  eos_sensor_unregister(uint8_t id);

int  eos_sensor_read(uint8_t id, eos_sensor_reading_t *reading);
int  eos_sensor_read_filtered(uint8_t id, eos_sensor_reading_t *reading);

int  eos_sensor_calibrate(uint8_t id, const eos_sensor_calib_t *calib);
int  eos_sensor_get_calibration(uint8_t id, eos_sensor_calib_t *calib);
int  eos_sensor_auto_calibrate(uint8_t id);

int  eos_sensor_set_filter(uint8_t id, eos_filter_type_t filter,
                            uint8_t window_size);

int  eos_sensor_get_count(void);
int  eos_sensor_get_info(uint8_t id, eos_sensor_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* EOS_ENABLE_SENSOR */
#endif /* EOS_SENSOR_H */
