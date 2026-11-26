// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file power.h
 * @brief EoS Power Management Framework
 *
 * Sleep modes, battery monitoring, wake sources, per-peripheral power
 * control, and CPU frequency scaling for battery-powered devices.
 *
 * Every operation here needs hardware: entering a sleep mode, arming a wake
 * source, gating a peripheral clock, reading a battery sense pin and changing
 * the core clock are all register writes on a specific part. This framework
 * owns none of them. It validates arguments, keeps the configuration, and
 * forwards the work to an ::eos_power_backend_t that a board port registers
 * with eos_power_register_backend().
 *
 * With no backend registered there is nothing to forward to, and every such
 * call returns ::EOS_POWER_ENOTSUP rather than 0 (#288). A caller that asks
 * for deep sleep and is told "success" while the core keeps running at full
 * current is worse off than one that is told the board cannot do it, so the
 * default build answers honestly: no sleep mode exists until a port provides
 * one.
 *
 * Return-code convention for the int-returning calls:
 *   - 0 / ::EOS_POWER_OK       the backend performed the operation
 *   - ::EOS_POWER_EINVAL       bad argument, or the framework is not
 *                              initialised (unchanged from earlier releases)
 *   - ::EOS_POWER_ENOTSUP      no backend, or this backend does not implement
 *                              this operation
 *   - any other negative value passed through unchanged from the backend
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
    uint8_t          adc_channel;       /* ADC channel for battery voltage —
                                         * passed to the backend's
                                         * read_battery_mv() */
    uint32_t         vbat_full_mv;      /* Battery full voltage (e.g., 4200) */
    uint32_t         vbat_empty_mv;     /* Battery empty voltage (e.g., 3000) */
    uint32_t         divider_ratio_x10; /* Sense-divider ratio * 10. The
                                         * backend reports the voltage at the
                                         * sense pin; the framework multiplies
                                         * by this/10 to get the battery
                                         * voltage. 0 or 10 means no divider. */
} eos_power_config_t;

/* ============================================================
 * Return Codes
 * ============================================================ */

#define EOS_POWER_OK        0    /**< Operation performed. */
#define EOS_POWER_EINVAL  (-1)   /**< Bad argument / not initialised. */
#define EOS_POWER_ENOTSUP (-2)   /**< No backend implements this operation. */

/* ============================================================
 * Board Backend
 * ============================================================
 *
 * Same shape as eos_hal_register_ext_backend() in hal/include/eos/hal.h: a
 * struct of optional function pointers, one registered at a time, NULL
 * members meaning "this part cannot do that". A NULL member is not an error
 * in itself — it is what turns the matching API call into
 * ::EOS_POWER_ENOTSUP instead of a silent no-op.
 *
 * ## What a callback must guarantee on failure
 *
 * A callback that returns nonzero must leave the hardware as it found it.
 * The framework does not update its cached state when a callback fails --
 * eos_power_is_peripheral_enabled() and eos_power_get_cpu_freq() keep
 * reporting what was last applied successfully -- so a backend that changes
 * the hardware and *then* reports an error leaves the two disagreeing, and
 * the framework has no way to detect it.
 *
 * Where a change cannot be made atomic, the callback should undo what it
 * managed before returning the error. If it genuinely cannot, it should
 * apply what it can and return ::EOS_POWER_OK, so the cached state matches
 * the hardware; reporting an error is for "nothing changed".
 *
 * ## Threading
 *
 * None of this is synchronised. The framework holds its state in ordinary
 * statics and calls backend callbacks without a lock, so registration,
 * initialisation, teardown and operations must not run concurrently.
 * Treat power setup as single-threaded and the operations as serialised by
 * the caller. See issue #305.
 */

typedef struct {
    const char *name;  /**< Backend identifier, e.g. "stm32f407". */

    /**
     * @brief Enter @p mode and block until the part leaves it.
     *
     * Called only for EOS_POWER_SLEEP, EOS_POWER_DEEP_SLEEP and
     * EOS_POWER_SHUTDOWN; EOS_POWER_RUN is handled by the framework, since a
     * core executing the call is already running.
     *
     * For the sleep modes the call is expected to return once a wake source
     * fires, and to store that source (an ::eos_wake_source_t bit) in
     * @p wake_source_out, or 0 when the part cannot report it. For
     * EOS_POWER_SHUTDOWN the call is not expected to return at all.
     *
     * @return 0 when the mode was entered, negative errno-style code otherwise.
     */
    int (*enter_mode)(eos_power_mode_t mode, uint32_t *wake_source_out);

    /** @brief Arm exactly the wake sources in @p wake_mask. Returns 0 on
     *  success. Required for eos_power_set_wake_sources() to do anything. */
    int (*set_wake_sources)(uint32_t wake_mask);

    /** @brief Read the battery sense pin on @p adc_channel, in millivolts
     *  at the pin (before the divider). Returns 0 on success. */
    int (*read_battery_mv)(uint8_t adc_channel, uint32_t *mv_out);

    /** @brief Optional full fuel-gauge read. When present,
     *  eos_power_get_battery_info() delegates to it instead of deriving the
     *  struct from read_battery_mv(). Returns 0 on success. */
    int (*read_battery_info)(eos_battery_info_t *info);

    /** @brief Gate (@p enable false) or ungate a peripheral's power/clock.
     *  Returns 0 on success. */
    int (*set_peripheral)(eos_peripheral_id_t periph, bool enable);

    /** @brief Switch the core clock to @p freq_hz, storing the frequency
     *  actually reached in @p actual_hz_out. Returns 0 on success. */
    int (*set_cpu_freq)(uint32_t freq_hz, uint32_t *actual_hz_out);
} eos_power_backend_t;

/**
 * @brief Register the board power backend, or pass NULL to unregister.
 *
 * The pointer is stored, not copied: @p backend must outlive the framework
 * (a file-scope const struct in the board port). Registration survives
 * eos_power_init()/eos_power_deinit(); it is a property of the board, not of
 * a framework session.
 *
 * @return 0 always.
 */
int eos_power_register_backend(const eos_power_backend_t *backend);

/** @brief The registered backend, or NULL when the board provides none. */
const eos_power_backend_t *eos_power_get_backend(void);

/* ============================================================
 * API
 * ============================================================ */

int  eos_power_init(const eos_power_config_t *cfg);
void eos_power_deinit(void);

/* Sleep control */

/**
 * @brief Enter a power mode, blocking until the part leaves it again.
 *
 * EOS_POWER_RUN succeeds without a backend: the core running this call is
 * already in RUN. Every other mode is the backend's enter_mode(), so without
 * one this returns ::EOS_POWER_ENOTSUP and the part keeps running — it does
 * not report success for a mode nobody entered.
 *
 * @return ::EOS_POWER_OK once the part has woken again, ::EOS_POWER_EINVAL
 *         for an out-of-range mode or before eos_power_init(),
 *         ::EOS_POWER_ENOTSUP when no backend implements it, or the
 *         backend's own negative code.
 */
int  eos_power_enter_mode(eos_power_mode_t mode);

/**
 * @brief The mode the part is in right now.
 *
 * EOS_POWER_RUN whenever the caller can observe it from the CPU that slept,
 * because a core that is executing is running. It reads as the sleeping mode
 * only from another core or an interrupt taken while
 * eos_power_enter_mode() is blocked inside the backend.
 */
eos_power_mode_t eos_power_get_mode(void);

/* Wake source configuration */

/**
 * @brief Arm the wake sources in @p wake_mask (::eos_wake_source_t bits).
 *
 * @return ::EOS_POWER_OK when the backend armed them, ::EOS_POWER_ENOTSUP
 *         when no backend can — the mask is not cached in that case, because
 *         a stored mask that arms nothing is indistinguishable from a working
 *         one.
 */
int  eos_power_set_wake_sources(uint32_t wake_mask);

/**
 * @brief The ::eos_wake_source_t bit that ended the last sleep.
 *
 * 0 when the part has not slept yet, or when the backend could not tell which
 * source fired.
 */
uint32_t eos_power_get_wake_source(void);

/* Battery monitoring */

/**
 * @brief Battery voltage in millivolts, divider compensation applied.
 *
 * @return 0 when it is not known: before eos_power_init(), when no backend
 *         implements read_battery_mv(), or when that read failed. 0 mV is not
 *         a plausible live battery, and eos_power_get_battery_info() reports
 *         the same condition as an error code for callers that need to tell
 *         "unknown" from a reading.
 */
uint32_t eos_power_get_battery_mv(void);

/**
 * @brief Battery charge as a percentage of the configured vbat window.
 *
 * @return 0 when the voltage is unknown (see eos_power_get_battery_mv()) as
 *         well as when the battery really is at or below vbat_empty_mv.
 *         Earlier releases returned 100 on a board with no battery sense at
 *         all (#288).
 */
uint8_t  eos_power_get_battery_pct(void);

/**
 * @brief Fill @p info from the backend's fuel gauge.
 *
 * Fields the backend does not report are left zero; in particular charging,
 * charger_connected, current_ma and temperature_c are 0 unless the backend
 * implements read_battery_info().
 *
 * @return ::EOS_POWER_OK, ::EOS_POWER_EINVAL for a NULL @p info or before
 *         init, or ::EOS_POWER_ENOTSUP when the board has no battery sense.
 */
int  eos_power_get_battery_info(eos_battery_info_t *info);

/* Per-peripheral power control */

/**
 * @brief Power/clock @p periph on.
 * @return ::EOS_POWER_OK, ::EOS_POWER_EINVAL for an invalid id, or
 *         ::EOS_POWER_ENOTSUP when no backend gates peripherals.
 */
int  eos_power_enable_peripheral(eos_peripheral_id_t periph);

/**
 * @brief Power/clock @p periph off.
 * @return as eos_power_enable_peripheral().
 */
int  eos_power_disable_peripheral(eos_peripheral_id_t periph);

/**
 * @brief The last state successfully applied to @p periph through a backend.
 *
 * A record of what this framework asked for and got, not a hardware read, and
 * seeded true at eos_power_init(). With no backend registered nothing is ever
 * applied, so it keeps reporting that seed.
 */
bool eos_power_is_peripheral_enabled(eos_peripheral_id_t periph);

/* CPU frequency scaling */

/**
 * @brief Switch the core clock to @p freq_hz.
 * @return ::EOS_POWER_OK, ::EOS_POWER_EINVAL before init, or
 *         ::EOS_POWER_ENOTSUP when no backend scales the clock.
 */
int  eos_power_set_cpu_freq(uint32_t freq_hz);

/**
 * @brief The core frequency the backend last reported reaching, 0 if unknown.
 */
uint32_t eos_power_get_cpu_freq(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_ENABLE_POWER */
#endif /* EOS_POWER_H */
