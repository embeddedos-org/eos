// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file hal_stm32f4_spi.c
 * @brief STM32F4 SPI1 Master Driver — CMSIS Register-Level
 */

#include <stdint.h>
#include <stddef.h>

#define SPI1_BASE 0x40013000UL

typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SR;
    volatile uint32_t DR;
    volatile uint32_t CRCPR;
    volatile uint32_t RXCRCR;
    volatile uint32_t TXCRCR;
    volatile uint32_t I2SCFGR;
    volatile uint32_t I2SPR;
} SPI_TypeDef;

#define SPI1 ((SPI_TypeDef *)SPI1_BASE)

#define SPI_CR1_SPE     (1U << 6)
#define SPI_CR1_MSTR    (1U << 2)
#define SPI_CR1_SSI     (1U << 8)
#define SPI_CR1_SSM     (1U << 9)
#define SPI_SR_RXNE     (1U << 0)
#define SPI_SR_TXE      (1U << 1)
#define SPI_SR_BSY      (1U << 7)

extern void eos_stm32f4_rcc_enable_apb2(uint32_t periph_mask);
#define RCC_APB2ENR_SPI1EN (1U << 12)

int eos_stm32f4_spi_init(uint8_t prescaler)
{
    eos_stm32f4_rcc_enable_apb2(RCC_APB2ENR_SPI1EN);

    SPI1->CR1 = 0;
    SPI1->CR1 = SPI_CR1_MSTR
              | SPI_CR1_SSM
              | SPI_CR1_SSI
              | ((uint32_t)(prescaler & 7U) << 3);
    SPI1->CR2 = 0;
    SPI1->CR1 |= SPI_CR1_SPE;

    return 0;
}

uint8_t eos_stm32f4_spi_transfer(uint8_t data)
{
    while (!(SPI1->SR & SPI_SR_TXE)) {}
    SPI1->DR = data;
    while (!(SPI1->SR & SPI_SR_RXNE)) {}
    return (uint8_t)SPI1->DR;
}

int eos_stm32f4_spi_write(const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++) {
        eos_stm32f4_spi_transfer(p[i]);
    }
    while (SPI1->SR & SPI_SR_BSY) {}
    return (int)len;
}

int eos_stm32f4_spi_read(void *buf, size_t len)
{
    uint8_t *p = (uint8_t *)buf;
    for (size_t i = 0; i < len; i++) {
        p[i] = eos_stm32f4_spi_transfer(0xFF);
    }
    return (int)len;
}
