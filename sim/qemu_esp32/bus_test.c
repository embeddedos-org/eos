// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
/**
 * @file bus_test.c
 * @brief On-board check of the ESP32 SPI master driver.
 *
 * Every check is a loopback inside the chip, so a bare DevKit needs no wire:
 * the GPIO matrix feeds each SPI port's MISO from its own MOSI pad, and a
 * full-duplex transfer must then read back exactly what it sent. Each
 * transfer is 200 bytes, so it crosses the peripheral's 64-byte buffer
 * three times under one chip select. A loopback cannot show the clock
 * phase (the master samples its own output on its own edge), so CPHA is not
 * checked; CPOL is, as the idle level of the SCLK pad.
 *
 * Prints one CHECK line per item and "BUS TEST PASSED" or "BUS TEST FAILED".
 * Needs a board: QEMU's esp32 machine does not model SPI2/SPI3 through the
 * GPIO matrix.
 */
#include <stdbool.h>
#include <stdint.h>
#include "eos/hal.h"
#include "board_esp32.h"

#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define GPIO_IN_REG 0x3FF4403CU
#define SPI_CS_GPIO 27U
#define XFER 200U

static int s_failures;

static void put(const char *s) { eos_esp32_console_puts(s); }
static void put_u(uint32_t v) { eos_esp32_console_put_u32(v); }

static void check(bool ok)
{
    put(ok ? "  PASS\n" : "  FAIL\n");
    if (!ok) s_failures++;
}

static uint8_t s_tx[XFER], s_rx[XFER];

static void test_spi_loopback(uint8_t port)
{
    static const uint32_t clocks[] = { 100000U, 1000000U, 10000000U, 20000000U, 26666667U };
    uint32_t good = 0, total = 0, cs_bad = 0, cpol_bad = 0;
    uint32_t sclk = port == 2U ? EOS_BOARD_SPI2_SCLK_GPIO : EOS_BOARD_SPI3_SCLK_GPIO;
    put("CHECK spi");
    put_u(port);
    put(" loopback, modes 0-3 x 100 kHz/1/10/20/26.67 MHz, 200-byte transfers ->");
    for (uint32_t m = 0; m < 4U; m++) {
        for (uint32_t c = 0; c < sizeof(clocks) / sizeof(clocks[0]); c++) {
            eos_spi_config_t cfg = { .port = port, .clock_hz = clocks[c],
                                     .mode = (eos_spi_mode_t)m, .bits_per_word = 8,
                                     .cs_pin = SPI_CS_GPIO };
            total++;
            if (eos_spi_init(&cfg) != 0 || eos_esp32_spi_set_loopback(port, true) != 0) continue;
            for (uint32_t i = 0; i < XFER; i++) {
                s_tx[i] = (uint8_t)(i * 37U + m * 11U + c * 5U + port);
                s_rx[i] = 0;
            }
            int n = eos_spi_transfer(port, s_tx, s_rx, XFER);
            bool same = n == (int)XFER;
            for (uint32_t i = 0; same && i < XFER; i++) same = s_rx[i] == s_tx[i];
            if (((REG32(GPIO_IN_REG) >> SPI_CS_GPIO) & 1U) == 0U) cs_bad++;   /* CS left low */
            if (((REG32(GPIO_IN_REG) >> sclk) & 1U) != (m >= 2U ? 1U : 0U)) cpol_bad++;
            if (same) good++;
            eos_spi_deinit(port);
        }
    }
    put(" ");
    put_u(good);
    put("/");
    put_u(total);
    put(", CS idle high after each: ");
    put(cs_bad == 0U ? "yes" : "no");
    put(", SCLK idles at CPOL: ");
    put(cpol_bad == 0U ? "yes" : "no");
    check(good == total && cs_bad == 0U && cpol_bad == 0U);
}

static void test_spi_write_read(void)
{
    /* eos_spi_read() clocks out 0xFF; looped back, it must read 0xFF. */
    put("CHECK spi2 write-only then read-only (read returns the 0xFF it clocks out)");
    eos_spi_config_t cfg = { .port = 2, .clock_hz = 4000000U, .mode = EOS_SPI_MODE_0,
                             .bits_per_word = 8, .cs_pin = EOS_ESP32_SPI_NO_CS };
    bool ok = eos_spi_init(&cfg) == 0 && eos_esp32_spi_set_loopback(2, true) == 0;
    ok = ok && eos_spi_write(2, s_tx, 70) == 70;
    for (uint32_t i = 0; i < 70U; i++) s_rx[i] = 0;
    ok = ok && eos_spi_read(2, s_rx, 70) == 70;
    for (uint32_t i = 0; ok && i < 70U; i++) ok = s_rx[i] == 0xFFU;
    eos_spi_deinit(2);
    check(ok);
}

static void test_spi_clock(void)
{
    /* The fastest divider not above the request: a device's maximum. 100 Hz
     * is below the slowest divider (152 Hz) and is refused (0). */
    static const uint32_t ask[] = { 1000000U, 26000000U, 26666667U, 3000000U, 200U, 100U };
    static const uint32_t want[] = { 1000000U, 20000000U, 26666666U, 2962962U, 200U, 0U };
    put("CHECK spi clock never above the request:");
    bool ok = true;
    for (uint32_t i = 0; i < sizeof(ask) / sizeof(ask[0]); i++) {
        eos_spi_config_t cfg = { .port = 3, .clock_hz = ask[i], .mode = EOS_SPI_MODE_0,
                                 .bits_per_word = 8, .cs_pin = EOS_ESP32_SPI_NO_CS };
        uint32_t got = eos_spi_init(&cfg) == 0 ? eos_esp32_spi_get_hz(3) : 0U;
        put(" ");
        put_u(ask[i]);
        put("->");
        put_u(got);
        if (got != want[i]) ok = false;
    }
    eos_spi_deinit(3);
    check(ok);
}

static void test_spi_rejects(void)
{
    put("CHECK spi rejects port 1, 16-bit words, 0 Hz, 27 MHz and a flash pin as CS");
    eos_spi_config_t ok_cfg = { .port = 2, .clock_hz = 1000000U, .mode = EOS_SPI_MODE_0,
                                .bits_per_word = 8, .cs_pin = EOS_ESP32_SPI_NO_CS };
    eos_spi_config_t c;
    int bad = 0;
    c = ok_cfg; c.port = 1;            bad += eos_spi_init(&c) == 0;
    c = ok_cfg; c.bits_per_word = 16;  bad += eos_spi_init(&c) == 0;
    c = ok_cfg; c.clock_hz = 0;        bad += eos_spi_init(&c) == 0;
    c = ok_cfg; c.clock_hz = 27000000U; bad += eos_spi_init(&c) == 0;
    c = ok_cfg; c.cs_pin = 7;          bad += eos_spi_init(&c) == 0;
    bad += eos_spi_transfer(1, s_tx, s_rx, 4) >= 0;
    check(bad == 0);
}

int main(void)
{
    eos_hal_init();
    put("bus_test: starting\n");
    test_spi_loopback(2);
    test_spi_loopback(3);
    test_spi_write_read();
    test_spi_clock();
    test_spi_rejects();
    put(s_failures == 0 ? "BUS TEST PASSED\n" : "BUS TEST FAILED\n");
    for (;;) {
        __asm volatile ("waiti 0");
    }
}
