// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file board_stm32f407vg.c
 * @brief STM32F407VG Discovery board initialization
 *
 * Configures:
 *   1. Clock tree: HSE 8 MHz → PLL → 168 MHz SYSCLK
 *   2. Flash latency: 5 wait states
 *   3. Bus prescalers: AHB/1, APB1/4 (42 MHz), APB2/2 (84 MHz)
 *   4. GPIO pinmux for all Discovery board peripherals
 *   5. Peripheral clock enables
 */

#include "board_stm32f407vg.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))

/* GPIO register helpers */
#define GPIO_BASE(port)     (0x40020000U + (port) * 0x400U)
#define GPIO_MODER(port)    REG32(GPIO_BASE(port) + 0x00)
#define GPIO_OTYPER(port)   REG32(GPIO_BASE(port) + 0x04)
#define GPIO_OSPEEDR(port)  REG32(GPIO_BASE(port) + 0x08)
#define GPIO_PUPDR(port)    REG32(GPIO_BASE(port) + 0x0C)
#define GPIO_AFRL(port)     REG32(GPIO_BASE(port) + 0x20)
#define GPIO_AFRH(port)     REG32(GPIO_BASE(port) + 0x24)

/* ============================================================
 * Pin Configuration Helpers
 * ============================================================ */

static void pin_set_mode(uint8_t port, uint8_t pin, uint8_t mode)
{
    uint32_t reg = GPIO_MODER(port);
    reg &= ~(3U << (pin * 2));
    reg |= ((uint32_t)mode << (pin * 2));
    GPIO_MODER(port) = reg;
}

static void pin_set_af(uint8_t port, uint8_t pin, uint8_t af)
{
    if (pin < 8) {
        uint32_t reg = GPIO_AFRL(port);
        reg &= ~(0xFU << (pin * 4));
        reg |= ((uint32_t)af << (pin * 4));
        GPIO_AFRL(port) = reg;
    } else {
        uint32_t reg = GPIO_AFRH(port);
        reg &= ~(0xFU << ((pin - 8) * 4));
        reg |= ((uint32_t)af << ((pin - 8) * 4));
        GPIO_AFRH(port) = reg;
    }
}

static void pin_set_speed(uint8_t port, uint8_t pin, uint8_t speed)
{
    uint32_t reg = GPIO_OSPEEDR(port);
    reg &= ~(3U << (pin * 2));
    reg |= ((uint32_t)speed << (pin * 2));
    GPIO_OSPEEDR(port) = reg;
}

static void pin_set_pull(uint8_t port, uint8_t pin, uint8_t pull)
{
    uint32_t reg = GPIO_PUPDR(port);
    reg &= ~(3U << (pin * 2));
    reg |= ((uint32_t)pull << (pin * 2));
    GPIO_PUPDR(port) = reg;
}

static void pin_set_otype(uint8_t port, uint8_t pin, uint8_t otype)
{
    if (otype)
        GPIO_OTYPER(port) |= (1U << pin);
    else
        GPIO_OTYPER(port) &= ~(1U << pin);
}

/* Mode constants */
#define MODE_INPUT  0
#define MODE_OUTPUT 1
#define MODE_AF     2
#define MODE_ANALOG 3

/* Speed constants */
#define SPEED_LOW   0
#define SPEED_MED   1
#define SPEED_HIGH  2
#define SPEED_VHIGH 3

/* Pull constants */
#define PULL_NONE   0
#define PULL_UP     1
#define PULL_DOWN   2

/* ============================================================
 * Clock Tree Configuration
 * ============================================================ */

void board_clock_init(void)
{
    /* 1. Enable HSE and wait for ready */
    RCC_CR |= RCC_CR_HSEON;
    while (!(RCC_CR & RCC_CR_HSERDY)) {}

    /* 2. Configure flash latency for 168 MHz */
    FLASH_ACR = (FLASH_ACR & ~0xFU) | FLASH_LATENCY;
    /* Enable prefetch, instruction cache, data cache */
    FLASH_ACR |= (1U << 8) | (1U << 9) | (1U << 10);

    /* 3. Configure PLL: HSE/M * N / P = 8/8 * 336/2 = 168 MHz */
    RCC_CR &= ~RCC_CR_PLLON;  /* Disable PLL before config */

    RCC_PLLCFGR = PLL_M
                | (PLL_N << 6)
                | (((PLL_P / 2) - 1) << 16)  /* P=2 → 0 */
                | RCC_PLLCFGR_PLLSRC_HSE
                | (PLL_Q << 24);

    /* 4. Enable PLL and wait for lock */
    RCC_CR |= RCC_CR_PLLON;
    while (!(RCC_CR & RCC_CR_PLLRDY)) {}

    /* 5. Configure bus prescalers BEFORE switching clock source */
    RCC_CFGR = RCC_CFGR_HPRE_DIV1     /* AHB = SYSCLK / 1 = 168 MHz */
             | RCC_CFGR_PPRE1_DIV4    /* APB1 = AHB / 4 = 42 MHz */
             | RCC_CFGR_PPRE2_DIV2;   /* APB2 = AHB / 2 = 84 MHz */

    /* 6. Switch SYSCLK to PLL */
    RCC_CFGR = (RCC_CFGR & ~3U) | RCC_CFGR_SW_PLL;
    while ((RCC_CFGR & 0x0CU) != RCC_CFGR_SWS_PLL) {}

    /* 7. Update SysTick for 168 MHz (1 ms tick) */
    REG32(0xE000E014) = (SYSCLK_FREQ_HZ / 1000) - 1;  /* SysTick RVR */
    REG32(0xE000E018) = 0;                               /* SysTick CVR */
    REG32(0xE000E010) = 7;                               /* Enable + IRQ + CPU clock */
}

/* ============================================================
 * Pinmux Configuration
 * ============================================================ */

void board_pinmux_init(void)
{
    /* Enable GPIO clocks for ports A, B, C, D, E */
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN
                 | RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN
                 | RCC_AHB1ENR_GPIOEEN;

    /* ---- LEDs: PD12-PD15 as push-pull outputs ---- */
    pin_set_mode(3, 12, MODE_OUTPUT);
    pin_set_mode(3, 13, MODE_OUTPUT);
    pin_set_mode(3, 14, MODE_OUTPUT);
    pin_set_mode(3, 15, MODE_OUTPUT);
    pin_set_speed(3, 12, SPEED_LOW);
    pin_set_speed(3, 13, SPEED_LOW);
    pin_set_speed(3, 14, SPEED_LOW);
    pin_set_speed(3, 15, SPEED_LOW);

    /* ---- User Button: PA0 input with no pull (external pull-down) ---- */
    pin_set_mode(0, 0, MODE_INPUT);
    pin_set_pull(0, 0, PULL_NONE);

    /* ---- USART2: PA2 (TX), PA3 (RX) — AF7 ---- */
    RCC_APB1ENR |= RCC_APB1ENR_USART2EN;
    pin_set_mode(0, 2, MODE_AF);
    pin_set_mode(0, 3, MODE_AF);
    pin_set_af(0, 2, UART_DEBUG_AF);
    pin_set_af(0, 3, UART_DEBUG_AF);
    pin_set_speed(0, 2, SPEED_HIGH);
    pin_set_speed(0, 3, SPEED_HIGH);
    pin_set_pull(0, 3, PULL_UP);  /* RX pull-up for idle high */

    /* ---- SPI1: PA5 (SCK), PA6 (MISO), PA7 (MOSI) — AF5 ---- */
    RCC_APB2ENR |= RCC_APB2ENR_SPI1EN;
    pin_set_mode(0, 5, MODE_AF);
    pin_set_mode(0, 6, MODE_AF);
    pin_set_mode(0, 7, MODE_AF);
    pin_set_af(0, 5, SPI1_AF);
    pin_set_af(0, 6, SPI1_AF);
    pin_set_af(0, 7, SPI1_AF);
    pin_set_speed(0, 5, SPEED_VHIGH);
    pin_set_speed(0, 7, SPEED_VHIGH);
    /* PE3 = accelerometer CS (software GPIO) */
    pin_set_mode(4, 3, MODE_OUTPUT);
    pin_set_speed(4, 3, SPEED_HIGH);
    REG32(GPIO_BASE(4) + 0x18) = (1U << 3);  /* BSRR: set PE3 high (CS deasserted) */

    /* ---- I2C1: PB6 (SCL), PB7 (SDA) — AF4, open-drain ---- */
    RCC_APB1ENR |= RCC_APB1ENR_I2C1EN;
    pin_set_mode(1, 6, MODE_AF);
    pin_set_mode(1, 7, MODE_AF);
    pin_set_af(1, 6, I2C1_AF);
    pin_set_af(1, 7, I2C1_AF);
    pin_set_otype(1, 6, 1);  /* Open-drain */
    pin_set_otype(1, 7, 1);
    pin_set_speed(1, 6, SPEED_HIGH);
    pin_set_speed(1, 7, SPEED_HIGH);
    pin_set_pull(1, 6, PULL_UP);  /* I2C needs pull-ups */
    pin_set_pull(1, 7, PULL_UP);

    /* ---- Audio DAC reset: PD4 output, drive low then high ---- */
    pin_set_mode(3, 4, MODE_OUTPUT);
    REG32(GPIO_BASE(3) + 0x18) = (1U << (4 + 16));  /* BSRR: reset PD4 low */

    /* ---- Timers ---- */
    RCC_APB1ENR |= RCC_APB1ENR_TIM2EN | RCC_APB1ENR_TIM3EN;

    /* ---- DMA ---- */
    RCC_AHB1ENR |= RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMA2EN;

    /* ---- ADC1 ---- */
    RCC_APB2ENR |= RCC_APB2ENR_ADC1EN;

    /* ---- PWR (needed for RTC/backup domain) ---- */
    RCC_APB1ENR |= RCC_APB1ENR_PWREN;
}

/* ============================================================
 * Full Board Init
 * ============================================================ */

void board_init(void)
{
    board_clock_init();
    board_pinmux_init();

    /* Release audio DAC reset after clocks are stable */
    /* Small delay */
    for (volatile int i = 0; i < 10000; i++) {}
    REG32(GPIO_BASE(3) + 0x18) = (1U << 4);  /* BSRR: set PD4 high (release reset) */
}
