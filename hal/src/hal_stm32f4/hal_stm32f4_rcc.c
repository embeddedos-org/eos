// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file hal_stm32f4_rcc.c
 * @brief STM32F4 Clock Configuration — CMSIS Register-Level
 *
 * Configures: HSE 8MHz → PLL → 168MHz SYSCLK, APB1 42MHz, APB2 84MHz
 */

#include <stdint.h>

#define RCC_BASE        0x40023800UL
#define FLASH_BASE_ADDR 0x40023C00UL

#define RCC_CR       (*(volatile uint32_t *)(RCC_BASE + 0x00))
#define RCC_PLLCFGR  (*(volatile uint32_t *)(RCC_BASE + 0x04))
#define RCC_CFGR     (*(volatile uint32_t *)(RCC_BASE + 0x08))
#define RCC_AHB1ENR  (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB1ENR  (*(volatile uint32_t *)(RCC_BASE + 0x40))
#define RCC_APB2ENR  (*(volatile uint32_t *)(RCC_BASE + 0x44))

#define FLASH_ACR    (*(volatile uint32_t *)(FLASH_BASE_ADDR + 0x00))

#define RCC_CR_HSEON    (1U << 16)
#define RCC_CR_HSERDY   (1U << 17)
#define RCC_CR_PLLON    (1U << 24)
#define RCC_CR_PLLRDY   (1U << 25)

#define RCC_CFGR_SW_PLL     (2U << 0)
#define RCC_CFGR_SWS_PLL    (2U << 2)
#define RCC_CFGR_HPRE_DIV1  (0U << 4)
#define RCC_CFGR_PPRE1_DIV4 (5U << 10)
#define RCC_CFGR_PPRE2_DIV2 (4U << 13)

#define RCC_PLLCFGR_PLLSRC_HSE (1U << 22)

#define FLASH_ACR_LATENCY_5WS (5U << 0)
#define FLASH_ACR_PRFTEN       (1U << 8)
#define FLASH_ACR_ICEN         (1U << 9)
#define FLASH_ACR_DCEN         (1U << 10)

void SystemInit(void)
{
    /* Enable HSE */
    RCC_CR |= RCC_CR_HSEON;
    while (!(RCC_CR & RCC_CR_HSERDY)) {}

    /* Configure Flash latency for 168MHz */
    FLASH_ACR = FLASH_ACR_LATENCY_5WS | FLASH_ACR_PRFTEN |
                FLASH_ACR_ICEN | FLASH_ACR_DCEN;

    /*
     * PLL Configuration: HSE 8MHz input
     *   PLLM = 8  → VCO input  = 8/8    = 1 MHz
     *   PLLN = 336 → VCO output = 1*336  = 336 MHz
     *   PLLP = 2  → SYSCLK     = 336/2  = 168 MHz
     *   PLLQ = 7  → USB clock  = 336/7  = 48 MHz
     */
    RCC_PLLCFGR = (8U << 0)           /* PLLM = 8 */
                | (336U << 6)          /* PLLN = 336 */
                | (0U << 16)           /* PLLP = 2 (00 = /2) */
                | RCC_PLLCFGR_PLLSRC_HSE
                | (7U << 24);          /* PLLQ = 7 */

    /* Enable PLL */
    RCC_CR |= RCC_CR_PLLON;
    while (!(RCC_CR & RCC_CR_PLLRDY)) {}

    /* Configure bus prescalers */
    RCC_CFGR = RCC_CFGR_HPRE_DIV1     /* AHB  = 168 MHz */
             | RCC_CFGR_PPRE1_DIV4    /* APB1 = 42 MHz */
             | RCC_CFGR_PPRE2_DIV2;   /* APB2 = 84 MHz */

    /* Switch to PLL as system clock */
    RCC_CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC_CFGR & (3U << 2)) != RCC_CFGR_SWS_PLL) {}
}

void eos_stm32f4_rcc_enable_gpio(uint32_t port_mask)
{
    RCC_AHB1ENR |= port_mask;
    volatile uint32_t dummy = RCC_AHB1ENR;
    (void)dummy;
}

void eos_stm32f4_rcc_enable_apb1(uint32_t periph_mask)
{
    RCC_APB1ENR |= periph_mask;
    volatile uint32_t dummy = RCC_APB1ENR;
    (void)dummy;
}

void eos_stm32f4_rcc_enable_apb2(uint32_t periph_mask)
{
    RCC_APB2ENR |= periph_mask;
    volatile uint32_t dummy = RCC_APB2ENR;
    (void)dummy;
}

uint32_t eos_stm32f4_rcc_get_sysclk(void) { return 168000000U; }
uint32_t eos_stm32f4_rcc_get_apb1clk(void) { return 42000000U; }
uint32_t eos_stm32f4_rcc_get_apb2clk(void) { return 84000000U; }
