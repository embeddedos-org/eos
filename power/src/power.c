// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file power.c
 * @brief EoS Power Management — Stub implementation
 */

#include <eos/power.h>
#include <string.h>

#if EOS_ENABLE_POWER

static eos_power_mode_t current_mode = EOS_POWER_RUN;
static uint32_t wake_mask_cfg = 0;
static uint32_t last_wake_source = 0;
static bool periph_enabled[EOS_PERIPH_MAX];
static uint32_t cpu_freq = 0;
static eos_power_config_t power_cfg;
static bool power_initialized = false;

int eos_power_init(const eos_power_config_t *cfg)
{
    if (!cfg) return -1;
    memcpy(&power_cfg, cfg, sizeof(power_cfg));
    current_mode = EOS_POWER_RUN;
    wake_mask_cfg = 0;
    last_wake_source = 0;
    for (int i = 0; i < EOS_PERIPH_MAX; i++) {
        periph_enabled[i] = true;
    }
    power_initialized = true;
    return 0;
}

void eos_power_deinit(void)
{
    power_initialized = false;
}

int eos_power_enter_mode(eos_power_mode_t mode)
{
    if (!power_initialized) return -1;
    current_mode = mode;
    return 0;
}

eos_power_mode_t eos_power_get_mode(void)
{
    return current_mode;
}

int eos_power_set_wake_sources(uint32_t wake_mask)
{
    if (!power_initialized) return -1;
    wake_mask_cfg = wake_mask;
    return 0;
}

uint32_t eos_power_get_wake_source(void)
{
    return last_wake_source;
}

uint32_t eos_power_get_battery_mv(void)
{
    if (!power_initialized) return 0;
    /* Stub: real implementation reads ADC */
    return power_cfg.vbat_full_mv;
}

uint8_t eos_power_get_battery_pct(void)
{
    if (!power_initialized) return 0;
    uint32_t mv = eos_power_get_battery_mv();
    if (mv >= power_cfg.vbat_full_mv) return 100;
    if (mv <= power_cfg.vbat_empty_mv) return 0;
    uint32_t range = power_cfg.vbat_full_mv - power_cfg.vbat_empty_mv;
    return (uint8_t)(((mv - power_cfg.vbat_empty_mv) * 100) / range);
}

int eos_power_get_battery_info(eos_battery_info_t *info)
{
    if (!power_initialized || !info) return -1;
    memset(info, 0, sizeof(*info));
    info->voltage_mv = eos_power_get_battery_mv();
    info->percentage = eos_power_get_battery_pct();
    info->charging = false;
    info->charger_connected = false;
    info->current_ma = 0;
    info->temperature_c = 25;
    return 0;
}

int eos_power_enable_peripheral(eos_peripheral_id_t periph)
{
    if (periph >= EOS_PERIPH_MAX) return -1;
    periph_enabled[periph] = true;
    return 0;
}

int eos_power_disable_peripheral(eos_peripheral_id_t periph)
{
    if (periph >= EOS_PERIPH_MAX) return -1;
    periph_enabled[periph] = false;
    return 0;
}

bool eos_power_is_peripheral_enabled(eos_peripheral_id_t periph)
{
    if (periph >= EOS_PERIPH_MAX) return false;
    return periph_enabled[periph];
}

int eos_power_set_cpu_freq(uint32_t freq_hz)
{
    if (!power_initialized) return -1;
    cpu_freq = freq_hz;
    return 0;
}

uint32_t eos_power_get_cpu_freq(void)
{
    return cpu_freq;
}

#endif /* EOS_ENABLE_POWER */
