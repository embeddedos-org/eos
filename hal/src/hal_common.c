// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file hal_common.c
 * @brief EoS HAL — Common dispatch layer
 *
 * Dispatches HAL API calls to the registered platform backend.
 */

#include <eos/hal.h>
#include <string.h>

static const eos_hal_backend_t *active_backend = NULL;

void eos_hal_register_backend(const eos_hal_backend_t *backend)
{
    active_backend = backend;
}

int eos_hal_init(void)
{
    if (!active_backend || !active_backend->init) return -1;
    return active_backend->init();
}

void eos_hal_deinit(void)
{
    if (active_backend && active_backend->deinit) {
        active_backend->deinit();
    }
}

void eos_delay_ms(uint32_t ms)
{
    if (active_backend && active_backend->delay_ms) {
        active_backend->delay_ms(ms);
    }
}

uint32_t eos_get_tick_ms(void)
{
    if (active_backend && active_backend->get_tick_ms) {
        return active_backend->get_tick_ms();
    }
    return 0;
}

/* ---- GPIO ---- */

int eos_gpio_init(const eos_gpio_config_t *cfg)
{
    if (!active_backend || !active_backend->gpio_init) return -1;
    return active_backend->gpio_init(cfg);
}

void eos_gpio_deinit(uint16_t pin)
{
    if (active_backend && active_backend->gpio_deinit) {
        active_backend->gpio_deinit(pin);
    }
}

void eos_gpio_write(uint16_t pin, bool value)
{
    if (active_backend && active_backend->gpio_write) {
        active_backend->gpio_write(pin, value);
    }
}

bool eos_gpio_read(uint16_t pin)
{
    if (active_backend && active_backend->gpio_read) {
        return active_backend->gpio_read(pin);
    }
    return false;
}

void eos_gpio_toggle(uint16_t pin)
{
    if (active_backend && active_backend->gpio_toggle) {
        active_backend->gpio_toggle(pin);
    }
}

int eos_gpio_set_irq(uint16_t pin, eos_gpio_edge_t edge,
                      eos_gpio_callback_t cb, void *ctx)
{
    if (!active_backend || !active_backend->gpio_set_irq) return -1;
    return active_backend->gpio_set_irq(pin, edge, cb, ctx);
}

/* ---- UART ---- */

int eos_uart_init(const eos_uart_config_t *cfg)
{
    if (!active_backend || !active_backend->uart_init) return -1;
    return active_backend->uart_init(cfg);
}

void eos_uart_deinit(uint8_t port)
{
    if (active_backend && active_backend->uart_deinit) {
        active_backend->uart_deinit(port);
    }
}

int eos_uart_write(uint8_t port, const uint8_t *data, size_t len)
{
    if (!active_backend || !active_backend->uart_write) return -1;
    return active_backend->uart_write(port, data, len);
}

int eos_uart_read(uint8_t port, uint8_t *data, size_t len, uint32_t timeout_ms)
{
    if (!active_backend || !active_backend->uart_read) return -1;
    return active_backend->uart_read(port, data, len, timeout_ms);
}

int eos_uart_set_rx_callback(uint8_t port, eos_uart_rx_callback_t cb, void *ctx)
{
    (void)port; (void)cb; (void)ctx;
    return -1;
}

/* ---- SPI ---- */

int eos_spi_init(const eos_spi_config_t *cfg)
{
    if (!active_backend || !active_backend->spi_init) return -1;
    return active_backend->spi_init(cfg);
}

void eos_spi_deinit(uint8_t port)
{
    if (active_backend && active_backend->spi_deinit) {
        active_backend->spi_deinit(port);
    }
}

int eos_spi_transfer(uint8_t port, const uint8_t *tx, uint8_t *rx, size_t len)
{
    if (!active_backend || !active_backend->spi_transfer) return -1;
    return active_backend->spi_transfer(port, tx, rx, len);
}

int eos_spi_write(uint8_t port, const uint8_t *data, size_t len)
{
    return eos_spi_transfer(port, data, NULL, len);
}

int eos_spi_read(uint8_t port, uint8_t *data, size_t len)
{
    return eos_spi_transfer(port, NULL, data, len);
}

/* ---- I2C ---- */

int eos_i2c_init(const eos_i2c_config_t *cfg)
{
    if (!active_backend || !active_backend->i2c_init) return -1;
    return active_backend->i2c_init(cfg);
}

void eos_i2c_deinit(uint8_t port)
{
    if (active_backend && active_backend->i2c_deinit) {
        active_backend->i2c_deinit(port);
    }
}

int eos_i2c_write(uint8_t port, uint16_t addr, const uint8_t *data, size_t len)
{
    if (!active_backend || !active_backend->i2c_write) return -1;
    return active_backend->i2c_write(port, addr, data, len);
}

int eos_i2c_read(uint8_t port, uint16_t addr, uint8_t *data, size_t len)
{
    if (!active_backend || !active_backend->i2c_read) return -1;
    return active_backend->i2c_read(port, addr, data, len);
}

int eos_i2c_write_reg(uint8_t port, uint16_t addr, uint8_t reg,
                       const uint8_t *data, size_t len)
{
    (void)port; (void)addr; (void)reg; (void)data; (void)len;
    return -1;
}

int eos_i2c_read_reg(uint8_t port, uint16_t addr, uint8_t reg,
                      uint8_t *data, size_t len)
{
    (void)port; (void)addr; (void)reg; (void)data; (void)len;
    return -1;
}

/* ---- Timer ---- */

int eos_timer_init(const eos_timer_config_t *cfg)
{
    if (!active_backend || !active_backend->timer_init) return -1;
    return active_backend->timer_init(cfg);
}

void eos_timer_deinit(uint8_t timer_id)
{
    if (active_backend && active_backend->timer_deinit) {
        active_backend->timer_deinit(timer_id);
    }
}

int eos_timer_start(uint8_t timer_id)
{
    if (!active_backend || !active_backend->timer_start) return -1;
    return active_backend->timer_start(timer_id);
}

int eos_timer_stop(uint8_t timer_id)
{
    if (!active_backend || !active_backend->timer_stop) return -1;
    return active_backend->timer_stop(timer_id);
}

uint32_t eos_timer_get_count(uint8_t timer_id)
{
    (void)timer_id;
    return 0;
}

/* ---- Interrupt ---- */

void eos_irq_disable(void)
{
    if (active_backend && active_backend->irq_disable) {
        active_backend->irq_disable();
    }
}

void eos_irq_enable(void)
{
    if (active_backend && active_backend->irq_enable) {
        active_backend->irq_enable();
    }
}

int eos_irq_register(uint16_t irq_num, void (*handler)(void), uint8_t priority)
{
    (void)irq_num; (void)handler; (void)priority;
    return -1;
}

void eos_irq_unregister(uint16_t irq_num)
{
    (void)irq_num;
}

void eos_irq_set_priority(uint16_t irq_num, uint8_t priority)
{
    (void)irq_num; (void)priority;
}
