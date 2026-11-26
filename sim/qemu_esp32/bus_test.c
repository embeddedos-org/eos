// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
/**
 * @file bus_test.c
 * @brief On-board check of the ESP32 SPI and I2C drivers.
 *
 * Every check is a loopback inside the chip, so a bare DevKit needs no wire:
 * the GPIO matrix feeds each SPI port's MISO from its own MOSI pad, and a
 * full-duplex transfer must then read back exactly what it sent. Each
 * transfer is 200 bytes, so it crosses the peripheral's 64-byte buffer
 * three times under one chip select. A loopback cannot show the clock
 * phase (the master samples its own output on its own edge), so CPHA is not
 * checked; CPOL is, as the idle level of the SCLK pad.
 *
 * I2C0 is the master and I2C1 a slave at 0x28. An I2C bus is a wired AND,
 * which two pads cannot form inside the chip, so with no wires the slave
 * only listens to the master's pads (eos_esp32_i2c_slave_listen()): it must
 * match the address the master sends, ignore another, and the master must
 * report the NACK and keep working. With GPIO21-GPIO33 and GPIO22-GPIO32
 * joined (detected), writes of up to 32 bytes (the slave's FIFO), reads of
 * the slave's queued bytes and a NACK from an absent address run too;
 * without them those print NOT RUN, not PASS.
 *
 * Prints one CHECK line per item and "BUS TEST PASSED" or "BUS TEST FAILED".
 * Needs a board: QEMU's esp32 machine does not model SPI2/SPI3 through the
 * GPIO matrix.
 */
#include <stdbool.h>
#include <stdint.h>
#include "eos/hal.h"
#include "eos/hal_extended.h"
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

/* ---- I2C: master I2C0, slave I2C1 at 0x28 ---------------------------- */

#define I2C_SLAVE_ADDR 0x28U

static bool i2c_pair(uint32_t hz)
{
    eos_i2c_config_t m = { .port = 0, .clock_hz = hz, .own_addr = 0 };
    return eos_i2c_init(&m) == 0 && eos_esp32_i2c_slave_init(1, I2C_SLAVE_ADDR, hz) == 0;
}

/* Wired bus: GPIO21-GPIO33 and GPIO22-GPIO32 joined. Detected by driving one
 * end through GPIO and reading the other with its pull-up on. */
static bool i2c_wired(void)
{
    const uint16_t ends[2][2] = { { EOS_BOARD_I2C0_SDA_GPIO, EOS_BOARD_I2C1_SDA_GPIO },
                                  { EOS_BOARD_I2C0_SCL_GPIO, EOS_BOARD_I2C1_SCL_GPIO } };
    bool wired = true;
    for (int i = 0; i < 2; i++) {
        eos_gpio_config_t in = { .pin = ends[i][1], .mode = EOS_GPIO_INPUT, .pull = EOS_GPIO_PULL_UP };
        eos_gpio_config_t out = { .pin = ends[i][0], .mode = EOS_GPIO_OUTPUT, .pull = EOS_GPIO_PULL_NONE };
        eos_gpio_write(ends[i][0], false);
        eos_gpio_init(&in);
        eos_gpio_init(&out);
        uint32_t t0 = eos_esp32_ccount();
        while (eos_esp32_ccount() - t0 < 24000U) {
        }
        wired = wired && !eos_gpio_read(ends[i][1]);
        eos_gpio_write(ends[i][0], true);
        t0 = eos_esp32_ccount();
        while (eos_esp32_ccount() - t0 < 24000U) {
        }
        wired = wired && eos_gpio_read(ends[i][1]);
        out.mode = EOS_GPIO_INPUT;
        eos_gpio_init(&out);
    }
    return wired;
}

static void test_i2c_address_no_wire(void)
{
    /* The slave hears the master's pads but cannot answer: the master must
     * report the NACK, and the slave must have matched the address the
     * master sent. Then the master must still work: a second attempt
     * behaves the same. */
    put("CHECK i2c0 address reaches listening i2c1 slave 0x28 (no wire), master reports NACK");
    bool ok = i2c_pair(100000U) && eos_esp32_i2c_slave_listen(1, 0) == 0;
    s_tx[0] = 0x11;
    bool nack = ok && eos_i2c_write(0, I2C_SLAVE_ADDR, s_tx, 1) < 0;
    bool heard = eos_esp32_i2c_slave_addressed(1);
    ok = ok && eos_esp32_i2c_slave_init(1, 0x29U, 100000U) == 0 &&
         eos_esp32_i2c_slave_listen(1, 0) == 0;
    bool nack2 = ok && eos_i2c_read(0, I2C_SLAVE_ADDR, s_rx, 1) < 0;
    bool other = !eos_esp32_i2c_slave_addressed(1);   /* 0x29 must not match 0x28 */
    put(heard ? ", slave matched" : ", slave did not match");
    put(other ? ", other address ignored" : ", other address matched");
    check(nack && heard && nack2 && other);
}

static void test_i2c_write(uint32_t hz, uint32_t len)
{
    put("CHECK i2c0 -> i2c1 slave 0x28 (wired): write ");
    put_u(len);
    put(" bytes at ");
    put_u(hz / 1000U);
    put(" kHz ->");
    bool ok = i2c_pair(hz);
    for (uint32_t i = 0; i < len; i++) s_tx[i] = (uint8_t)(0xA0U + i * 7U);
    int n = ok ? eos_i2c_write(0, I2C_SLAVE_ADDR, s_tx, len) : -1;
    int got = eos_esp32_i2c_slave_take(1, s_rx, sizeof(s_rx));
    put(" master ");
    put(n == (int)len ? "ok" : "error");
    put(", slave got ");
    put_u(got < 0 ? 0U : (uint32_t)got);
    bool same = n == (int)len && got == (int)len;
    for (uint32_t i = 0; same && i < len; i++) same = s_rx[i] == s_tx[i];
    put(same ? ", data matches" : ", data differs");
    check(same);
}

static void test_i2c_read(uint32_t hz, uint32_t len)
{
    put("CHECK i2c0 <- i2c1 slave 0x28 (wired): read ");
    put_u(len);
    put(" bytes at ");
    put_u(hz / 1000U);
    put(" kHz ->");
    bool ok = i2c_pair(hz);
    for (uint32_t i = 0; i < len; i++) s_tx[i] = (uint8_t)(0x5AU ^ (i * 13U));
    ok = ok && eos_esp32_i2c_slave_queue(1, s_tx, len) == (int)len;
    for (uint32_t i = 0; i < len; i++) s_rx[i] = 0;
    int n = ok ? eos_i2c_read(0, I2C_SLAVE_ADDR, s_rx, len) : -1;
    bool same = n == (int)len;
    for (uint32_t i = 0; same && i < len; i++) same = s_rx[i] == s_tx[i];
    put(same ? " data matches" : " data differs");
    check(same);
}

static void test_i2c_nack(void)
{
    put("CHECK i2c0 write to absent 0x50 fails, then 0x28 works (wired)");
    bool ok = i2c_pair(100000U);
    s_tx[0] = 1;
    bool nack = eos_i2c_write(0, 0x50U, s_tx, 1) < 0;
    bool after = ok && eos_i2c_write(0, I2C_SLAVE_ADDR, s_tx, 1) == 1;
    check(nack && after);
}

static void test_i2c_rejects(void)
{
    put("CHECK i2c rejects port 2, 1 kHz, 2 MHz, 10-bit address, empty read");
    eos_i2c_config_t c = { .port = 2, .clock_hz = 100000U, .own_addr = 0 };
    int bad = eos_i2c_init(&c) == 0;
    c.port = 0; c.clock_hz = 1000U;    bad += eos_i2c_init(&c) == 0;
    c.clock_hz = 2000000U;             bad += eos_i2c_init(&c) == 0;
    c.clock_hz = 100000U;
    bad += eos_i2c_init(&c) != 0;
    bad += eos_i2c_write(0, 0x150U, s_tx, 1) >= 0;
    bad += eos_i2c_read(0, I2C_SLAVE_ADDR, s_rx, 0) >= 0;
    check(bad == 0);
}

/* Pulls on an RTC-capable pad (GPIO4, 33) live in the RTC IO register and
 * on a digital pad (GPIO23) in IO_MUX; an input with nothing attached must
 * follow either. */
static void test_gpio_pulls(void)
{
    static const uint16_t pins[] = { 4, 33, 23 };
    put("CHECK gpio pull-up/pull-down read back on GPIO4, 33 (RTC pads) and 23:");
    bool ok = true;
    for (uint32_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
        eos_gpio_config_t g = { .pin = pins[i], .mode = EOS_GPIO_INPUT, .pull = EOS_GPIO_PULL_UP };
        bool up = eos_gpio_init(&g) == 0;
        uint32_t t0 = eos_esp32_ccount();
        while (eos_esp32_ccount() - t0 < 240000U) {
        }
        up = up && eos_gpio_read(pins[i]);
        g.pull = EOS_GPIO_PULL_DOWN;
        bool down = eos_gpio_init(&g) == 0;
        t0 = eos_esp32_ccount();
        while (eos_esp32_ccount() - t0 < 240000U) {
        }
        down = down && !eos_gpio_read(pins[i]);
        put(" ");
        put_u(pins[i]);
        put(up && down ? "=ok" : "=bad");
        ok = ok && up && down;
    }
    check(ok);
}

/* ---- I2S0: DOUT looped to DIN in the matrix ----------------------------- */

/* 14 leading zeros: the receiver's FIFO is full of older words when the
 * pattern starts, and drops what arrives before eos_audio_record() reads;
 * the zeros take that loss. 14 + 48 + the closing zero frame is 64 words,
 * one transmit FIFO, so eos_audio_play() returns at once instead of waiting
 * for the FIFO to drain while the receiver overflows. */
#define LEAD 14
static int16_t s_pcm_tx[LEAD + 48], s_pcm_rx[400];

static int find_pattern(int n_bytes, int div)
{
    for (int i = 0; n_bytes > 0 && i + 48 <= n_bytes / 2; i++) {
        bool m = true;
        for (int k = 0; m && k < 48; k++) m = s_pcm_rx[i + k] == s_pcm_tx[LEAD + k] / div;
        if (m) return i;
    }
    return -1;
}

/* Count rising edges on @p pin over @p ms by sampling GPIO_IN. */
static uint32_t edges(uint16_t pin, uint32_t ms)
{
    uint32_t n = 0, prev = (REG32(GPIO_IN_REG) >> pin) & 1U;
    uint32_t t0 = eos_esp32_ccount(), span = ms * (EOS_ESP32_CPU_MHZ * 1000U);
    while (eos_esp32_ccount() - t0 < span) {
        uint32_t v = (REG32(GPIO_IN_REG) >> pin) & 1U;
        if (v && !prev) n++;
        prev = v;
    }
    return n;
}

static void test_i2s(uint32_t rate)
{
    put("CHECK i2s0 ");
    put_u(rate);
    put(" Hz stereo 16-bit:");
    eos_audio_config_t cfg = { .id = 0, .sample_rate = rate, .channels = 2,
                               .format = EOS_AUDIO_FMT_PCM_16 };
    bool ok = eos_audio_init(&cfg) == 0 && eos_esp32_i2s_set_loopback(true) == 0;
    uint32_t got_rate = eos_esp32_i2s_get_rate();
    uint32_t ws = edges(EOS_BOARD_I2S_WS_GPIO, 20) * 50U;     /* per second */
    uint32_t bck = edges(EOS_BOARD_I2S_BCK_GPIO, 2) * 500U;
    put(" divider ");
    put_u(got_rate);
    put(" Hz, WS ");
    put_u(ws);
    put(" Hz, BCK ");
    put_u(bck);
    put(" Hz;");
    for (uint32_t i = 0; i < 48U; i++) {
        s_pcm_tx[LEAD + i] = (int16_t)(0x1234 + (int32_t)i * 0x0507 - (int32_t)(i & 1U) * 0x4000);
    }
    ok = ok && eos_audio_play(0, (const uint8_t *)s_pcm_tx, sizeof(s_pcm_tx)) == (int)sizeof(s_pcm_tx);
    int n = ok ? eos_audio_record(0, (uint8_t *)s_pcm_rx, sizeof(s_pcm_rx), 100) : -1;
    /* The receiver runs from the start: find the pattern in what it heard. */
    int at = find_pattern(n, 1);
    put(at >= 0 ? " 48 samples looped back in order" : " pattern not found");
    eos_audio_deinit(0);
    bool clocks = got_rate > rate * 999U / 1000U && got_rate < rate * 1001U / 1000U &&
                  ws > rate * 97U / 100U && ws < rate * 103U / 100U &&
                  bck > rate * 32U * 97U / 100U && bck < rate * 32U * 103U / 100U;
    check(ok && at >= 0 && clocks);
}

static void test_i2s_mono(void)
{
    /* One channel: each sample goes out in both slots and comes back once.
     * 7 lead zeros + 24 samples, doubled, plus the zero frame: 64 words. */
    static int16_t tx[31], rx[200];
    put("CHECK i2s0 mono 16 kHz: 24 samples looped back in order");
    eos_audio_config_t cfg = { .id = 0, .sample_rate = 16000, .channels = 1,
                               .format = EOS_AUDIO_FMT_PCM_16 };
    bool ok = eos_audio_init(&cfg) == 0 && eos_esp32_i2s_set_loopback(true) == 0;
    for (uint32_t i = 0; i < 24U; i++) tx[7U + i] = (int16_t)(-3000 + (int32_t)i * 251);
    ok = ok && eos_audio_play(0, (const uint8_t *)tx, sizeof(tx)) == (int)sizeof(tx);
    int n = ok ? eos_audio_record(0, (uint8_t *)rx, sizeof(rx), 100) : -1;
    int at = -1;
    for (int i = 0; n > 0 && i + 24 <= n / 2 && at < 0; i++) {
        bool m = true;
        for (int k = 0; m && k < 24; k++) m = rx[i + k] == tx[7 + k];
        if (m) at = i;
    }
    eos_audio_deinit(0);
    check(ok && n == (int)sizeof(rx) && at >= 0);
}

static void test_i2s_volume(void)
{
    put("CHECK i2s0 volume 50% halves, mute zeroes, and the line idles at 0 after play:");
    eos_audio_config_t cfg = { .id = 0, .sample_rate = 16000, .channels = 2,
                               .format = EOS_AUDIO_FMT_PCM_16 };
    bool ok = eos_audio_init(&cfg) == 0 && eos_esp32_i2s_set_loopback(true) == 0;
    for (uint32_t i = 0; i < 48U; i++) s_pcm_tx[LEAD + i] = (int16_t)(1000 + (int32_t)i * 100);
    ok = ok && eos_audio_set_volume(0, 50) == 0;
    ok = ok && eos_audio_play(0, (const uint8_t *)s_pcm_tx, sizeof(s_pcm_tx)) > 0;
    int n = ok ? eos_audio_record(0, (uint8_t *)s_pcm_rx, sizeof(s_pcm_rx), 100) : -1;
    int at = find_pattern(n, 2);
    /* After the pattern and the closing zero frame, only zeros. */
    bool idle = at >= 0;
    for (int i = at + 48 + 2; idle && i < n / 2; i++) idle = s_pcm_rx[i] == 0;
    eos_audio_set_volume(0, 100);
    ok = ok && eos_audio_mute(0, true) == 0;
    ok = ok && eos_audio_play(0, (const uint8_t *)s_pcm_tx, sizeof(s_pcm_tx)) > 0;
    n = ok ? eos_audio_record(0, (uint8_t *)s_pcm_rx, sizeof(s_pcm_rx), 100) : -1;
    bool silent = n > 0;
    for (int i = 0; silent && i < n / 2; i++) silent = s_pcm_rx[i] == 0;
    eos_audio_mute(0, false);
    eos_audio_deinit(0);
    put(at >= 0 ? " halved" : " not halved");
    put(idle ? ", idles at 0" : ", does not idle at 0");
    put(silent ? ", muted silent" : ", muted not silent");
    check(ok && at >= 0 && idle && silent);
}

int main(void)
{
    eos_hal_init();
    put("bus_test: starting\n");
    test_gpio_pulls();
    test_spi_loopback(2);
    test_spi_loopback(3);
    test_spi_write_read();
    test_spi_clock();
    test_spi_rejects();
    test_i2s(8000U);
    test_i2s(44100U);
    test_i2s_mono();
    test_i2s_volume();
    test_i2c_address_no_wire();
    test_i2c_rejects();
    if (i2c_wired()) {
        test_i2c_write(100000U, 4);
        test_i2c_write(400000U, 31);
        test_i2c_write(100000U, 32);   /* the slave's RX FIFO holds 32 */
        test_i2c_read(100000U, 1);
        test_i2c_read(400000U, 16);
        test_i2c_read(100000U, 32);
        test_i2c_nack();
    } else {
        put("CHECK i2c data transfers: NOT RUN (join GPIO21-GPIO33 and GPIO22-GPIO32 to run them)\n");
    }
    put(s_failures == 0 ? "BUS TEST PASSED\n" : "BUS TEST FAILED\n");
    for (;;) {
        __asm volatile ("waiti 0");
    }
}
