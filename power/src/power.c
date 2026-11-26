// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file power.c
 * @brief EoS Power Management — framework over a board power backend
 *
 * Nothing in this file touches hardware, and nothing in this file pretends
 * to. Entering a sleep mode, arming a wake source, gating a peripheral clock,
 * reading a battery sense pin and retuning the core clock are all part
 * specific register writes; they arrive through the eos_power_backend_t a
 * board port registers.
 *
 * Until #288 this file answered 0 to all of them while doing nothing but
 * assigning a static: eos_power_enter_mode(EOS_POWER_DEEP_SLEEP) reported
 * success with the core still running at full current, and
 * eos_power_get_battery_mv() returned the configured full-charge constant, so
 * every board read 100%. Those calls now return EOS_POWER_ENOTSUP when no
 * backend implements them. A caller can handle "this board cannot sleep"; it
 * cannot handle a lie.
 */

#include <eos/power.h>
#include <stdint.h>   /* UINT32_MAX, uint64_t: battery divider maths */
#include <string.h>

#if EOS_ENABLE_POWER

/* None of this state is synchronised: plain statics, and backend callbacks
 * called without a lock. Power setup and operations must not run
 * concurrently -- see power.h "Threading" and issue #305, which also
 * covers making current_mode atomic for the sleeping-mode observation
 * eos_power_enter_mode() deliberately exposes. */
static const eos_power_backend_t *s_backend = NULL;

static eos_power_mode_t current_mode = EOS_POWER_RUN;
static uint32_t wake_mask_cfg = 0;
static uint32_t last_wake_source = 0;
static bool periph_enabled[EOS_PERIPH_MAX];
static uint32_t cpu_freq = 0;
static eos_power_config_t power_cfg;
static bool power_initialized = false;

int eos_power_register_backend(const eos_power_backend_t *backend)
{
    /* NULL unregisters: a board that hands control back, and a test that has
     * to get back to the no-backend case, both need that. */
    s_backend = backend;
    return EOS_POWER_OK;
}

const eos_power_backend_t *eos_power_get_backend(void)
{
    return s_backend;
}

int eos_power_init(const eos_power_config_t *cfg)
{
    if (!cfg) return EOS_POWER_EINVAL;
    memcpy(&power_cfg, cfg, sizeof(power_cfg));
    current_mode = EOS_POWER_RUN;
    wake_mask_cfg = 0;
    last_wake_source = 0;
    for (int i = 0; i < EOS_PERIPH_MAX; i++) {
        periph_enabled[i] = true;
    }
    cpu_freq = 0;
    power_initialized = true;
    return EOS_POWER_OK;
}

void eos_power_deinit(void)
{
    power_initialized = false;
}

int eos_power_enter_mode(eos_power_mode_t mode)
{
    if (!power_initialized || mode < EOS_POWER_RUN || mode > EOS_POWER_SHUTDOWN) {
        return EOS_POWER_EINVAL;
    }

    /* RUN needs no backend: the core executing this call is already in it. */
    if (mode == EOS_POWER_RUN) {
        current_mode = EOS_POWER_RUN;
        return EOS_POWER_OK;
    }

    if (!s_backend || !s_backend->enter_mode) return EOS_POWER_ENOTSUP;

    uint32_t woke_by = 0;

    /* Visible to an interrupt or another core while the call below blocks;
     * the caller itself cannot observe it, which is the point. */
    current_mode = mode;
    int rc = s_backend->enter_mode(mode, &woke_by);
    /* Control came back, so whatever the backend did, this core is running. */
    current_mode = EOS_POWER_RUN;

    if (rc != 0) return rc;

    last_wake_source = woke_by;
    return EOS_POWER_OK;
}

eos_power_mode_t eos_power_get_mode(void)
{
    return current_mode;
}

/* Every bit eos_wake_source_t defines. A mask outside this is not an
 * obscure wake source the backend might know about -- it is a caller
 * mistake, and forwarding it would cache a request the API cannot express
 * and report success for it. */
#define EOS_WAKE_ALL (EOS_WAKE_GPIO | EOS_WAKE_TIMER | EOS_WAKE_RTC | \
                      EOS_WAKE_UART | EOS_WAKE_BLE | EOS_WAKE_USB | \
                      EOS_WAKE_INTERRUPT)

int eos_power_set_wake_sources(uint32_t wake_mask)
{
    if (!power_initialized) return EOS_POWER_EINVAL;
    if ((wake_mask & ~(uint32_t)EOS_WAKE_ALL) != 0u) return EOS_POWER_EINVAL;
    if (!s_backend || !s_backend->set_wake_sources) return EOS_POWER_ENOTSUP;

    int rc = s_backend->set_wake_sources(wake_mask);
    if (rc != 0) return rc;

    wake_mask_cfg = wake_mask;
    return EOS_POWER_OK;
}

uint32_t eos_power_get_wake_source(void)
{
    return last_wake_source;
}

/*
 * One battery reading, reporting the backend's own result.
 *
 * eos_power_get_battery_mv() has to flatten every failure to 0 because its
 * return type has no room for an error, which loses the difference between
 * "this backend cannot read the battery" and "the read was attempted and
 * failed". eos_power_get_battery_info() can tell callers apart, so it uses
 * this instead.
 */
static int power_read_battery_mv(uint32_t *mv_out)
{
    if (!power_initialized) return EOS_POWER_EINVAL;
    if (!s_backend || !s_backend->read_battery_mv) return EOS_POWER_ENOTSUP;

    uint32_t pin_mv = 0;
    int rc = s_backend->read_battery_mv(power_cfg.adc_channel, &pin_mv);
    if (rc != 0) return rc;                 /* the backend's own error */

    /* The backend reads the divider's output; the battery is upstream of
     * it. power.h: "0 or 10 means no divider" -- so every other value is a
     * divider, including a ratio below 10, which scales the reading down.
     * In 64-bit: a large configured ratio times a large pin reading
     * overflows uint32_t, and a wrapped battery voltage is worse than an
     * error. */
    uint32_t ratio = power_cfg.divider_ratio_x10;
    if (ratio != 0u && ratio != 10u) {
        uint64_t scaled = ((uint64_t)pin_mv * (uint64_t)ratio) / 10u;
        if (scaled > UINT32_MAX) return EOS_POWER_EINVAL;
        pin_mv = (uint32_t)scaled;
    }

    *mv_out = pin_mv;
    return EOS_POWER_OK;
}

uint32_t eos_power_get_battery_mv(void)
{
    uint32_t mv = 0;
    if (power_read_battery_mv(&mv) != EOS_POWER_OK) return 0;
    return mv;
}

/* Percentage for a voltage already read, so a caller that has a sample can
 * convert it without reading the battery a second time. */
static uint8_t battery_pct_from_mv(uint32_t mv)
{
    /* 0 mV is "no reading", not "a flat battery measured at zero volts", and
     * both answer 0% here -- eos_power_get_battery_info() is the call that
     * distinguishes them. */
    if (mv == 0) return 0;
    if (power_cfg.vbat_full_mv <= power_cfg.vbat_empty_mv) return 0;
    if (mv >= power_cfg.vbat_full_mv) return 100;
    if (mv <= power_cfg.vbat_empty_mv) return 0;
    uint32_t range = power_cfg.vbat_full_mv - power_cfg.vbat_empty_mv;
    return (uint8_t)(((mv - power_cfg.vbat_empty_mv) * 100) / range);
}

uint8_t eos_power_get_battery_pct(void)
{
    if (!power_initialized) return 0;
    return battery_pct_from_mv(eos_power_get_battery_mv());
}

int eos_power_get_battery_info(eos_battery_info_t *info)
{
    if (!power_initialized || !info) return EOS_POWER_EINVAL;
    if (!s_backend) return EOS_POWER_ENOTSUP;

    if (s_backend->read_battery_info) {
        memset(info, 0, sizeof(*info));
        return s_backend->read_battery_info(info);
    }

    /* The backend's own error reaches the caller: a failed reading is not
     * the same as an unsupported one, and returning ENOTSUP for both told
     * callers this board has no battery sense when the ADC had simply
     * failed. */
    uint32_t mv = 0;
    int rc = power_read_battery_mv(&mv);
    if (rc != EOS_POWER_OK) return rc;

    /* Charge state, current and temperature need a gauge this backend has
     * not got, so they stay zero rather than being invented. */
    memset(info, 0, sizeof(*info));
    info->voltage_mv = mv;
    /* Converted from the voltage just stored, not read again: a second
     * reading could disagree with info->voltage_mv, or fail and report 0%
     * next to a valid voltage. */
    info->percentage = battery_pct_from_mv(mv);
    return EOS_POWER_OK;
}

static int power_set_peripheral(eos_peripheral_id_t periph, bool enable)
{
    /* As every other entry point does. Without this, a call before
     * eos_power_init() or after eos_power_deinit() still reached a
     * registered backend and changed the hardware, and a later init() reset
     * the shadow to "enabled" without undoing it -- leaving the framework
     * certain of a state the board was not in. */
    if (!power_initialized) return EOS_POWER_EINVAL;
    if ((uintmax_t)periph >= (uintmax_t)EOS_PERIPH_MAX) return EOS_POWER_EINVAL;
    if (!s_backend || !s_backend->set_peripheral) return EOS_POWER_ENOTSUP;

    int rc = s_backend->set_peripheral(periph, enable);
    if (rc != 0) return rc;

    periph_enabled[periph] = enable;
    return EOS_POWER_OK;
}

int eos_power_enable_peripheral(eos_peripheral_id_t periph)
{
    return power_set_peripheral(periph, true);
}

int eos_power_disable_peripheral(eos_peripheral_id_t periph)
{
    return power_set_peripheral(periph, false);
}

bool eos_power_is_peripheral_enabled(eos_peripheral_id_t periph)
{
    if ((uintmax_t)periph >= (uintmax_t)EOS_PERIPH_MAX) return false;
    return periph_enabled[periph];
}

int eos_power_set_cpu_freq(uint32_t freq_hz)
{
    if (!power_initialized) return EOS_POWER_EINVAL;
    if (!s_backend || !s_backend->set_cpu_freq) return EOS_POWER_ENOTSUP;

    uint32_t actual_hz = freq_hz;
    int rc = s_backend->set_cpu_freq(freq_hz, &actual_hz);
    if (rc != 0) return rc;

    /* What the PLL reached, which is not always what was asked for. */
    cpu_freq = actual_hz;
    return EOS_POWER_OK;
}

uint32_t eos_power_get_cpu_freq(void)
{
    return cpu_freq;
}

#endif /* EOS_ENABLE_POWER */
