// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_hal.c
 * @brief Unit tests for EoS Hardware Abstraction Layer
 */

#include <eos/hal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    static void name(void); \
    static void run_##name(void) { \
        printf("  %-50s ", #name); \
        name(); \
        tests_passed++; \
        printf("[PASS]\n"); \
    } \
    static void name(void)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while(0)

/* Mock backend for testing */
static int mock_init_called = 0;
static int mock_deinit_called = 0;
static bool mock_gpio_state[256];

static int mock_init(void) { mock_init_called++; return 0; }
static void mock_deinit(void) { mock_deinit_called++; }
static void mock_delay(uint32_t ms) { (void)ms; }
static uint32_t mock_ticks = 0;
static uint32_t mock_get_tick(void) { return mock_ticks++; }

static int mock_gpio_init(const eos_gpio_config_t *cfg) {
    if (!cfg) return -1;
    if (cfg->pin >= 256) return -1;
    mock_gpio_state[cfg->pin] = false;
    return 0;
}
static void mock_gpio_deinit(uint16_t pin) { (void)pin; }
static void mock_gpio_write(uint16_t pin, bool val) { if (pin < 256) mock_gpio_state[pin] = val; }
static bool mock_gpio_read(uint16_t pin) { return (pin < 256) ? mock_gpio_state[pin] : false; }
static void mock_gpio_toggle(uint16_t pin) { if (pin < 256) mock_gpio_state[pin] = !mock_gpio_state[pin]; }
static int mock_gpio_set_irq(uint16_t pin, eos_gpio_edge_t e, eos_gpio_callback_t cb, void *ctx)
{ (void)pin; (void)e; (void)cb; (void)ctx; return 0; }

static int mock_uart_init(const eos_uart_config_t *c) { (void)c; return 0; }
static void mock_uart_deinit(uint8_t p) { (void)p; }
static int mock_uart_write(uint8_t p, const uint8_t *d, size_t l) { (void)p; (void)d; (void)l; return 0; }
static int mock_uart_read(uint8_t p, uint8_t *d, size_t l, uint32_t t) { (void)p; (void)d; (void)l; (void)t; return 0; }

static int mock_spi_init(const eos_spi_config_t *c) { (void)c; return 0; }
static void mock_spi_deinit(uint8_t p) { (void)p; }
static int mock_spi_transfer(uint8_t p, const uint8_t *tx, uint8_t *rx, size_t l) { (void)p; (void)tx; (void)rx; (void)l; return 0; }

static int mock_i2c_init(const eos_i2c_config_t *c) { (void)c; return 0; }
static void mock_i2c_deinit(uint8_t p) { (void)p; }
static int mock_i2c_write(uint8_t p, uint16_t a, const uint8_t *d, size_t l) { (void)p; (void)a; (void)d; (void)l; return 0; }
static int mock_i2c_read(uint8_t p, uint16_t a, uint8_t *d, size_t l) { (void)p; (void)a; (void)d; (void)l; return 0; }

static int mock_timer_init(const eos_timer_config_t *c) { (void)c; return 0; }
static void mock_timer_deinit(uint8_t id) { (void)id; }
static int mock_timer_start(uint8_t id) { (void)id; return 0; }
static int mock_timer_stop(uint8_t id) { (void)id; return 0; }

static void mock_irq_disable(void) {}
static void mock_irq_enable(void) {}

static const eos_hal_backend_t mock_backend = {
    .name = "mock", .init = mock_init, .deinit = mock_deinit,
    .delay_ms = mock_delay, .get_tick_ms = mock_get_tick,
    .gpio_init = mock_gpio_init, .gpio_deinit = mock_gpio_deinit,
    .gpio_write = mock_gpio_write, .gpio_read = mock_gpio_read,
    .gpio_toggle = mock_gpio_toggle, .gpio_set_irq = mock_gpio_set_irq,
    .uart_init = mock_uart_init, .uart_deinit = mock_uart_deinit,
    .uart_write = mock_uart_write, .uart_read = mock_uart_read,
    .spi_init = mock_spi_init, .spi_deinit = mock_spi_deinit,
    .spi_transfer = mock_spi_transfer,
    .i2c_init = mock_i2c_init, .i2c_deinit = mock_i2c_deinit,
    .i2c_write = mock_i2c_write, .i2c_read = mock_i2c_read,
    .timer_init = mock_timer_init, .timer_deinit = mock_timer_deinit,
    .timer_start = mock_timer_start, .timer_stop = mock_timer_stop,
    .irq_disable = mock_irq_disable, .irq_enable = mock_irq_enable,
};

static void setup(void) {
    mock_init_called = 0;
    mock_deinit_called = 0;
    mock_ticks = 0;
    memset(mock_gpio_state, 0, sizeof(mock_gpio_state));
    eos_hal_register_backend(&mock_backend);
}

TEST(test_init_deinit) {
    setup();
    ASSERT(eos_hal_init() == 0);
    ASSERT(mock_init_called == 1);
    eos_hal_deinit();
    ASSERT(mock_deinit_called == 1);
}

TEST(test_gpio_write_read) {
    setup();
    eos_hal_init();
    eos_gpio_config_t cfg = { .pin = 13, .mode = EOS_GPIO_OUTPUT };
    ASSERT(eos_gpio_init(&cfg) == 0);
    ASSERT(eos_gpio_read(13) == false);
    eos_gpio_write(13, true);
    ASSERT(eos_gpio_read(13) == true);
    eos_gpio_write(13, false);
    ASSERT(eos_gpio_read(13) == false);
}

TEST(test_gpio_toggle) {
    setup();
    eos_hal_init();
    eos_gpio_config_t cfg = { .pin = 5, .mode = EOS_GPIO_OUTPUT };
    eos_gpio_init(&cfg);
    ASSERT(eos_gpio_read(5) == false);
    eos_gpio_toggle(5);
    ASSERT(eos_gpio_read(5) == true);
    eos_gpio_toggle(5);
    ASSERT(eos_gpio_read(5) == false);
}

TEST(test_tick_counter) {
    setup();
    eos_hal_init();
    uint32_t t1 = eos_get_tick_ms();
    uint32_t t2 = eos_get_tick_ms();
    ASSERT(t2 > t1);
}

TEST(test_no_backend) {
    eos_hal_register_backend(NULL);
    ASSERT(eos_hal_init() == -1);
    ASSERT(eos_gpio_read(0) == false);
    ASSERT(eos_get_tick_ms() == 0);
}

int main(void) {
    printf("=== EoS: HAL Unit Tests ===\n\n");
    run_test_init_deinit();
    run_test_gpio_write_read();
    run_test_gpio_toggle();
    run_test_tick_counter();
    run_test_no_backend();
    tests_run = 5;
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
