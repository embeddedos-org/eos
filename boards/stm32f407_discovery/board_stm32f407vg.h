// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file board_stm32f407vg.h
 * @brief STM32F407VG Discovery board pinmux, clock tree, and peripheral config
 *
 * Pin assignments match the STM32F4-Discovery board (STM32F407VGT6):
 *   - 4 user LEDs: PD12 (green), PD13 (orange), PD14 (red), PD15 (blue)
 *   - 1 user button: PA0 (active high)
 *   - USART2: PA2 (TX), PA3 (RX) — ST-Link VCP
 *   - SPI1: PA5 (SCK), PA6 (MISO), PA7 (MOSI), PE3 (CS for accelerometer)
 *   - I2C1: PB6 (SCL), PB7 (SDA) — audio codec CS43L22
 *   - USB OTG FS: PA11 (DM), PA12 (DP), PA9 (VBUS)
 *   - Audio: PC7 (I2S3_MCK), PC10 (I2S3_SCK), PC12 (I2S3_SD)
 */

#ifndef BOARD_STM32F407VG_H
#define BOARD_STM32F407VG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Pin Encoding: (port << 8) | pin_number
 *   Port A = 0, B = 1, C = 2, D = 3, E = 4
 * ============================================================ */

#define PIN(port, num)  (((port) << 8) | (num))
#define PIN_PORT(pin)   (((pin) >> 8) & 0xF)
#define PIN_NUM(pin)    ((pin) & 0xFF)

/* ---- User LEDs (PD12-PD15) ---- */
#define LED_GREEN       PIN(3, 12)  /* PD12 */
#define LED_ORANGE      PIN(3, 13)  /* PD13 */
#define LED_RED         PIN(3, 14)  /* PD14 */
#define LED_BLUE        PIN(3, 15)  /* PD15 */

/* ---- User Button ---- */
#define BTN_USER        PIN(0, 0)   /* PA0, active high */

/* ---- USART2 (ST-Link VCP / debug console) ---- */
#define UART_DEBUG_TX   PIN(0, 2)   /* PA2, AF7 */
#define UART_DEBUG_RX   PIN(0, 3)   /* PA3, AF7 */
#define UART_DEBUG_AF   7           /* AF7 = USART1-3 */
#define UART_DEBUG_PORT 1           /* USART2 index (0-based: USART1=0, USART2=1) */
#define UART_DEBUG_BAUD 115200

/* ---- SPI1 (LIS3DSH accelerometer on Discovery) ---- */
#define SPI1_SCK        PIN(0, 5)   /* PA5, AF5 */
#define SPI1_MISO       PIN(0, 6)   /* PA6, AF5 */
#define SPI1_MOSI       PIN(0, 7)   /* PA7, AF5 */
#define SPI1_CS_ACCEL   PIN(4, 3)   /* PE3, GPIO output (software CS) */
#define SPI1_AF         5           /* AF5 = SPI1/SPI2 */

/* ---- I2C1 (CS43L22 audio DAC) ---- */
#define I2C1_SCL        PIN(1, 6)   /* PB6, AF4 */
#define I2C1_SDA        PIN(1, 7)   /* PB7, AF4 */
#define I2C1_AF         4           /* AF4 = I2C1-3 */
#define I2C_AUDIO_ADDR  0x4A        /* CS43L22 I2C address */

/* ---- USB OTG FS ---- */
#define USB_DM          PIN(0, 11)  /* PA11, AF10 */
#define USB_DP          PIN(0, 12)  /* PA12, AF10 */
#define USB_VBUS        PIN(0, 9)   /* PA9, input */
#define USB_AF          10          /* AF10 = OTG_FS */

/* ---- Audio I2S3 ---- */
#define I2S3_MCK        PIN(2, 7)   /* PC7, AF6 */
#define I2S3_SCK        PIN(2, 10)  /* PC10, AF6 */
#define I2S3_SD         PIN(2, 12)  /* PC12, AF6 */
#define I2S_AF          6           /* AF6 = SPI3/I2S3 */
#define AUDIO_RESET     PIN(3, 4)   /* PD4, GPIO output (CS43L22 ~RESET) */

/* ============================================================
 * Clock Tree Configuration
 *
 * HSE = 8 MHz (on-board crystal)
 * PLL: M=8, N=336, P=2, Q=7
 *   PLLCLK = HSE / M * N / P = 8/8 * 336/2 = 168 MHz
 *   USB_CLK = HSE / M * N / Q = 8/8 * 336/7 = 48 MHz
 *
 * AHB  = SYSCLK / 1 = 168 MHz
 * APB1 = AHB / 4     = 42 MHz  (max 42 MHz)
 * APB2 = AHB / 2     = 84 MHz  (max 84 MHz)
 *
 * Flash latency: 5 wait states (for 168 MHz, 3.3V)
 * ============================================================ */

#define HSE_FREQ_HZ         8000000U
#define SYSCLK_FREQ_HZ      168000000U
#define AHB_FREQ_HZ         168000000U
#define APB1_FREQ_HZ        42000000U
#define APB2_FREQ_HZ        84000000U

/* PLL parameters */
#define PLL_M               8
#define PLL_N               336
#define PLL_P               2       /* PLLP divider (2,4,6,8) → encoded as 0,1,2,3 */
#define PLL_Q               7       /* USB OTG FS = 48 MHz */

/* Flash latency for 168 MHz at 3.3V */
#define FLASH_LATENCY       5

/* Timer clock multipliers (APBx prescaler > 1 → timer clock = 2x APBx) */
#define TIM_APB1_FREQ_HZ    (APB1_FREQ_HZ * 2)  /* 84 MHz */
#define TIM_APB2_FREQ_HZ    (APB2_FREQ_HZ * 2)  /* 168 MHz */

/* ============================================================
 * RCC Register Addresses
 * ============================================================ */

#define RCC_BASE            0x40023800U
#define RCC_CR              (*(volatile uint32_t *)(RCC_BASE + 0x00))
#define RCC_PLLCFGR         (*(volatile uint32_t *)(RCC_BASE + 0x04))
#define RCC_CFGR            (*(volatile uint32_t *)(RCC_BASE + 0x08))
#define RCC_AHB1ENR         (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_AHB2ENR         (*(volatile uint32_t *)(RCC_BASE + 0x34))
#define RCC_APB1ENR         (*(volatile uint32_t *)(RCC_BASE + 0x40))
#define RCC_APB2ENR         (*(volatile uint32_t *)(RCC_BASE + 0x44))

/* Flash ACR */
#define FLASH_ACR           (*(volatile uint32_t *)0x40023C00U)

/* RCC_CR bits */
#define RCC_CR_HSEON        (1U << 16)
#define RCC_CR_HSERDY       (1U << 17)
#define RCC_CR_PLLON        (1U << 24)
#define RCC_CR_PLLRDY       (1U << 25)

/* RCC_CFGR bits */
#define RCC_CFGR_SW_PLL     (2U << 0)
#define RCC_CFGR_SWS_PLL    (2U << 2)
#define RCC_CFGR_HPRE_DIV1  (0U << 4)
#define RCC_CFGR_PPRE1_DIV4 (5U << 10)
#define RCC_CFGR_PPRE2_DIV2 (4U << 13)

/* RCC_PLLCFGR bits */
#define RCC_PLLCFGR_PLLSRC_HSE (1U << 22)

/* RCC_AHB1ENR: GPIO clocks */
#define RCC_AHB1ENR_GPIOAEN (1U << 0)
#define RCC_AHB1ENR_GPIOBEN (1U << 1)
#define RCC_AHB1ENR_GPIOCEN (1U << 2)
#define RCC_AHB1ENR_GPIODEN (1U << 3)
#define RCC_AHB1ENR_GPIOEEN (1U << 4)
#define RCC_AHB1ENR_DMA1EN  (1U << 21)
#define RCC_AHB1ENR_DMA2EN  (1U << 22)

/* RCC_APB1ENR: peripheral clocks */
#define RCC_APB1ENR_TIM2EN  (1U << 0)
#define RCC_APB1ENR_TIM3EN  (1U << 1)
#define RCC_APB1ENR_TIM4EN  (1U << 2)
#define RCC_APB1ENR_USART2EN (1U << 17)
#define RCC_APB1ENR_I2C1EN  (1U << 21)
#define RCC_APB1ENR_PWREN   (1U << 28)

/* RCC_APB2ENR: peripheral clocks */
#define RCC_APB2ENR_USART1EN (1U << 4)
#define RCC_APB2ENR_ADC1EN  (1U << 8)
#define RCC_APB2ENR_SPI1EN  (1U << 12)
#define RCC_APB2ENR_SYSCFGEN (1U << 14)

/* ============================================================
 * Board Init API
 * ============================================================ */

/**
 * @brief Configure the clock tree (HSE → PLL → 168 MHz SYSCLK).
 * Must be called first, before any peripheral init.
 */
void board_clock_init(void);

/**
 * @brief Configure all pin multiplexing for Discovery board peripherals.
 * Sets up GPIO AF for UART, SPI, I2C, LEDs, button.
 */
void board_pinmux_init(void);

/**
 * @brief Full board initialization (clock + pinmux + peripherals).
 * Call this from main() before using any HAL functions.
 */
void board_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_STM32F407VG_H */
