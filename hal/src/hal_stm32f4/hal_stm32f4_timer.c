// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file hal_stm32f4_timer.c
 * @brief STM32F4 General-Purpose Timer (TIM2-TIM5) — CMSIS Register-Level
 */

#include <stdint.h>

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
    volatile uint32_t RES1;
    volatile uint32_t CCR1;
    volatile uint32_t CCR2;
    volatile uint32_t CCR3;
    volatile uint32_t CCR4;
    volatile uint32_t RES2;
    volatile uint32_t DCR;
    volatile uint32_t DMAR;
} TIM_TypeDef;

#define TIM_CR1_CEN  (1U << 0)
#define TIM_DIER_UIE (1U << 0)
#define TIM_SR_UIF   (1U << 0)

static TIM_TypeDef *tim_base[] = {
    (TIM_TypeDef *)TIM2_BASE,
    (TIM_TypeDef *)TIM3_BASE,
    (TIM_TypeDef *)TIM4_BASE,
    (TIM_TypeDef *)TIM5_BASE,
};

extern void eos_stm32f4_rcc_enable_apb1(uint32_t periph_mask);

int eos_stm32f4_timer_init(uint8_t timer_idx, uint32_t prescaler, uint32_t period)
{
    if (timer_idx > 3) return -1;

    uint32_t rcc_bit = (1U << timer_idx); /* TIM2=bit0, TIM3=bit1, etc. */
    eos_stm32f4_rcc_enable_apb1(rcc_bit);

    TIM_TypeDef *tim = tim_base[timer_idx];
    tim->CR1 = 0;
    tim->PSC = prescaler - 1;
    tim->ARR = period - 1;
    tim->EGR = 1; /* Force update */
    tim->SR  = 0;

    return 0;
}

void eos_stm32f4_timer_start(uint8_t timer_idx)
{
    if (timer_idx > 3) return;
    tim_base[timer_idx]->CR1 |= TIM_CR1_CEN;
}

void eos_stm32f4_timer_stop(uint8_t timer_idx)
{
    if (timer_idx > 3) return;
    tim_base[timer_idx]->CR1 &= ~TIM_CR1_CEN;
}

uint32_t eos_stm32f4_timer_get_count(uint8_t timer_idx)
{
    if (timer_idx > 3) return 0;
    return tim_base[timer_idx]->CNT;
}

void eos_stm32f4_timer_enable_irq(uint8_t timer_idx)
{
    if (timer_idx > 3) return;
    tim_base[timer_idx]->DIER |= TIM_DIER_UIE;
}

int eos_stm32f4_timer_clear_flag(uint8_t timer_idx)
{
    if (timer_idx > 3) return -1;
    if (tim_base[timer_idx]->SR & TIM_SR_UIF) {
        tim_base[timer_idx]->SR &= ~TIM_SR_UIF;
        return 1;
    }
    return 0;
}

void eos_stm32f4_timer_set_pwm(uint8_t timer_idx, uint8_t channel, uint32_t duty)
{
    if (timer_idx > 3 || channel > 3) return;
    TIM_TypeDef *tim = tim_base[timer_idx];

    volatile uint32_t *ccr = &tim->CCR1 + channel;
    *ccr = duty;

    /* PWM mode 1 on selected channel */
    if (channel < 2) {
        uint32_t shift = channel * 8;
        tim->CCMR1 &= ~(0xFFU << shift);
        tim->CCMR1 |= (0x68U << shift); /* OC mode 110 + preload */
    } else {
        uint32_t shift = (channel - 2) * 8;
        tim->CCMR2 &= ~(0xFFU << shift);
        tim->CCMR2 |= (0x68U << shift);
    }
    tim->CCER |= (1U << (channel * 4));
}
