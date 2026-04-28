// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file hal_extended.c
 * @brief Extended HAL implementations — Flash, DMA, WDT, RTC, ADC
 *
 * STM32F4 register-level implementations behind EOS_MCU_STM32F4 guards.
 */

#include <eos/hal_extended.h>
#include <string.h>

#if defined(EOS_MCU_STM32F4)

#define REG32(addr) (*(volatile uint32_t *)(addr))

/* ================================================================
 * Flash — STM32F4 Internal Flash (FLASH_CR/SR at 0x40023C00)
 * ================================================================ */
#if EOS_ENABLE_FLASH

#define FLASH_BASE_REG  0x40023C00U
#define FLASH_ACR       REG32(FLASH_BASE_REG + 0x00)
#define FLASH_KEYR      REG32(FLASH_BASE_REG + 0x04)
#define FLASH_SR        REG32(FLASH_BASE_REG + 0x0C)
#define FLASH_CR        REG32(FLASH_BASE_REG + 0x10)

#define FLASH_SR_BSY    (1U << 16)
#define FLASH_CR_PG     (1U << 0)
#define FLASH_CR_SER    (1U << 1)
#define FLASH_CR_STRT   (1U << 16)
#define FLASH_CR_LOCK   (1U << 31)
#define FLASH_KEY1      0x45670123U
#define FLASH_KEY2      0xCDEF89ABU

static void flash_unlock(void) {
    if (FLASH_CR & FLASH_CR_LOCK) {
        FLASH_KEYR = FLASH_KEY1;
        FLASH_KEYR = FLASH_KEY2;
    }
}

static void flash_lock(void) { FLASH_CR |= FLASH_CR_LOCK; }
static void flash_wait(void) { while (FLASH_SR & FLASH_SR_BSY) {} }

int eos_flash_init(const eos_flash_config_t *cfg) { (void)cfg; return 0; }
void eos_flash_deinit(uint8_t id) { (void)id; }

int eos_flash_read(uint8_t id, uint32_t addr, void *buf, size_t len) {
    (void)id;
    memcpy(buf, (const void *)(uintptr_t)addr, len);
    return 0;
}

int eos_flash_write(uint8_t id, uint32_t addr, const void *data, size_t len) {
    (void)id;
    flash_unlock();
    FLASH_CR |= FLASH_CR_PG;
    const uint8_t *src = (const uint8_t *)data;
    volatile uint8_t *dst = (volatile uint8_t *)(uintptr_t)addr;
    for (size_t i = 0; i < len; i++) { dst[i] = src[i]; flash_wait(); }
    FLASH_CR &= ~FLASH_CR_PG;
    flash_lock();
    return 0;
}

int eos_flash_erase_sector(uint8_t id, uint32_t sector_addr) {
    (void)id;
    uint8_t sector = 0;
    if (sector_addr >= 0x08060000U) sector = 7;
    else if (sector_addr >= 0x08040000U) sector = 6;
    else if (sector_addr >= 0x08020000U) sector = 5;
    else if (sector_addr >= 0x08010000U) sector = 4;
    else sector = (uint8_t)((sector_addr - 0x08000000U) / 0x4000U);

    flash_unlock();
    FLASH_CR = FLASH_CR_SER | ((uint32_t)sector << 3);
    FLASH_CR |= FLASH_CR_STRT;
    flash_wait();
    FLASH_CR &= ~FLASH_CR_SER;
    flash_lock();
    return 0;
}

int eos_flash_erase_chip(uint8_t id) { (void)id; return -1; /* Not safe */ }
int eos_flash_get_info(uint8_t id, eos_flash_info_t *info) {
    (void)id;
    if (!info) return -1;
    info->total_size = 0x100000; /* 1MB */
    info->sector_size = 0x4000;
    info->page_size = 1;
    info->sector_count = 12;
    return 0;
}
#endif /* EOS_ENABLE_FLASH */

/* ================================================================
 * DMA — STM32F4 DMA1/DMA2
 * ================================================================ */
#if EOS_ENABLE_DMA

#define DMA1_BASE   0x40026000U
#define DMA2_BASE   0x40026400U
#define DMA_LISR(base)    REG32((base) + 0x00)
#define DMA_HISR(base)    REG32((base) + 0x04)
#define DMA_S_CR(base,s)  REG32((base) + 0x10 + 0x18*(s))
#define DMA_S_NDTR(base,s) REG32((base) + 0x14 + 0x18*(s))
#define DMA_S_PAR(base,s) REG32((base) + 0x18 + 0x18*(s))
#define DMA_S_M0AR(base,s) REG32((base) + 0x1C + 0x18*(s))
#define RCC_AHB1ENR_REG REG32(0x40023830U)

int eos_dma_init(const eos_dma_config_t *cfg) {
    if (!cfg) return -1;
    RCC_AHB1ENR_REG |= (1U << 21) | (1U << 22); /* DMA1 + DMA2 clocks */
    uint32_t base = (cfg->channel < 8) ? DMA1_BASE : DMA2_BASE;
    uint8_t stream = cfg->channel % 8;
    DMA_S_CR(base, stream) = 0; /* Disable */
    uint32_t cr = ((uint32_t)cfg->direction << 6) |
                  ((uint32_t)cfg->data_width << 11) |
                  ((uint32_t)cfg->data_width << 13) |
                  ((uint32_t)cfg->priority << 16);
    if (cfg->circular) cr |= (1U << 8);
    DMA_S_CR(base, stream) = cr;
    return 0;
}

void eos_dma_deinit(uint8_t channel) {
    uint32_t base = (channel < 8) ? DMA1_BASE : DMA2_BASE;
    DMA_S_CR(base, channel % 8) = 0;
}

int eos_dma_start(uint8_t channel, const void *src, void *dst, size_t len) {
    uint32_t base = (channel < 8) ? DMA1_BASE : DMA2_BASE;
    uint8_t stream = channel % 8;
    DMA_S_PAR(base, stream) = (uint32_t)(uintptr_t)src;
    DMA_S_M0AR(base, stream) = (uint32_t)(uintptr_t)dst;
    DMA_S_NDTR(base, stream) = (uint32_t)len;
    DMA_S_CR(base, stream) |= 1; /* Enable */
    return 0;
}

int eos_dma_stop(uint8_t channel) {
    uint32_t base = (channel < 8) ? DMA1_BASE : DMA2_BASE;
    DMA_S_CR(base, channel % 8) &= ~1U;
    return 0;
}

bool eos_dma_busy(uint8_t channel) {
    uint32_t base = (channel < 8) ? DMA1_BASE : DMA2_BASE;
    return (DMA_S_CR(base, channel % 8) & 1U) != 0;
}

size_t eos_dma_remaining(uint8_t channel) {
    uint32_t base = (channel < 8) ? DMA1_BASE : DMA2_BASE;
    return (size_t)DMA_S_NDTR(base, channel % 8);
}

int eos_dma_set_callback(uint8_t channel, eos_dma_callback_t cb, void *ctx) {
    (void)channel; (void)cb; (void)ctx;
    return 0;
}
#endif /* EOS_ENABLE_DMA */

/* ================================================================
 * WDT — STM32F4 Independent Watchdog (IWDG)
 * ================================================================ */
#if EOS_ENABLE_WDT

#define IWDG_KR   REG32(0x40003000U)
#define IWDG_PR   REG32(0x40003004U)
#define IWDG_RLR  REG32(0x40003008U)
#define IWDG_SR   REG32(0x4000300CU)
#define IWDG_KEY_START  0xCCCCU
#define IWDG_KEY_FEED   0xAAAAU
#define IWDG_KEY_ACCESS 0x5555U

int eos_wdt_init(const eos_wdt_config_t *cfg) {
    if (!cfg) return -1;
    IWDG_KR = IWDG_KEY_ACCESS;
    /* Prescaler: /256, LSI ~32kHz → 125Hz */
    IWDG_PR = 6;
    /* Reload: timeout_ms * 125 / 1000 */
    uint32_t reload = (cfg->timeout_ms * 125) / 1000;
    if (reload > 0xFFF) reload = 0xFFF;
    IWDG_RLR = reload;
    while (IWDG_SR) {} /* Wait for update */
    return 0;
}

void eos_wdt_deinit(void) { /* IWDG cannot be stopped once started */ }
int eos_wdt_start(void) { IWDG_KR = IWDG_KEY_START; return 0; }
int eos_wdt_stop(void) { return -1; /* Cannot stop IWDG */ }
void eos_wdt_feed(void) { IWDG_KR = IWDG_KEY_FEED; }
int eos_wdt_set_callback(eos_wdt_callback_t cb, void *ctx) {
    (void)cb; (void)ctx; return -1; /* IWDG resets, no callback */
}
#endif /* EOS_ENABLE_WDT */

/* ================================================================
 * RTC — STM32F4 Real-Time Clock
 * ================================================================ */
#if EOS_ENABLE_RTC

#define RTC_TR    REG32(0x40002800U)
#define RTC_DR    REG32(0x40002804U)
#define RTC_CR    REG32(0x40002808U)
#define RTC_ISR   REG32(0x4000280CU)
#define RTC_WPR   REG32(0x40002824U)
#define RCC_APB1ENR_REG REG32(0x40023840U)
#define RCC_BDCR  REG32(0x40023870U)

static uint8_t bcd_to_bin(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0xF); }
static uint8_t bin_to_bcd(uint8_t bin) { return ((bin / 10) << 4) | (bin % 10); }

int eos_rtc_init(void) {
    RCC_APB1ENR_REG |= (1U << 28); /* PWR clock */
    REG32(0x40007000U) |= (1U << 8); /* PWR_CR DBP */
    RCC_BDCR |= (1U << 0) | (1U << 8) | (1U << 15); /* LSE on, RTC sel=LSE, RTC enable */
    return 0;
}

void eos_rtc_deinit(void) {}

int eos_rtc_get_time(eos_rtc_time_t *time) {
    if (!time) return -1;
    uint32_t tr = RTC_TR;
    uint32_t dr = RTC_DR;
    time->hour = bcd_to_bin((tr >> 16) & 0x3F);
    time->minute = bcd_to_bin((tr >> 8) & 0x7F);
    time->second = bcd_to_bin(tr & 0x7F);
    time->year = 2000 + bcd_to_bin((dr >> 16) & 0xFF);
    time->month = bcd_to_bin((dr >> 8) & 0x1F);
    time->day = bcd_to_bin(dr & 0x3F);
    time->weekday = (dr >> 13) & 0x7;
    return 0;
}

int eos_rtc_set_time(const eos_rtc_time_t *time) {
    if (!time) return -1;
    RTC_WPR = 0xCA; RTC_WPR = 0x53; /* Unlock */
    RTC_ISR |= (1U << 7); /* INIT mode */
    while (!(RTC_ISR & (1U << 6))) {} /* Wait INITF */
    RTC_TR = ((uint32_t)bin_to_bcd(time->hour) << 16) |
             ((uint32_t)bin_to_bcd(time->minute) << 8) |
             bin_to_bcd(time->second);
    RTC_DR = ((uint32_t)bin_to_bcd(time->year - 2000) << 16) |
             ((uint32_t)time->weekday << 13) |
             ((uint32_t)bin_to_bcd(time->month) << 8) |
             bin_to_bcd(time->day);
    RTC_ISR &= ~(1U << 7); /* Exit INIT */
    RTC_WPR = 0xFF; /* Lock */
    return 0;
}

int eos_rtc_set_alarm(const eos_rtc_time_t *a, eos_rtc_alarm_callback_t cb, void *ctx) {
    (void)a; (void)cb; (void)ctx; return 0;
}
int eos_rtc_cancel_alarm(void) { return 0; }
uint32_t eos_rtc_get_unix_timestamp(void) {
    eos_rtc_time_t t;
    eos_rtc_get_time(&t);
    /* Simplified — no leap year handling */
    return (uint32_t)((t.year - 1970) * 31536000UL + t.month * 2592000UL +
           t.day * 86400UL + t.hour * 3600UL + t.minute * 60UL + t.second);
}
#endif /* EOS_ENABLE_RTC */

/* ================================================================
 * ADC — STM32F4 ADC1
 * ================================================================ */
#if EOS_ENABLE_ADC

#define ADC1_BASE   0x40012000U
#define ADC_SR(base)    REG32((base) + 0x00)
#define ADC_CR1(base)   REG32((base) + 0x04)
#define ADC_CR2(base)   REG32((base) + 0x08)
#define ADC_SQR3(base)  REG32((base) + 0x34)
#define ADC_DR_R(base)  REG32((base) + 0x4C)
#define ADC_SR_EOC      (1U << 1)
#define ADC_CR2_ADON    (1U << 0)
#define ADC_CR2_SWSTART (1U << 30)

int eos_adc_init(const eos_adc_config_t *cfg) {
    if (!cfg) return -1;
    REG32(0x40023844U) |= (1U << 8); /* ADC1 clock */
    uint32_t res = 0;
    if (cfg->resolution_bits == 10) res = 1;
    else if (cfg->resolution_bits == 8) res = 2;
    ADC_CR1(ADC1_BASE) = (res << 24);
    ADC_CR2(ADC1_BASE) = ADC_CR2_ADON;
    return 0;
}

void eos_adc_deinit(uint8_t channel) {
    (void)channel;
    ADC_CR2(ADC1_BASE) = 0;
}

uint32_t eos_adc_read(uint8_t channel) {
    ADC_SQR3(ADC1_BASE) = channel & 0x1F;
    ADC_CR2(ADC1_BASE) |= ADC_CR2_SWSTART;
    while (!(ADC_SR(ADC1_BASE) & ADC_SR_EOC)) {}
    return ADC_DR_R(ADC1_BASE) & 0xFFF;
}

uint32_t eos_adc_read_mv(uint8_t channel) {
    uint32_t raw = eos_adc_read(channel);
    return (raw * 3300) / 4095; /* 3.3V reference, 12-bit */
}
#endif /* EOS_ENABLE_ADC */

#endif /* EOS_MCU_STM32F4 */
