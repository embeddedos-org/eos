// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file hal_pwm.c
 * @brief PWM implementation for STM32F4 and host backends
 *
 * Provides platform-specific PWM implementations:
 * - STM32F4: Hardware PWM using TIM2-TIM5 timers
 * - Host: State-recording stub (no waveform generation) for API-level testing
 */

#include <eos/hal_extended.h>
#include <string.h>

/* ================================================================
 * STM32F4 Hardware PWM Implementation
 * ================================================================ */
#if defined(EOS_MCU_STM32F4) && EOS_ENABLE_PWM

#include "hal_stm32f4_regs.h"

/* Timer base addresses */
#define TIM2_BASE 0x40000000UL
#define TIM3_BASE 0x40000400UL
#define TIM4_BASE 0x40000800UL
#define TIM5_BASE 0x40000C00UL

typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SMCR;
    volatile uint32_t DIER;
    volatile uint32_t SR;
    volatile uint32_t EGR;
    volatile uint32_t CCMR1;
    volatile uint32_t CCMR2;
    volatile uint32_t CCER;
    volatile uint32_t CNT;
    volatile uint32_t PSC;
    volatile uint32_t ARR;
    volatile uint32_t RCR;
    volatile uint32_t CCR1;
    volatile uint32_t CCR2;
    volatile uint32_t CCR3;
    volatile uint32_t CCR4;
    volatile uint32_t BDTR;
    volatile uint32_t DCR;
    volatile uint32_t DMAR;
} TIM_TypeDef;

/* Timer control bits */
#define TIM_CR1_CEN       (1U << 0)
#define TIM_CR1_ARPE      (1U << 7)
#define TIM_EGR_UG        (1U << 0)
#define TIM_CCMR1_OC1PE   (1U << 3)
#define TIM_CCMR2_OC3PE   (1U << 11)
#define TIM_CCER_CC1E     (1U << 0)

/* RCC control */
#define RCC_APB1ENR_TIM2  (1U << 0)

static TIM_TypeDef *const tim_bases[] = {
    (TIM_TypeDef *)TIM2_BASE,
    (TIM_TypeDef *)TIM3_BASE,
    (TIM_TypeDef *)TIM4_BASE,
    (TIM_TypeDef *)TIM5_BASE,
};

/* PWM channel state */
static struct {
    bool initialized;
    bool running;
    uint32_t frequency_hz;
    uint16_t duty_pct_x10;
} pwm_state[16] = {0};

static uint8_t channel_to_timer[] = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3};
static uint8_t channel_to_tim_ch[] = {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3};

/* Assuming 84MHz APB1 clock for STM32F4.
 * PWM_freq = 84MHz / (PSC + 1) / (ARR + 1)
 */
#define TIMER_CLOCK_HZ 84000000UL

/* Caller must have already rejected frequency_hz == 0. Chooses the smallest
 * prescaler that keeps the period within the timer's 16-bit ARR range;
 * rejects frequencies too high for this timer clock to represent (fewer
 * than 2 counter ticks per period) instead of wrapping PSC/ARR. */
static int compute_prescaler_period(uint32_t frequency_hz, uint32_t *out_prescaler, uint32_t *out_period)
{
    uint32_t ticks = TIMER_CLOCK_HZ / frequency_hz;
    if (ticks < 2) return -1;

    uint32_t prescaler = (ticks + 0xFFFFu) / 0x10000u;
    if (prescaler < 1) prescaler = 1;

    *out_prescaler = prescaler;
    *out_period = ticks / prescaler - 1;
    return 0;
}

static bool timer_has_active_channel(uint8_t timer_idx, uint8_t except_channel)
{
    for (uint8_t i = 0; i < 16; i++) {
        if (i != except_channel && channel_to_timer[i] == timer_idx &&
            pwm_state[i].initialized && pwm_state[i].running) {
            return true;
        }
    }
    return false;
}

int eos_pwm_init(const eos_pwm_config_t *cfg)
{
    if (!cfg || cfg->channel >= 16 || cfg->frequency_hz == 0 ||
        cfg->duty_pct_x10 > 1000) {
        return -1;
    }

    uint8_t timer_idx = channel_to_timer[cfg->channel];
    uint8_t tim_ch = channel_to_tim_ch[cfg->channel];

    if (timer_idx >= 4) return -1;

    uint32_t prescaler, period;
    if (compute_prescaler_period(cfg->frequency_hz, &prescaler, &period) != 0) {
        return -1;
    }

    TIM_TypeDef *tim = tim_bases[timer_idx];

    /* Enable timer clock */
    RCC_APB1ENR_REG |= (RCC_APB1ENR_TIM2 << timer_idx);

    /* Configure timer for PWM */
    tim->CR1 = TIM_CR1_ARPE;    /* Auto-reload preload enable */
    tim->CR2 = 0;
    tim->SMCR = 0;
    tim->PSC = prescaler - 1;
    tim->ARR = period;

    /* Configure PWM mode for the channel */
    if (tim_ch < 2) {
        /* Channels 1-2 in CCMR1 */
        uint32_t shift = tim_ch * 8;
        tim->CCMR1 &= ~(0xFFU << shift);
        tim->CCMR1 |= (0x60U << shift) | (TIM_CCMR1_OC1PE << shift);
    } else {
        /* Channels 3-4 in CCMR2 */
        uint32_t shift = (tim_ch - 2) * 8;
        tim->CCMR2 &= ~(0xFFU << shift);
        tim->CCMR2 |= (0x60U << shift) | (TIM_CCMR2_OC3PE << shift);
    }

    /* Set initial duty cycle */
    uint32_t ccr_val = (period * cfg->duty_pct_x10) / 1000;
    switch (tim_ch) {
        case 0: tim->CCR1 = ccr_val; break;
        case 1: tim->CCR2 = ccr_val; break;
        case 2: tim->CCR3 = ccr_val; break;
        case 3: tim->CCR4 = ccr_val; break;
    }

    /* Enable output for the channel */
    tim->CCER |= (TIM_CCER_CC1E << (tim_ch * 4));

    /* Force an update event so the ARR/CCR preload values just written
     * take effect on this period instead of the next overflow. */
    tim->EGR = TIM_EGR_UG;

    /* Store state */
    pwm_state[cfg->channel].initialized = true;
    pwm_state[cfg->channel].running = false;
    pwm_state[cfg->channel].frequency_hz = cfg->frequency_hz;
    pwm_state[cfg->channel].duty_pct_x10 = cfg->duty_pct_x10;

    return 0;
}

void eos_pwm_deinit(uint8_t channel)
{
    if (channel >= 16) return;

    uint8_t timer_idx = channel_to_timer[channel];
    uint8_t tim_ch = channel_to_tim_ch[channel];

    if (timer_idx >= 4) return;

    TIM_TypeDef *tim = tim_bases[timer_idx];

    /* Disable channel output */
    tim->CCER &= ~(TIM_CCER_CC1E << (tim_ch * 4));

    /* Stop the timer only if no other channel on this same timer is
     * still active. */
    if (!timer_has_active_channel(timer_idx, channel)) {
        tim->CR1 &= ~TIM_CR1_CEN;
    }

    pwm_state[channel].initialized = false;
}

int eos_pwm_set_duty(uint8_t channel, uint16_t duty_pct_x10)
{
    if (channel >= 16 || duty_pct_x10 > 1000) return -1;
    if (!pwm_state[channel].initialized) return -1;

    uint8_t timer_idx = channel_to_timer[channel];
    uint8_t tim_ch = channel_to_tim_ch[channel];

    if (timer_idx >= 4) return -1;

    TIM_TypeDef *tim = tim_bases[timer_idx];
    uint32_t period = tim->ARR;
    uint32_t ccr_val = (period * duty_pct_x10) / 1000;

    switch (tim_ch) {
        case 0: tim->CCR1 = ccr_val; break;
        case 1: tim->CCR2 = ccr_val; break;
        case 2: tim->CCR3 = ccr_val; break;
        case 3: tim->CCR4 = ccr_val; break;
    }

    pwm_state[channel].duty_pct_x10 = duty_pct_x10;
    return 0;
}

int eos_pwm_set_freq(uint8_t channel, uint32_t frequency_hz)
{
    if (channel >= 16 || frequency_hz == 0) return -1;
    if (!pwm_state[channel].initialized) return -1;

    uint8_t timer_idx = channel_to_timer[channel];
    if (timer_idx >= 4) return -1;

    uint32_t prescaler, period;
    if (compute_prescaler_period(frequency_hz, &prescaler, &period) != 0) {
        return -1;
    }

    TIM_TypeDef *tim = tim_bases[timer_idx];

    /* Stop timer temporarily */
    bool was_running = pwm_state[channel].running;
    tim->CR1 &= ~TIM_CR1_CEN;

    tim->PSC = prescaler - 1;
    tim->ARR = period;

    /* Restore duty cycle with new period */
    uint16_t duty = pwm_state[channel].duty_pct_x10;
    uint8_t tim_ch = channel_to_tim_ch[channel];
    uint32_t ccr_val = (period * duty) / 1000;

    switch (tim_ch) {
        case 0: tim->CCR1 = ccr_val; break;
        case 1: tim->CCR2 = ccr_val; break;
        case 2: tim->CCR3 = ccr_val; break;
        case 3: tim->CCR4 = ccr_val; break;
    }

    /* Force an update event so the new ARR/CCR take effect immediately */
    tim->EGR = TIM_EGR_UG;

    /* Restart if it was running */
    if (was_running) {
        tim->CR1 |= TIM_CR1_CEN;
    }

    pwm_state[channel].frequency_hz = frequency_hz;
    return 0;
}

int eos_pwm_start(uint8_t channel)
{
    if (channel >= 16) return -1;
    if (!pwm_state[channel].initialized) return -1;

    uint8_t timer_idx = channel_to_timer[channel];
    if (timer_idx >= 4) return -1;

    TIM_TypeDef *tim = tim_bases[timer_idx];
    tim->CR1 |= TIM_CR1_CEN;
    pwm_state[channel].running = true;

    return 0;
}

int eos_pwm_stop(uint8_t channel)
{
    if (channel >= 16) return -1;
    if (!pwm_state[channel].initialized) return -1;

    uint8_t timer_idx = channel_to_timer[channel];
    if (timer_idx >= 4) return -1;

    TIM_TypeDef *tim = tim_bases[timer_idx];

    if (!timer_has_active_channel(timer_idx, channel)) {
        tim->CR1 &= ~TIM_CR1_CEN;
    }

    pwm_state[channel].running = false;
    return 0;
}

#endif /* EOS_MCU_STM32F4 && EOS_ENABLE_PWM */

/* ================================================================
 * Host PWM Implementation (state-recording stub)
 * ================================================================ */
#if !defined(EOS_MCU_STM32F4) && EOS_ENABLE_PWM

static struct {
    bool initialized;
    bool running;
    uint32_t frequency_hz;
    uint16_t duty_pct_x10;
} host_pwm_state[16] = {0};

int eos_pwm_init(const eos_pwm_config_t *cfg)
{
    if (!cfg || cfg->channel >= 16 || cfg->frequency_hz == 0 ||
        cfg->duty_pct_x10 > 1000) {
        return -1;
    }

    host_pwm_state[cfg->channel].initialized = true;
    host_pwm_state[cfg->channel].running = false;
    host_pwm_state[cfg->channel].frequency_hz = cfg->frequency_hz;
    host_pwm_state[cfg->channel].duty_pct_x10 = cfg->duty_pct_x10;

    return 0;
}

void eos_pwm_deinit(uint8_t channel)
{
    if (channel >= 16) return;
    host_pwm_state[channel].initialized = false;
    host_pwm_state[channel].running = false;
}

int eos_pwm_set_duty(uint8_t channel, uint16_t duty_pct_x10)
{
    if (channel >= 16 || duty_pct_x10 > 1000) return -1;
    if (!host_pwm_state[channel].initialized) return -1;

    host_pwm_state[channel].duty_pct_x10 = duty_pct_x10;
    return 0;
}

int eos_pwm_set_freq(uint8_t channel, uint32_t frequency_hz)
{
    if (channel >= 16 || frequency_hz == 0) return -1;
    if (!host_pwm_state[channel].initialized) return -1;

    host_pwm_state[channel].frequency_hz = frequency_hz;
    return 0;
}

int eos_pwm_start(uint8_t channel)
{
    if (channel >= 16) return -1;
    if (!host_pwm_state[channel].initialized) return -1;

    host_pwm_state[channel].running = true;
    return 0;
}

int eos_pwm_stop(uint8_t channel)
{
    if (channel >= 16) return -1;
    if (!host_pwm_state[channel].initialized) return -1;

    host_pwm_state[channel].running = false;
    return 0;
}

#endif /* !EOS_MCU_STM32F4 && EOS_ENABLE_PWM */
