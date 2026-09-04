// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file hal_pwm.c
 * @brief PWM implementation for STM32F4 and Linux host backends
 *
 * Provides platform-specific PWM implementations:
 * - STM32F4: Hardware PWM using TIM2-TIM5 timers
 * - Linux: Software PWM simulation using timerfd
 */

#include <eos/hal_extended.h>
#include <string.h>

/* ================================================================
 * STM32F4 Hardware PWM Implementation
 * ================================================================ */
#if defined(EOS_MCU_STM32F4) && EOS_ENABLE_PWM

#define REG32(addr) (*(volatile uint32_t *)(addr))

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
#define TIM_CCMR1_OC1M    (7U << 4)
#define TIM_CCMR1_OC1PE   (1U << 3)
#define TIM_CCMR2_OC3M    (7U << 12)
#define TIM_CCMR2_OC3PE   (1U << 11)
#define TIM_CCER_CC1E     (1U << 0)
#define TIM_CCER_CC2E     (1U << 4)
#define TIM_CCER_CC3E     (1U << 8)
#define TIM_CCER_CC4E     (1U << 12)
#define TIM_BDTR_MOE      (1U << 15)

/* RCC control */
#define RCC_APB1ENR_REG   REG32(0x40023840U)
#define RCC_APB1ENR_TIM2  (1U << 0)
#define RCC_APB1ENR_TIM3  (1U << 1)
#define RCC_APB1ENR_TIM4  (1U << 2)
#define RCC_APB1ENR_TIM5  (1U << 3)

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
} pwm_state[4] = {0};

static uint8_t channel_to_timer[] = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3};
static uint8_t channel_to_tim_ch[] = {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3};

int eos_pwm_init(const eos_pwm_config_t *cfg)
{
    if (!cfg || cfg->channel >= 16) return -1;
    
    uint8_t timer_idx = channel_to_timer[cfg->channel];
    uint8_t tim_ch = channel_to_tim_ch[cfg->channel];
    
    if (timer_idx >= 4) return -1;
    
    TIM_TypeDef *tim = tim_bases[timer_idx];
    
    /* Enable timer clock */
    RCC_APB1ENR_REG |= (RCC_APB1ENR_TIM2 << timer_idx);
    
    /* Configure timer for PWM */
    tim->CR1 = TIM_CR1_ARPE;    /* Auto-reload preload enable */
    tim->CR2 = 0;
    tim->SMCR = 0;
    
    /* Calculate prescaler and period for desired frequency
     * Assuming 84MHz APB1 clock for STM32F4
     * PWM_freq = 84MHz / (PSC + 1) / (ARR + 1)
     */
    uint32_t timer_clock = 84000000;
    uint32_t prescaler = 1;
    uint32_t period = timer_clock / cfg->frequency_hz / prescaler - 1;
    
    if (period > 0xFFFF) {
        prescaler = (period / 0xFFFF) + 1;
        period = timer_clock / cfg->frequency_hz / prescaler - 1;
    }
    
    tim->PSC = prescaler - 1;
    tim->ARR = period;
    tim->RCR = 0;
    
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
    
    /* Enable main output (for advanced timers) */
    tim->BDTR = TIM_BDTR_MOE;
    
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
    
    /* Stop timer if this was the last channel */
    bool any_channel_active = false;
    for (uint8_t i = 0; i < 16; i++) {
        if (i != channel && pwm_state[i].initialized && pwm_state[i].running) {
            any_channel_active = true;
            break;
        }
    }
    
    if (!any_channel_active) {
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
    
    TIM_TypeDef *tim = tim_bases[timer_idx];
    
    /* Recalculate prescaler and period */
    uint32_t timer_clock = 84000000;
    uint32_t prescaler = 1;
    uint32_t period = timer_clock / frequency_hz / prescaler - 1;
    
    if (period > 0xFFFF) {
        prescaler = (period / 0xFFFF) + 1;
        period = timer_clock / frequency_hz / prescaler - 1;
    }
    
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
    
    /* Check if any other channels on this timer are still running */
    bool other_channels_running = false;
    for (uint8_t i = 0; i < 16; i++) {
        if (i != channel && channel_to_timer[i] == timer_idx && 
            pwm_state[i].initialized && pwm_state[i].running) {
            other_channels_running = true;
            break;
        }
    }
    
    if (!other_channels_running) {
        tim->CR1 &= ~TIM_CR1_CEN;
    }
    
    pwm_state[channel].running = false;
    return 0;
}

#endif /* EOS_MCU_STM32F4 && EOS_ENABLE_PWM */

/* ================================================================
 * Host PWM Implementation (Software Simulation)
 * ================================================================ */
#if !defined(EOS_MCU_STM32F4) && EOS_ENABLE_PWM

#include <time.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

static struct {
    bool initialized;
    bool running;
    uint32_t frequency_hz;
    uint16_t duty_pct_x10;
} host_pwm_state[16] = {0};

int eos_pwm_init(const eos_pwm_config_t *cfg)
{
    if (!cfg || cfg->channel >= 16) return -1;
    
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