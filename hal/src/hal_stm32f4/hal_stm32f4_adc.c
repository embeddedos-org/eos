// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file hal_stm32f4_adc.c
 * @brief STM32F4 ADC1 Driver — Single channel + temperature sensor
 */

#include <stdint.h>

#define ADC1_BASE   0x40012000UL
#define ADC_COMMON  0x40012300UL

typedef struct {
    volatile uint32_t SR;
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SMPR1;
    volatile uint32_t SMPR2;
    volatile uint32_t JOFR1;
    volatile uint32_t JOFR2;
    volatile uint32_t JOFR3;
    volatile uint32_t JOFR4;
    volatile uint32_t HTR;
    volatile uint32_t LTR;
    volatile uint32_t SQR1;
    volatile uint32_t SQR2;
    volatile uint32_t SQR3;
    volatile uint32_t JSQR;
    volatile uint32_t JDR1;
    volatile uint32_t JDR2;
    volatile uint32_t JDR3;
    volatile uint32_t JDR4;
    volatile uint32_t DR;
} ADC_TypeDef;

typedef struct {
    volatile uint32_t CSR;
    volatile uint32_t CCR;
    volatile uint32_t CDR;
} ADC_Common_TypeDef;

#define ADC1 ((ADC_TypeDef *)ADC1_BASE)
#define ADCC ((ADC_Common_TypeDef *)ADC_COMMON)

#define ADC_SR_EOC      (1U << 1)
#define ADC_CR2_ADON    (1U << 0)
#define ADC_CR2_SWSTART (1U << 30)
#define ADC_CCR_TSVREFE (1U << 23)

extern void eos_stm32f4_rcc_enable_apb2(uint32_t periph_mask);
#define RCC_APB2ENR_ADC1EN (1U << 8)

int eos_stm32f4_adc_init(void)
{
    eos_stm32f4_rcc_enable_apb2(RCC_APB2ENR_ADC1EN);

    ADC1->CR1 = 0;
    ADC1->CR2 = 0;

    /* Prescaler /4 → ADC clock = 84/4 = 21 MHz */
    ADCC->CCR = (1U << 16);

    /* Enable temperature sensor */
    ADCC->CCR |= ADC_CCR_TSVREFE;

    /* Turn on ADC */
    ADC1->CR2 |= ADC_CR2_ADON;

    return 0;
}

uint32_t eos_stm32f4_adc_read(uint8_t channel)
{
    if (channel > 18) return 0;

    /* Set sample time to 84 cycles for this channel */
    if (channel < 10) {
        ADC1->SMPR2 &= ~(7U << (channel * 3));
        ADC1->SMPR2 |= (4U << (channel * 3));
    } else {
        ADC1->SMPR1 &= ~(7U << ((channel - 10) * 3));
        ADC1->SMPR1 |= (4U << ((channel - 10) * 3));
    }

    /* Single conversion, 1 channel in sequence */
    ADC1->SQR1 = 0;
    ADC1->SQR3 = channel;

    /* Start conversion */
    ADC1->CR2 |= ADC_CR2_SWSTART;

    /* Wait for end of conversion */
    while (!(ADC1->SR & ADC_SR_EOC)) {}

    return ADC1->DR & 0xFFF;
}

/* Centi-degrees from a 12-bit temperature-sensor sample (3.3 V reference).
 * V_sense = raw * 3.3 / 4096; T(°C) = (V_sense - 0.76) / 0.0025 + 25.
 * In microvolts, T(centi-°C) = (uV - 760000) / 25 + 2500. The old form
 * added whole degrees, (cV - 76) * 100 / 25, to 2500 centi-degrees, so
 * readings stayed within a few tenths of 25.00 °C, and the centivolt step
 * lost about 4 °C of resolution. Pure, so tests/test_stm32f4_conv.c
 * runs it on the host. */
int32_t eos_stm32f4_adc_temp_from_raw(uint32_t raw)
{
    int64_t uv = (int64_t)(raw & 0xFFFU) * 3300000 / 4096;
    return (int32_t)((uv - 760000) / 25 + 2500);
}

int32_t eos_stm32f4_adc_read_temperature(void)
{
    uint32_t raw = eos_stm32f4_adc_read(16); /* Channel 16 = temp sensor */
    return eos_stm32f4_adc_temp_from_raw(raw); /* e.g. 2500 = 25.00 °C */
}
