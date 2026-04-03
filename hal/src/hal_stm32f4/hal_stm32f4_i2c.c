// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file hal_stm32f4_i2c.c
 * @brief STM32F4 I2C1 Master Driver — CMSIS Register-Level
 */

#include <stdint.h>
#include <stddef.h>

#define I2C1_BASE 0x40005400UL

typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t OAR1;
    volatile uint32_t OAR2;
    volatile uint32_t DR;
    volatile uint32_t SR1;
    volatile uint32_t SR2;
    volatile uint32_t CCR;
    volatile uint32_t TRISE;
    volatile uint32_t FLTR;
} I2C_TypeDef;

#define I2C1 ((I2C_TypeDef *)I2C1_BASE)

#define I2C_CR1_PE      (1U << 0)
#define I2C_CR1_START   (1U << 8)
#define I2C_CR1_STOP    (1U << 9)
#define I2C_CR1_ACK     (1U << 10)
#define I2C_CR1_SWRST   (1U << 15)
#define I2C_SR1_SB      (1U << 0)
#define I2C_SR1_ADDR    (1U << 1)
#define I2C_SR1_BTF     (1U << 2)
#define I2C_SR1_RXNE    (1U << 6)
#define I2C_SR1_TXE     (1U << 7)

extern void eos_stm32f4_rcc_enable_apb1(uint32_t periph_mask);
extern uint32_t eos_stm32f4_rcc_get_apb1clk(void);
#define RCC_APB1ENR_I2C1EN (1U << 21)

#define I2C_TIMEOUT 100000U

static int i2c_wait_flag(volatile uint32_t *reg, uint32_t flag)
{
    uint32_t timeout = I2C_TIMEOUT;
    while (!(*reg & flag)) {
        if (--timeout == 0) return -1;
    }
    return 0;
}

int eos_stm32f4_i2c_init(uint32_t speed_hz)
{
    eos_stm32f4_rcc_enable_apb1(RCC_APB1ENR_I2C1EN);

    I2C1->CR1 = I2C_CR1_SWRST;
    I2C1->CR1 = 0;

    uint32_t apb1_mhz = eos_stm32f4_rcc_get_apb1clk() / 1000000U;
    I2C1->CR2 = apb1_mhz & 0x3F;

    /* Standard mode: CCR = f_APB1 / (2 * speed) */
    uint32_t ccr = eos_stm32f4_rcc_get_apb1clk() / (2 * speed_hz);
    if (ccr < 4) ccr = 4;
    I2C1->CCR = ccr;

    I2C1->TRISE = apb1_mhz + 1;
    I2C1->CR1 = I2C_CR1_PE;

    return 0;
}

int eos_stm32f4_i2c_write(uint8_t addr, const uint8_t *data, size_t len)
{
    I2C1->CR1 |= I2C_CR1_START;
    if (i2c_wait_flag(&I2C1->SR1, I2C_SR1_SB) != 0) return -1;

    I2C1->DR = (addr << 1) | 0;
    if (i2c_wait_flag(&I2C1->SR1, I2C_SR1_ADDR) != 0) return -1;
    (void)I2C1->SR2;

    for (size_t i = 0; i < len; i++) {
        if (i2c_wait_flag(&I2C1->SR1, I2C_SR1_TXE) != 0) return -1;
        I2C1->DR = data[i];
    }

    if (i2c_wait_flag(&I2C1->SR1, I2C_SR1_BTF) != 0) return -1;
    I2C1->CR1 |= I2C_CR1_STOP;

    return (int)len;
}

int eos_stm32f4_i2c_read(uint8_t addr, uint8_t *buf, size_t len)
{
    if (len == 0) return 0;

    I2C1->CR1 |= I2C_CR1_ACK | I2C_CR1_START;
    if (i2c_wait_flag(&I2C1->SR1, I2C_SR1_SB) != 0) return -1;

    I2C1->DR = (addr << 1) | 1;
    if (i2c_wait_flag(&I2C1->SR1, I2C_SR1_ADDR) != 0) return -1;
    (void)I2C1->SR2;

    for (size_t i = 0; i < len; i++) {
        if (i == len - 1) {
            I2C1->CR1 &= ~I2C_CR1_ACK;
            I2C1->CR1 |= I2C_CR1_STOP;
        }
        if (i2c_wait_flag(&I2C1->SR1, I2C_SR1_RXNE) != 0) return -1;
        buf[i] = (uint8_t)I2C1->DR;
    }

    return (int)len;
}
