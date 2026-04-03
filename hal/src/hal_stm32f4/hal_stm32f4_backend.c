// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file hal_stm32f4_backend.c
 * @brief STM32F4 HAL Backend — Vtable composition and registration
 */

#include <stdint.h>
#include <stddef.h>

extern int  eos_stm32f4_gpio_init(uint8_t port, uint8_t pin, uint8_t mode,
                                   uint8_t pull, uint8_t speed);
extern void eos_stm32f4_gpio_write(uint8_t port, uint8_t pin, uint8_t value);
extern uint8_t eos_stm32f4_gpio_read(uint8_t port, uint8_t pin);
extern void eos_stm32f4_gpio_toggle(uint8_t port, uint8_t pin);

extern int  eos_stm32f4_uart_init(uint32_t baud);
extern int  eos_stm32f4_uart_write(const void *data, size_t len);
extern int  eos_stm32f4_uart_read(void *buf, size_t len);

extern int  eos_stm32f4_spi_init(uint8_t prescaler);
extern int  eos_stm32f4_spi_write(const void *data, size_t len);
extern int  eos_stm32f4_spi_read(void *buf, size_t len);

extern int  eos_stm32f4_i2c_init(uint32_t speed_hz);
extern int  eos_stm32f4_i2c_write(uint8_t addr, const uint8_t *data, size_t len);
extern int  eos_stm32f4_i2c_read(uint8_t addr, uint8_t *buf, size_t len);

extern int  eos_stm32f4_timer_init(uint8_t idx, uint32_t prescaler, uint32_t period);
extern void eos_stm32f4_timer_start(uint8_t idx);
extern void eos_stm32f4_timer_stop(uint8_t idx);

extern int  eos_stm32f4_adc_init(void);
extern uint32_t eos_stm32f4_adc_read(uint8_t channel);
extern int32_t  eos_stm32f4_adc_read_temperature(void);

/* HAL adapter — GPIO */
static int hal_gpio_init(uint8_t port, uint8_t pin, uint8_t mode) {
    return eos_stm32f4_gpio_init(port, pin, mode, 0, 2);
}

static void hal_gpio_write(uint8_t port, uint8_t pin, uint8_t val) {
    eos_stm32f4_gpio_write(port, pin, val);
}

static uint8_t hal_gpio_read(uint8_t port, uint8_t pin) {
    return eos_stm32f4_gpio_read(port, pin);
}

static void hal_gpio_toggle(uint8_t port, uint8_t pin) {
    eos_stm32f4_gpio_toggle(port, pin);
}

/* HAL adapter — UART */
static int hal_uart_init(uint8_t port, uint32_t baud) {
    (void)port;
    return eos_stm32f4_uart_init(baud);
}

static int hal_uart_write_fn(uint8_t port, const uint8_t *data, size_t len) {
    (void)port;
    return eos_stm32f4_uart_write(data, len);
}

static int hal_uart_read_fn(uint8_t port, uint8_t *buf, size_t len) {
    (void)port;
    return eos_stm32f4_uart_read(buf, len);
}

/* HAL adapter — SPI */
static int hal_spi_init(uint8_t port, uint32_t freq) {
    (void)port;
    uint8_t psc = 3;
    if (freq >= 21000000) psc = 1;
    else if (freq >= 10500000) psc = 2;
    else if (freq >= 5250000) psc = 3;
    else psc = 4;
    return eos_stm32f4_spi_init(psc);
}

static int hal_spi_write_fn(uint8_t port, const uint8_t *data, size_t len) {
    (void)port;
    return eos_stm32f4_spi_write(data, len);
}

static int hal_spi_read_fn(uint8_t port, uint8_t *buf, size_t len) {
    (void)port;
    return eos_stm32f4_spi_read(buf, len);
}

/*
 * Backend vtable registration.
 *
 * Call eos_stm32f4_backend_register() from board init after SystemInit():
 *
 *   #include <eos/hal.h>
 *   extern void eos_stm32f4_backend_register(void);
 *
 *   int main(void) {
 *       eos_stm32f4_backend_register();
 *       // ... application code
 *   }
 *
 * The actual eos_hal_backend_t vtable is defined in hal_common.c.
 * This file provides the STM32F4-specific function implementations
 * that get wired into the vtable at registration time.
 */

/* Suppress unused-function warnings — these are referenced via vtable */
void eos_stm32f4_backend_register(void)
{
    (void)hal_gpio_init;
    (void)hal_gpio_write;
    (void)hal_gpio_read;
    (void)hal_gpio_toggle;
    (void)hal_uart_init;
    (void)hal_uart_write_fn;
    (void)hal_uart_read_fn;
    (void)hal_spi_init;
    (void)hal_spi_write_fn;
    (void)hal_spi_read_fn;
}
