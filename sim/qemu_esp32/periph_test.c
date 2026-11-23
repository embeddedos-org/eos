// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
/**
 * @file periph_test.c
 * @brief On-board check of the ESP32 ADC, DAC, PWM and UART1/2 drivers.
 *
 * Every check is a loopback inside the chip, so a bare DevKit needs no wire:
 *
 *  - DAC 0/1 drive GPIO25/26, which are also ADC2 channels 8/9 (EoS ADC
 *    channels 18/19): the ADC measures the DAC.
 *  - ADC1 channel 4 (GPIO32) is read with the pad's internal pull-up, then
 *    pull-down, set directly in the RTC pad register (the one register this
 *    test writes itself).
 *  - PWM channel 0 drives the LED pin; the pad is sampled through GPIO_IN for
 *    frequency and duty.
 *  - UART1 and UART2 run in the peripheral's loopback mode, through every
 *    frame format at two baud rates, reconfigured back to back.
 *  - GPIO25 is driven as a digital pin after the DAC used it, which only
 *    works if the analog mux was released.
 *
 * Prints one CHECK line per item and "PERIPH TEST PASSED" or
 * "PERIPH TEST FAILED". Needs a board: QEMU's esp32 machine does not model
 * the SAR ADC, the DAC, LEDC, UART1/2 loopback or GPIO pad readback, so
 * there the test runs to completion and fails (CI builds it, it does not
 * run it).
 */
#include <stdbool.h>
#include <stdint.h>
#include "eos/hal.h"
#include "eos/hal_extended.h"
#include "board_esp32.h"

#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define GPIO_IN_REG 0x3FF4403CU
#define CYCLES_PER_MS 40000U

static int s_failures;

static void put(const char *s) { eos_esp32_console_puts(s); }
static void put_u(uint32_t v) { eos_esp32_console_put_u32(v); }

static void check(const char *name, bool ok)
{
    put(ok ? "  PASS\n" : "  FAIL\n");
    (void)name;
    if (!ok) s_failures++;
}

static void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++) {
        uint32_t t0 = eos_esp32_ccount();
        while (eos_esp32_ccount() - t0 < CYCLES_PER_MS) {
        }
    }
}

/* Average of 8 reads; 0xFFFFFFFF, the driver's timeout marker, if any read
 * timed out, so a failed conversion is not averaged into a plausible number. */
static uint32_t adc_avg(uint8_t ch)
{
    uint32_t sum = 0;
    for (int i = 0; i < 8; i++) {
        uint32_t v = eos_adc_read(ch);
        if (v == 0xFFFFFFFFU) return v;
        sum += v;
    }
    return sum / 8U;
}

static void test_dac_adc(uint8_t dac, uint8_t adc)
{
    put("CHECK dac");
    put_u(dac);
    put("->adc");
    put_u(adc);
    eos_dac_config_t dc = { .channel = dac, .resolution_bits = 8, .vref_mv = 3300 };
    eos_adc_config_t ac = { .channel = adc, .resolution_bits = 12, .sample_rate_hz = 0 };
    if (eos_dac_init(&dc) != 0 || eos_adc_init(&ac) != 0) { put(" init failed"); check("dac", false); return; }
    static const uint8_t levels[5] = { 0, 64, 128, 192, 255 };
    uint32_t raw[5];
    bool monotonic = true;
    for (int i = 0; i < 5; i++) {
        eos_dac_write(dac, levels[i]);
        delay_ms(2);
        raw[i] = adc_avg(adc);
        if (i > 0 && raw[i] <= raw[i - 1]) monotonic = false;
        put(i ? "/" : " raw ");
        put_u(raw[i]);
    }
    /* write_mv / read_mv: 1650 mV in, read back through the eFuse-calibrated
     * conversion. Allow 5%: the DAC itself is uncalibrated (VDD x code/256,
     * with VDD taken as 3300 mV). */
    eos_dac_write_mv(dac, 1650);
    delay_ms(2);
    uint32_t mv = 0;
    for (int i = 0; i < 8; i++) {
        uint32_t v = eos_adc_read_mv(adc);
        if (v == 0xFFFFFFFFU) { mv = v; break; }
        mv += v;
    }
    if (mv != 0xFFFFFFFFU) mv /= 8U;
    put(" 1650mV->");
    put_u(mv);
    put("mV");
    bool mv_ok = mv > 1650U * 95U / 100U && mv < 1650U * 105U / 100U;
    eos_adc_deinit(adc);
    eos_dac_deinit(dac);
    check("dac_adc", monotonic && raw[0] < 300U && raw[4] > 3000U && mv_ok);
}

/* The calibration is read from eFuse on the chip itself; a Vref outside
 * 1000-1200 mV (ESP-IDF's table range) means the field was misread. */
static void test_adc_cal_source(void)
{
    static const char *const names[] = { "two-point", "efuse-vref", "default-vref" };
    uint32_t vref = 0;
    eos_esp32_adc_cal_t src = eos_esp32_adc_cal_source(&vref);
    put("CHECK adc calibration source ");
    put(src <= EOS_ESP32_ADC_CAL_DEFAULT_VREF ? names[src] : "?");
    put(" vref ");
    put_u(vref);
    put("mV");
    check("adc_cal", src <= EOS_ESP32_ADC_CAL_DEFAULT_VREF && vref >= 1000U && vref <= 1200U);
}

static void test_adc1_pulls(void)
{
    /* GPIO32's RTC pad register and its pull bits (rtc_io_desc[] for X32P). */
    const uint32_t pad = 0x3FF4848CU, rue = 0x00400000U, rde = 0x00800000U;
    put("CHECK adc1 ch4 (GPIO32) pull-up/pull-down");
    eos_adc_config_t ac = { .channel = 4, .resolution_bits = 12, .sample_rate_hz = 0 };
    if (eos_adc_init(&ac) != 0) { put(" init failed"); check("adc1", false); return; }
    REG32(pad) = (REG32(pad) & ~rde) | rue;
    delay_ms(5);
    uint32_t up = adc_avg(4);
    REG32(pad) = (REG32(pad) & ~rue) | rde;
    delay_ms(5);
    uint32_t down = adc_avg(4);
    REG32(pad) &= ~(rue | rde);
    eos_adc_deinit(4);
    put(" up ");
    put_u(up);
    put(" down ");
    put_u(down);
    check("adc1", up > 2500U && down < 500U);
}

/* Sample the LED pad for @p ms; return rising edges and the high fraction in
 * tenths of a percent. */
static void sample_pin(uint32_t pin, uint32_t ms, uint32_t *edges, uint32_t *high_x10)
{
    uint32_t t0 = eos_esp32_ccount(), n = 0, hi = 0, rises = 0, prev = 0;
    while (eos_esp32_ccount() - t0 < ms * CYCLES_PER_MS) {
        uint32_t v = (REG32(GPIO_IN_REG) >> pin) & 1U;
        if (v && !prev) rises++;
        prev = v;
        hi += v;
        n++;
    }
    *edges = rises;
    /* 32-bit only (no libgcc): hi <= n, and n/10 >= 1 for any ms >= 1. */
    *high_x10 = hi * 100U / (n / 10U);
}

static void test_pwm(void)
{
    const uint32_t pin = EOS_BOARD_LED_GPIO;
    uint32_t edges, duty;
    eos_pwm_config_t pc = { .channel = 0, .frequency_hz = 1000, .duty_pct_x10 = 250 };
    put("CHECK pwm ch0 init 1 kHz 25%");
    bool ok = eos_pwm_init(&pc) == 0;
    sample_pin(pin, 200, &edges, &duty);
    put(" -> ");
    put_u(edges);
    put(" edges/200ms duty x10 ");
    put_u(duty);
    check("pwm", ok && edges >= 195U && edges <= 205U && duty >= 230U && duty <= 270U);

    put("CHECK pwm set_duty 75%");
    ok = eos_pwm_set_duty(0, 750) == 0;
    delay_ms(5);
    sample_pin(pin, 200, &edges, &duty);
    put(" -> duty x10 ");
    put_u(duty);
    check("pwm_duty", ok && duty >= 730U && duty <= 770U);

    put("CHECK pwm set_freq 5 kHz");
    ok = eos_pwm_set_freq(0, 5000) == 0;
    delay_ms(5);
    sample_pin(pin, 200, &edges, &duty);
    put(" -> ");
    put_u(edges);
    put(" edges/200ms duty x10 ");
    put_u(duty);
    check("pwm_freq", ok && edges >= 990U && edges <= 1010U && duty >= 730U && duty <= 770U);

    put("CHECK pwm stop");
    ok = eos_pwm_stop(0) == 0;
    delay_ms(2);
    sample_pin(pin, 50, &edges, &duty);
    put(" -> ");
    put_u(edges);
    put(" edges");
    check("pwm_stop", ok && edges == 0U);

    put("CHECK pwm rejects channel 4, 0 Hz and duty > 100%");
    eos_pwm_config_t bad1 = { .channel = 4, .frequency_hz = 1000, .duty_pct_x10 = 500 };
    eos_pwm_config_t bad2 = { .channel = 1, .frequency_hz = 0, .duty_pct_x10 = 500 };
    eos_pwm_config_t bad3 = { .channel = 1, .frequency_hz = 1000, .duty_pct_x10 = 1001 };
    check("pwm_args", eos_pwm_init(&bad1) != 0 && eos_pwm_init(&bad2) != 0 && eos_pwm_init(&bad3) != 0);

    put("CHECK pwm frequency range: 1 Hz and 8388608 Hz accepted, 8388609 Hz refused");
    eos_pwm_config_t lo = { .channel = 1, .frequency_hz = 1, .duty_pct_x10 = 500 };
    eos_pwm_config_t hi = { .channel = 1, .frequency_hz = 8388608U, .duty_pct_x10 = 500 };
    eos_pwm_config_t over = { .channel = 1, .frequency_hz = 8388609U, .duty_pct_x10 = 500 };
    bool range_ok = eos_pwm_init(&lo) == 0 && eos_pwm_init(&hi) == 0 && eos_pwm_init(&over) != 0;
    eos_pwm_deinit(1);
    check("pwm_range", range_ok);
    eos_pwm_deinit(0);
}

/* One loopback transfer of 64 random bytes; true if all came back. */
static bool uart_loop(uint8_t port, uint32_t baud, uint8_t bits, eos_uart_parity_t par, eos_uart_stop_t stop)
{
    eos_uart_config_t uc = { .port = port, .baudrate = baud, .data_bits = bits,
                             .parity = par, .stop_bits = stop };
    if (eos_uart_init(&uc) != 0 || eos_esp32_uart_set_loopback(port, true) != 0) return false;
    uint8_t out[64], in[64];
    static uint32_t seed = 12345U;
    seed ^= eos_esp32_ccount();
    uint8_t mask = (uint8_t)((1U << bits) - 1U);
    for (int i = 0; i < 64; i++) { seed = seed * 1103515245U + 12345U; out[i] = (uint8_t)(seed >> 16) & mask; }
    eos_uart_write(port, out, sizeof(out));
    int n = eos_uart_read(port, in, sizeof(in), 100);
    int same = 0;
    for (int i = 0; i < n; i++) same += (in[i] & mask) == out[i];
    eos_esp32_uart_set_loopback(port, false);
    return n == 64 && same == 64;
}

/*
 * Every frame format on both UARTs, each configured straight after the
 * previous one. Reconfiguration is where the bugs were: the RX FIFO read
 * through the AHB alias, and FIFO reset bits toggled on UART1/2, made every
 * second reconfiguration of UART2 receive garbage.
 */
static void test_uart_formats(uint32_t baud)
{
    put("CHECK uart1+2 loopback, 5-8 data bits x none/even/odd x 1/2 stop at ");
    put_u(baud);
    int pass = 0, total = 0;
    for (uint8_t port = 1; port <= 2; port++) {
        for (uint8_t bits = 5; bits <= 8; bits++) {
            for (int par = 0; par <= 2; par++) {
                for (int st = 0; st <= 1; st++) {
                    total++;
                    if (uart_loop(port, baud, bits, (eos_uart_parity_t)par, (eos_uart_stop_t)st)) {
                        pass++;
                    } else {
                        put(" [fail uart");
                        put_u(port);
                        put(" ");
                        put_u(bits);
                        put(par == 0 ? "N" : par == 1 ? "E" : "O");
                        put(st ? "2]" : "1]");
                    }
                }
            }
        }
    }
    put(" -> ");
    put_u((uint32_t)pass);
    put("/");
    put_u((uint32_t)total);
    check("uart_formats", pass == total);
}

static void test_uart_rejects(void)
{
    put("CHECK uart rejects port 3, 4 data bits and 10 Mbaud");
    eos_uart_config_t a = { .port = 3, .baudrate = 115200, .data_bits = 8, .parity = EOS_UART_PARITY_NONE, .stop_bits = EOS_UART_STOP_1 };
    eos_uart_config_t b = { .port = 1, .baudrate = 115200, .data_bits = 4, .parity = EOS_UART_PARITY_NONE, .stop_bits = EOS_UART_STOP_1 };
    eos_uart_config_t c = { .port = 2, .baudrate = 10000000, .data_bits = 8, .parity = EOS_UART_PARITY_NONE, .stop_bits = EOS_UART_STOP_1 };
    check("uart_args", eos_uart_init(&a) != 0 && eos_uart_init(&b) != 0 && eos_uart_init(&c) != 0);
}

static void test_pad_release(void)
{
    put("CHECK GPIO25 digital again after the DAC");
    eos_gpio_config_t g = { .pin = 25, .mode = EOS_GPIO_OUTPUT, .pull = EOS_GPIO_PULL_NONE,
                            .speed = EOS_GPIO_SPEED_LOW, .af_num = 0 };
    bool ok = eos_gpio_init(&g) == 0;
    int bad = 0;
    for (int i = 0; i < 20; i++) {
        eos_gpio_write(25, i & 1);
        if (eos_gpio_read(25) != (bool)(i & 1)) bad++;
    }
    eos_gpio_write(25, false);
    put(" -> ");
    put_u((uint32_t)bad);
    put(" wrong of 20");
    check("pad_release", ok && bad == 0);
}

int main(void)
{
    put("\nEoS ESP32 peripheral test\n");
    if (eos_hal_init() != 0) { put("hal init failed\n"); return 1; }
    test_adc_cal_source();
    test_dac_adc(0, 18);
    test_dac_adc(1, 19);
    test_adc1_pulls();
    test_pwm();
    test_uart_formats(115200);
    test_uart_formats(921600);
    test_uart_rejects();
    test_pad_release();
    put(s_failures == 0 ? "PERIPH TEST PASSED\n" : "PERIPH TEST FAILED\n");
    for (;;) {
    }
}
