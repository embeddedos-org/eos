// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file hal_stm32f4_gpio.c
 * @brief STM32F4 GPIO Driver — CMSIS Register-Level
 */

#include <stdint.h>

#define GPIOA_BASE 0x40020000UL
#define GPIO_STRIDE 0x400UL

typedef struct {
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFR[2];
} GPIO_TypeDef;

#define GPIO_PORT(n) ((GPIO_TypeDef *)(GPIOA_BASE + (n) * GPIO_STRIDE))

extern void eos_stm32f4_rcc_enable_gpio(uint32_t port_mask);

int eos_stm32f4_gpio_init(uint8_t port, uint8_t pin, uint8_t mode,
                           uint8_t pull, uint8_t speed)
{
    if (port > 8 || pin > 15) return -1;

    eos_stm32f4_rcc_enable_gpio(1U << port);
    GPIO_TypeDef *gpio = GPIO_PORT(port);

    /* Mode: 00=Input, 01=Output, 10=AF, 11=Analog */
    gpio->MODER &= ~(3U << (pin * 2));
    gpio->MODER |= ((uint32_t)(mode & 3U) << (pin * 2));

    /* Output speed */
    gpio->OSPEEDR &= ~(3U << (pin * 2));
    gpio->OSPEEDR |= ((uint32_t)(speed & 3U) << (pin * 2));

    /* Pull-up/pull-down: 00=None, 01=PU, 10=PD */
    gpio->PUPDR &= ~(3U << (pin * 2));
    gpio->PUPDR |= ((uint32_t)(pull & 3U) << (pin * 2));

    return 0;
}

void eos_stm32f4_gpio_write(uint8_t port, uint8_t pin, uint8_t value)
{
    GPIO_TypeDef *gpio = GPIO_PORT(port);
    if (value) {
        gpio->BSRR = (1U << pin);
    } else {
        gpio->BSRR = (1U << (pin + 16));
    }
}

uint8_t eos_stm32f4_gpio_read(uint8_t port, uint8_t pin)
{
    GPIO_TypeDef *gpio = GPIO_PORT(port);
    return (gpio->IDR & (1U << pin)) ? 1 : 0;
}

void eos_stm32f4_gpio_toggle(uint8_t port, uint8_t pin)
{
    GPIO_TypeDef *gpio = GPIO_PORT(port);
    gpio->ODR ^= (1U << pin);
}

void eos_stm32f4_gpio_set_af(uint8_t port, uint8_t pin, uint8_t af)
{
    GPIO_TypeDef *gpio = GPIO_PORT(port);
    uint8_t idx = pin >> 3;
    uint8_t pos = (pin & 7U) * 4;
    gpio->AFR[idx] &= ~(0xFU << pos);
    gpio->AFR[idx] |= ((uint32_t)(af & 0xFU) << pos);
}
