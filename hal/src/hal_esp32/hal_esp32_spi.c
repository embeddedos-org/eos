// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file hal_esp32_spi.c
 * @brief ESP32 SPI master (SPI2 "HSPI" and SPI3 "VSPI") for the HAL backend
 *
 * Port 2 is SPI2 and port 3 is SPI3; their pins are EOS_BOARD_SPIn_*_GPIO
 * (board_esp32.h), routed through the GPIO matrix. Full duplex, 8-bit words,
 * modes 0-3, MSB first, polled. A transfer moves through the peripheral's
 * 64-byte buffer (W0..W15) in chunks; chip select is a GPIO driven low for
 * the whole transfer, so a transfer of any length is one CS assertion.
 * cs_pin = EOS_ESP32_SPI_NO_CS leaves chip select to the caller.
 *
 * Register sequences follow ESP-IDF v5.4 hal/esp32/include/hal/spi_ll.h
 * (master init, mode, clock calculation, user transaction); addresses and
 * bit positions are from soc/esp32/register/soc/spi_reg.h. Nothing from
 * ESP-IDF is linked.
 */

#include <string.h>
#include "eos/hal.h"
#include "board_esp32.h"
#include "hal_esp32_internal.h"

#define REG32 ESP32_REG32

#define SPI_BASE(p)        ((p) == 2U ? 0x3FF64000U : 0x3FF65000U)
#define SPI_CMD(p)         (SPI_BASE(p) + 0x00U)
#define SPI_CTRL(p)        (SPI_BASE(p) + 0x08U)
#define SPI_CTRL2(p)       (SPI_BASE(p) + 0x14U)
#define SPI_CLOCK(p)       (SPI_BASE(p) + 0x18U)
#define SPI_USER(p)        (SPI_BASE(p) + 0x1CU)
#define SPI_USER1(p)       (SPI_BASE(p) + 0x20U)
#define SPI_USER2(p)       (SPI_BASE(p) + 0x24U)
#define SPI_MOSI_DLEN(p)   (SPI_BASE(p) + 0x28U)
#define SPI_MISO_DLEN(p)   (SPI_BASE(p) + 0x2CU)
#define SPI_PIN(p)         (SPI_BASE(p) + 0x34U)
#define SPI_SLAVE(p)       (SPI_BASE(p) + 0x38U)
#define SPI_W(p, n)        (SPI_BASE(p) + 0x80U + 4U * (n))

#define CMD_USR            (1U << 18)
#define USER_DOUTDIN       (1U << 0)
#define USER_CK_OUT_EDGE   (1U << 7)
#define USER_USR_MOSI      (1U << 27)
#define USER_USR_MISO      (1U << 28)
#define PIN_CS_DIS_ALL     0x7U          /* hardware CS0..CS2 off */
#define PIN_CK_IDLE_EDGE   (1U << 29)
#define CLOCK_EQU_SYSCLK   (1U << 31)

#define DPORT_SPI2_CLK_EN  (1U << 6)
#define DPORT_SPI3_CLK_EN  (1U << 16)

/* gpio_sig_map.h */
#define HSPICLK_OUT_IDX    8U
#define HSPIQ_IN_IDX       9U
#define HSPID_OUT_IDX      10U
#define VSPICLK_OUT_IDX    63U
#define VSPIQ_IN_IDX       64U
#define VSPID_OUT_IDX      65U

#define SPI_BUF_BYTES      64U

typedef struct {
    bool     ready;
    uint16_t cs;
    uint16_t sclk, mosi, miso;
    uint32_t hz;
} spi_port_t;

static spi_port_t s_spi[4];

static bool port_ok(uint8_t port) { return port == 2U || port == 3U; }

static uint32_t sig_clk(uint8_t p)  { return p == 2U ? HSPICLK_OUT_IDX : VSPICLK_OUT_IDX; }
static uint32_t sig_mosi(uint8_t p) { return p == 2U ? HSPID_OUT_IDX : VSPID_OUT_IDX; }
static uint32_t sig_miso(uint8_t p) { return p == 2U ? HSPIQ_IN_IDX : VSPIQ_IN_IDX; }

/*
 * SPI clock = APB / (pre * n), with pre 1..8192 and n 2..64, or APB itself.
 * The requested clock is the device's maximum, so this takes the fastest
 * divider that does not exceed it (ESP-IDF's spi_ll_master_cal_clock()
 * takes the nearest, which can be faster: 26 MHz asked gave 26.67 MHz).
 * Ties go to the larger n, for the finer duty cycle; duty is 50%. Returns
 * the CLOCK register value and the frequency it gives through *eff, or 0
 * (eff 0) below the slowest divider, APB / 524288.
 */
static uint32_t spi_clock_reg(uint32_t hz, uint32_t *eff)
{
    const uint32_t apb = ESP32_APB_HZ;
    if (hz >= apb) {
        *eff = apb;
        return CLOCK_EQU_SYSCLK;
    }
    uint32_t best_n = 0, best_pre = 0, best_f = 0;
    for (uint32_t n = 2; n <= 64U; n++) {
        uint32_t pre = (apb / n + hz - 1U) / hz;       /* round up: f <= hz */
        if (pre == 0U) pre = 1U;
        if (pre > 8192U) pre = 8192U;
        uint32_t f = apb / (pre * n);
        if (f <= hz && f >= best_f) {
            best_f = f;
            best_n = n;
            best_pre = pre;
        }
    }
    if (best_n == 0U) {
        *eff = 0;
        return 0;
    }
    uint32_t h = best_n / 2U;                          /* 50% duty */
    *eff = best_f;
    return ((best_pre - 1U) << 18) | ((best_n - 1U) << 12) | ((h - 1U) << 6) | (best_n - 1U);
}

int esp32_spi_init(const eos_spi_config_t *cfg)
{
    if (cfg == NULL || !port_ok(cfg->port)) return -1;
    if (cfg->bits_per_word != 0U && cfg->bits_per_word != 8U) return -1;
    if (cfg->mode > EOS_SPI_MODE_3 || cfg->clock_hz == 0U) return -1;
    /* Through the GPIO matrix the input path adds delay the master's
     * sampling edge does not allow for above APB/3 (ESP-IDF's
     * SPI_MASTER_FREQ_26M, its limit for full duplex through the matrix). */
    if (cfg->clock_hz > ESP32_SPI_MAX_HZ) return -1;
    uint8_t p = cfg->port;
    spi_port_t *s = &s_spi[p];
    /* A failed (re)initialisation leaves the port unusable, not half set up
     * with the previous settings. */
    if (s->ready) esp32_spi_deinit(p);
    s->sclk = p == 2U ? EOS_BOARD_SPI2_SCLK_GPIO : EOS_BOARD_SPI3_SCLK_GPIO;
    s->mosi = p == 2U ? EOS_BOARD_SPI2_MOSI_GPIO : EOS_BOARD_SPI3_MOSI_GPIO;
    s->miso = p == 2U ? EOS_BOARD_SPI2_MISO_GPIO : EOS_BOARD_SPI3_MISO_GPIO;
    s->cs = cfg->cs_pin;
    if (s->cs != EOS_ESP32_SPI_NO_CS && (!esp32_pin_usable(s->cs) || s->cs >= 34U)) return -1;

    esp32_periph_enable(p == 2U ? DPORT_SPI2_CLK_EN : DPORT_SPI3_CLK_EN);

    REG32(SPI_SLAVE(p)) = 0;                      /* master, interrupts off */
    REG32(SPI_CTRL2(p)) = 0;                      /* no setup/hold/delay */
    REG32(SPI_CTRL(p)) = 0;                       /* MSB first, one data line */
    REG32(SPI_USER1(p)) = 0;
    REG32(SPI_USER2(p)) = 0;
    uint32_t user = USER_DOUTDIN | USER_USR_MOSI | USER_USR_MISO;
    uint32_t pin = PIN_CS_DIS_ALL;
    /* spi_ll_master_set_mode(): CPOL is the idle level, and the output edge
     * flips with CPHA ^ CPOL. */
    if (cfg->mode == EOS_SPI_MODE_1 || cfg->mode == EOS_SPI_MODE_2) user |= USER_CK_OUT_EDGE;
    if (cfg->mode == EOS_SPI_MODE_2 || cfg->mode == EOS_SPI_MODE_3) pin |= PIN_CK_IDLE_EDGE;
    REG32(SPI_USER(p)) = user;
    REG32(SPI_PIN(p)) = pin;
    uint32_t clock = spi_clock_reg(cfg->clock_hz, &s->hz);
    if (s->hz == 0U) return -1;
    REG32(SPI_CLOCK(p)) = clock;

    if (esp32_pin_route_output(s->sclk, sig_clk(p)) != 0 ||
        esp32_pin_route_output(s->mosi, sig_mosi(p)) != 0 ||
        esp32_pin_route_input(s->miso, sig_miso(p)) != 0) {
        return -1;
    }
    if (s->cs != EOS_ESP32_SPI_NO_CS) {
        /* Latch high before the driver turns on: no CS glitch. */
        esp32_gpio_set(s->cs, true);
        eos_gpio_config_t g = { .pin = s->cs, .mode = EOS_GPIO_OUTPUT,
                                .pull = EOS_GPIO_PULL_NONE };
        if (esp32_gpio_configure(&g) != 0) return -1;
    }
    s->ready = true;
    return 0;
}

void esp32_spi_deinit(uint8_t port)
{
    if (!port_ok(port) || !s_spi[port].ready) return;
    while (REG32(SPI_CMD(port)) & CMD_USR) {
    }
    s_spi[port].ready = false;
}

int eos_esp32_spi_set_loopback(uint8_t port, bool enable)
{
    if (!port_ok(port) || !s_spi[port].ready) return -1;
    const spi_port_t *s = &s_spi[port];
    /* MISO reads the MOSI pad instead of its own: the pad's input stays
     * enabled while the matrix drives it. */
    return esp32_pin_route_input(enable ? s->mosi : s->miso, sig_miso(port));
}

uint32_t eos_esp32_spi_get_hz(uint8_t port)
{
    return port_ok(port) && s_spi[port].ready ? s_spi[port].hz : 0U;
}

/* A chunk is at most 64 bytes, about 2 ms at the slowest clock this driver
 * accepts above 1 kHz; a transaction that has not finished in 100 ms will
 * not. */
#define SPI_TIMEOUT_MS 100U

static int spi_chunk(uint8_t p, const uint8_t *tx, uint8_t *rx, size_t n)
{
    uint32_t words[SPI_BUF_BYTES / 4U];
    memset(words, 0xFF, sizeof(words));            /* read-only: clock out 0xFF */
    if (tx != NULL) memcpy(words, tx, n);
    uint32_t bits = (uint32_t)n * 8U;
    REG32(SPI_MOSI_DLEN(p)) = bits - 1U;
    REG32(SPI_MISO_DLEN(p)) = bits - 1U;
    for (uint32_t i = 0; i < (n + 3U) / 4U; i++) REG32(SPI_W(p, i)) = words[i];
    REG32(SPI_CMD(p)) = CMD_USR;
    uint32_t t0 = eos_esp32_ccount();
    while (REG32(SPI_CMD(p)) & CMD_USR) {
        if (eos_esp32_ccount() - t0 > SPI_TIMEOUT_MS * (ESP32_CPU_HZ / 1000U)) return -1;
    }
    if (rx != NULL) {
        for (uint32_t i = 0; i < (n + 3U) / 4U; i++) words[i] = REG32(SPI_W(p, i));
        memcpy(rx, words, n);
    }
    return 0;
}

int esp32_spi_transfer(uint8_t port, const uint8_t *tx, uint8_t *rx, size_t len)
{
    if (!port_ok(port) || !s_spi[port].ready) return -1;
    if (tx == NULL && rx == NULL) return -1;
    if (len == 0U) return 0;
    if (len > INT32_MAX) return -1;
    const spi_port_t *s = &s_spi[port];
    if (s->cs != EOS_ESP32_SPI_NO_CS) esp32_gpio_set(s->cs, false);
    size_t done = 0;
    int rc = 0;
    while (done < len) {
        size_t n = len - done < SPI_BUF_BYTES ? len - done : SPI_BUF_BYTES;
        rc = spi_chunk(port, tx != NULL ? tx + done : NULL, rx != NULL ? rx + done : NULL, n);
        if (rc != 0) break;
        done += n;
    }
    if (s->cs != EOS_ESP32_SPI_NO_CS) esp32_gpio_set(s->cs, true);
    return rc == 0 ? (int)len : -1;
}
