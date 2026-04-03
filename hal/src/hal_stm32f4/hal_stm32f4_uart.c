// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file hal_stm32f4_uart.c
 * @brief STM32F4 USART2 Driver — CMSIS Register-Level (PA2=TX, PA3=RX)
 */

#include <stdint.h>
#include <stddef.h>

#define USART2_BASE 0x40004400UL

typedef struct {
    volatile uint32_t SR;
    volatile uint32_t DR;
    volatile uint32_t BRR;
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t CR3;
    volatile uint32_t GTPR;
} USART_TypeDef;

#define USART2 ((USART_TypeDef *)USART2_BASE)

#define USART_SR_RXNE  (1U << 5)
#define USART_SR_TXE   (1U << 7)
#define USART_SR_TC    (1U << 6)
#define USART_CR1_UE   (1U << 13)
#define USART_CR1_TE   (1U << 3)
#define USART_CR1_RE   (1U << 2)

extern void eos_stm32f4_rcc_enable_apb1(uint32_t periph_mask);
extern void eos_stm32f4_gpio_init(uint8_t port, uint8_t pin, uint8_t mode,
                                   uint8_t pull, uint8_t speed);
extern void eos_stm32f4_gpio_set_af(uint8_t port, uint8_t pin, uint8_t af);
extern uint32_t eos_stm32f4_rcc_get_apb1clk(void);

#define RCC_APB1ENR_USART2EN (1U << 17)

int eos_stm32f4_uart_init(uint32_t baud)
{
    /* Enable USART2 clock */
    eos_stm32f4_rcc_enable_apb1(RCC_APB1ENR_USART2EN);

    /* Configure PA2 (TX) and PA3 (RX) as AF7 */
    eos_stm32f4_gpio_init(0, 2, 2, 1, 3); /* Port A, pin 2, AF mode, pull-up, high speed */
    eos_stm32f4_gpio_init(0, 3, 2, 1, 3);
    eos_stm32f4_gpio_set_af(0, 2, 7);
    eos_stm32f4_gpio_set_af(0, 3, 7);

    /* Disable USART during configuration */
    USART2->CR1 = 0;

    /* Set baud rate: BRR = f_APB1 / baud */
    uint32_t apb1_clk = eos_stm32f4_rcc_get_apb1clk();
    USART2->BRR = (apb1_clk + baud / 2) / baud;

    /* 8N1, enable TX + RX + USART */
    USART2->CR2 = 0;
    USART2->CR3 = 0;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    return 0;
}

void eos_stm32f4_uart_putchar(char c)
{
    while (!(USART2->SR & USART_SR_TXE)) {}
    USART2->DR = (uint8_t)c;
}

int eos_stm32f4_uart_getchar(void)
{
    if (USART2->SR & USART_SR_RXNE) {
        return (int)(USART2->DR & 0xFF);
    }
    return -1;
}

int eos_stm32f4_uart_write(const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++) {
        eos_stm32f4_uart_putchar((char)p[i]);
    }
    return (int)len;
}

int eos_stm32f4_uart_read(void *buf, size_t len)
{
    uint8_t *p = (uint8_t *)buf;
    size_t count = 0;
    while (count < len) {
        int c = eos_stm32f4_uart_getchar();
        if (c < 0) break;
        p[count++] = (uint8_t)c;
    }
    return (int)count;
}
