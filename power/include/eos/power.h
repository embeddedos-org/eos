// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file power.h
 * @brief EoS Power Management Framework
 *
 * Sleep modes, battery monitoring, wake sources, per-peripheral power
 * control, and CPU frequency scaling for battery-powered devices.
 */

#ifndef EOS_POWER_H
#define EOS_POWER_H

#include <stdint.h>
#include <stdbool.h>
#include <eos/eos_config.h>

#if EOS_ENABLE_POWER

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Power States
 * ============================================================ */

typedef enum {
    EOS_POWER_RUN        = 0,
    EOS_POWER_SLEEP      = 1,
    EOS_POWER_DEEP_SLEEP = 2,
    EOS_POWER_SHUTDOWN   = 3,
} eos_power_mode_t;

/* ============================================================
 * Wake Sources
 * ============================================================ */

typedef enum {
    EOS_WAKE_GPIO      = (1 << 0),
    EOS_WAKE_TIMER     = (1 << 1),
    EOS_WAKE_RTC       = (1 << 2),
    EOS_WAKE_UART      = (1 << 3),
    EOS_WAKE_BLE       = (1 << 4),
    EOS_WAKE_USB       = (1 << 5),
    EOS_WAKE_INTERRUPT = (1 << 6),
} eos_wake_source_t;

/* ============================================================
 * Peripheral IDs for power control
 * ============================================================ */

typedef enum {
    EOS_PERIPH_GPIO     = 0,
    EOS_PERIPH_UART     = 1,
    EOS_PERIPH_SPI      = 2,
    EOS_PERIPH_I2C      = 3,
    EOS_PERIPH_TIMER    = 4,
    EOS_PERIPH_ADC      = 5,
    EOS_PERIPH_DAC      = 6,
    EOS_PERIPH_PWM      = 7,
    EOS_PERIPH_CAN      = 8,
    EOS_PERIPH_USB      = 9,
    EOS_PERIPH_ETHERNET = 10,
    EOS_PERIPH_WIFI     = 11,
    EOS_PERIPH_BLE      = 12,
    EOS_PERIPH_CAMERA   = 13,
    EOS_PERIPH_AUDIO    = 14,
    EOS_PERIPH_DISPLAY  = 15,
    EOS_PERIPH_MOTOR    = 16,
    EOS_PERIPH_GNSS     = 17,
    EOS_PERIPH_IMU      = 18,
    EOS_PERIPH_MAX      = 19,
} eos_peripheral_id_t;

/* ============================================================
 * Battery Info
 * ============================================================ */

typedef struct {
    uint32_t voltage_mv;
    uint8_t  percentage;
    bool     charging;
    bool     charger_connected;
    int16_t  current_ma;    /* positive = charging, negative = discharging */
    int8_t   temperature_c;
} eos_battery_info_t;

/* ============================================================
 * Power Configuration
 * ============================================================ */

typedef struct {
    uint8_t          adc_channel;       /* ADC channel for battery voltage */
    uint32_t         vbat_full_mv;      /* Battery full voltage (e.g., 4200) */
    uint32_t         vbat_empty_mv;     /* Battery empty voltage (e.g., 3000) */
    uint32_t         divider_ratio_x10; /* Voltage divider ratio * 10 */
} eos_power_config_t;

/* ============================================================
 * API
 * ============================================================ */

int  eos_power_init(const eos_power_config_t *cfg);
void eos_power_deinit(void);

/* Sleep control */
int  eos_power_enter_mode(eos_power_mode_t mode);
eos_power_mode_t eos_power_get_mode(void);

/* Wake source configuration */
int  eos_power_set_wake_sources(uint32_t wake_mask);
uint32_t eos_power_get_wake_source(void);

/* Battery monitoring */
uint32_t eos_power_get_battery_mv(void);
uint8_t  eos_power_get_battery_pct(void);
int  eos_power_get_battery_info(eos_battery_info_t *info);

/* Per-peripheral power control */
int  eos_power_enable_peripheral(eos_peripheral_id_t periph);
int  eos_power_disable_peripheral(eos_peripheral_id_t periph);
bool eos_power_is_peripheral_enabled(eos_peripheral_id_t periph);

/* CPU frequency scaling */
int  eos_power_set_cpu_freq(uint32_t freq_hz);
uint32_t eos_power_get_cpu_freq(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_ENABLE_POWER */
#endif /* EOS_POWER_H */
