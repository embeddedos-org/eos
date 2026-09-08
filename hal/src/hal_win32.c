// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file hal_win32.c
 * @brief EoS HAL backend for hosted Windows builds.
 *
 * Windows has a host clock but no POSIX GPIO/sysfs implementation. Peripheral
 * operations remain explicit no-op stubs, matching the generic host contract.
 */

#include <eos/hal.h>
#include <eos/eos_windows.h>

#if defined(_WIN32)

static bool hal_initialized;
static ULONGLONG host_start_tick;

static int win32_gpio_init(const eos_gpio_config_t *cfg) { (void)cfg; return 0; }
static void win32_gpio_deinit(uint16_t pin) { (void)pin; }
static void win32_gpio_write(uint16_t pin, bool value) { (void)pin; (void)value; }
static bool win32_gpio_read(uint16_t pin) { (void)pin; return false; }
static void win32_gpio_toggle(uint16_t pin) { (void)pin; }
static int win32_gpio_set_irq(uint16_t pin, eos_gpio_edge_t edge,
                              eos_gpio_callback_t cb, void *ctx) {
    (void)pin; (void)edge; (void)cb; (void)ctx; return 0;
}

static int win32_uart_init(const eos_uart_config_t *cfg) { (void)cfg; return 0; }
static void win32_uart_deinit(uint8_t port) { (void)port; }
static int win32_uart_write(uint8_t port, const uint8_t *data, size_t len) {
    (void)port; (void)data; (void)len; return 0;
}
static int win32_uart_read(uint8_t port, uint8_t *data, size_t len, uint32_t timeout_ms) {
    (void)port; (void)data; (void)len; (void)timeout_ms; return 0;
}

static int win32_spi_init(const eos_spi_config_t *cfg) { (void)cfg; return 0; }
static void win32_spi_deinit(uint8_t port) { (void)port; }
static int win32_spi_transfer(uint8_t port, const uint8_t *tx, uint8_t *rx, size_t len) {
    (void)port; (void)tx; (void)rx; (void)len; return 0;
}

static int win32_i2c_init(const eos_i2c_config_t *cfg) { (void)cfg; return 0; }
static void win32_i2c_deinit(uint8_t port) { (void)port; }
static int win32_i2c_write(uint8_t port, uint16_t addr, const uint8_t *data, size_t len) {
    (void)port; (void)addr; (void)data; (void)len; return 0;
}
static int win32_i2c_read(uint8_t port, uint16_t addr, uint8_t *data, size_t len) {
    (void)port; (void)addr; (void)data; (void)len; return 0;
}

static int win32_timer_init(const eos_timer_config_t *cfg) { (void)cfg; return 0; }
static void win32_timer_deinit(uint8_t timer_id) { (void)timer_id; }
static int win32_timer_start(uint8_t timer_id) { (void)timer_id; return 0; }
static int win32_timer_stop(uint8_t timer_id) { (void)timer_id; return 0; }
static void win32_irq_disable(void) {}
static void win32_irq_enable(void) {}

static void win32_delay_ms(uint32_t ms) { Sleep(ms); }

static uint32_t win32_get_tick_ms(void) {
    return hal_initialized ? (uint32_t)(GetTickCount64() - host_start_tick) : 0;
}

static int win32_hal_init(void) {
    host_start_tick = GetTickCount64();
    hal_initialized = true;
    return 0;
}

static void win32_hal_deinit(void) { hal_initialized = false; }

static const eos_hal_backend_t win32_backend = {
    .name = "win32",
    .init = win32_hal_init, .deinit = win32_hal_deinit,
    .delay_ms = win32_delay_ms, .get_tick_ms = win32_get_tick_ms,
    .gpio_init = win32_gpio_init, .gpio_deinit = win32_gpio_deinit,
    .gpio_write = win32_gpio_write, .gpio_read = win32_gpio_read,
    .gpio_toggle = win32_gpio_toggle, .gpio_set_irq = win32_gpio_set_irq,
    .uart_init = win32_uart_init, .uart_deinit = win32_uart_deinit,
    .uart_write = win32_uart_write, .uart_read = win32_uart_read,
    .spi_init = win32_spi_init, .spi_deinit = win32_spi_deinit,
    .spi_transfer = win32_spi_transfer,
    .i2c_init = win32_i2c_init, .i2c_deinit = win32_i2c_deinit,
    .i2c_write = win32_i2c_write, .i2c_read = win32_i2c_read,
    .timer_init = win32_timer_init, .timer_deinit = win32_timer_deinit,
    .timer_start = win32_timer_start, .timer_stop = win32_timer_stop,
    .irq_disable = win32_irq_disable, .irq_enable = win32_irq_enable,
};

void eos_hal_win32_register(void) { eos_hal_register_backend(&win32_backend); }

#endif /* _WIN32 */
