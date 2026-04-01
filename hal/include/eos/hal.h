// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file hal.h
 * @brief EoS Hardware Abstraction Layer — Platform-independent peripheral API
 *
 * Provides unified interfaces for GPIO, UART, SPI, I2C, Timer, and Interrupt
 * management across Linux and RTOS platforms.
 */

#ifndef EOS_HAL_H
#define EOS_HAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * HAL Initialization
 * ============================================================ */

/**
 * Initialize the HAL subsystem for the current platform.
 * Must be called before any other HAL function.
 *
 * @return 0 on success, negative error code on failure.
 */
int eos_hal_init(void);

/**
 * Deinitialize the HAL subsystem, releasing all resources.
 */
void eos_hal_deinit(void);

/**
 * Delay execution for a specified number of milliseconds.
 *
 * @param ms Milliseconds to delay.
 */
void eos_delay_ms(uint32_t ms);

/**
 * Get the system tick count in milliseconds since HAL init.
 *
 * @return Current tick count in milliseconds.
 */
uint32_t eos_get_tick_ms(void);

/* ============================================================
 * GPIO Interface
 * ============================================================ */

typedef enum {
    EOS_GPIO_INPUT  = 0,
    EOS_GPIO_OUTPUT = 1,
    EOS_GPIO_AF     = 2,
    EOS_GPIO_ANALOG = 3,
} eos_gpio_mode_t;

typedef enum {
    EOS_GPIO_PULL_NONE = 0,
    EOS_GPIO_PULL_UP   = 1,
    EOS_GPIO_PULL_DOWN = 2,
} eos_gpio_pull_t;

typedef enum {
    EOS_GPIO_SPEED_LOW    = 0,
    EOS_GPIO_SPEED_MEDIUM = 1,
    EOS_GPIO_SPEED_HIGH   = 2,
    EOS_GPIO_SPEED_VHIGH  = 3,
} eos_gpio_speed_t;

typedef enum {
    EOS_GPIO_EDGE_NONE    = 0,
    EOS_GPIO_EDGE_RISING  = 1,
    EOS_GPIO_EDGE_FALLING = 2,
    EOS_GPIO_EDGE_BOTH    = 3,
} eos_gpio_edge_t;

typedef struct {
    uint16_t pin;
    eos_gpio_mode_t mode;
    eos_gpio_pull_t pull;
    eos_gpio_speed_t speed;
    uint8_t af_num;
} eos_gpio_config_t;

typedef void (*eos_gpio_callback_t)(uint16_t pin, void *ctx);

int  eos_gpio_init(const eos_gpio_config_t *cfg);
void eos_gpio_deinit(uint16_t pin);
void eos_gpio_write(uint16_t pin, bool value);
bool eos_gpio_read(uint16_t pin);
void eos_gpio_toggle(uint16_t pin);
int  eos_gpio_set_irq(uint16_t pin, eos_gpio_edge_t edge,
                       eos_gpio_callback_t cb, void *ctx);

/* ============================================================
 * UART Interface
 * ============================================================ */

typedef enum {
    EOS_UART_PARITY_NONE = 0,
    EOS_UART_PARITY_EVEN = 1,
    EOS_UART_PARITY_ODD  = 2,
} eos_uart_parity_t;

typedef enum {
    EOS_UART_STOP_1  = 0,
    EOS_UART_STOP_2  = 1,
} eos_uart_stop_t;

typedef struct {
    uint8_t port;
    uint32_t baudrate;
    uint8_t data_bits;
    eos_uart_parity_t parity;
    eos_uart_stop_t stop_bits;
} eos_uart_config_t;

typedef void (*eos_uart_rx_callback_t)(uint8_t port, uint8_t byte, void *ctx);

int  eos_uart_init(const eos_uart_config_t *cfg);
void eos_uart_deinit(uint8_t port);
int  eos_uart_write(uint8_t port, const uint8_t *data, size_t len);
int  eos_uart_read(uint8_t port, uint8_t *data, size_t len, uint32_t timeout_ms);
int  eos_uart_set_rx_callback(uint8_t port, eos_uart_rx_callback_t cb, void *ctx);

/* ============================================================
 * SPI Interface
 * ============================================================ */

typedef enum {
    EOS_SPI_MODE_0 = 0, /* CPOL=0 CPHA=0 */
    EOS_SPI_MODE_1 = 1, /* CPOL=0 CPHA=1 */
    EOS_SPI_MODE_2 = 2, /* CPOL=1 CPHA=0 */
    EOS_SPI_MODE_3 = 3, /* CPOL=1 CPHA=1 */
} eos_spi_mode_t;

typedef struct {
    uint8_t port;
    uint32_t clock_hz;
    eos_spi_mode_t mode;
    uint8_t bits_per_word;
    uint16_t cs_pin;
} eos_spi_config_t;

int  eos_spi_init(const eos_spi_config_t *cfg);
void eos_spi_deinit(uint8_t port);
int  eos_spi_transfer(uint8_t port, const uint8_t *tx, uint8_t *rx, size_t len);
int  eos_spi_write(uint8_t port, const uint8_t *data, size_t len);
int  eos_spi_read(uint8_t port, uint8_t *data, size_t len);

/* ============================================================
 * I2C Interface
 * ============================================================ */

typedef struct {
    uint8_t port;
    uint32_t clock_hz;
    uint16_t own_addr;
} eos_i2c_config_t;

int  eos_i2c_init(const eos_i2c_config_t *cfg);
void eos_i2c_deinit(uint8_t port);
int  eos_i2c_write(uint8_t port, uint16_t addr, const uint8_t *data, size_t len);
int  eos_i2c_read(uint8_t port, uint16_t addr, uint8_t *data, size_t len);
int  eos_i2c_write_reg(uint8_t port, uint16_t addr, uint8_t reg,
                        const uint8_t *data, size_t len);
int  eos_i2c_read_reg(uint8_t port, uint16_t addr, uint8_t reg,
                       uint8_t *data, size_t len);

/* ============================================================
 * Timer Interface
 * ============================================================ */

typedef void (*eos_timer_callback_t)(uint8_t timer_id, void *ctx);

typedef struct {
    uint8_t timer_id;
    uint32_t period_us;
    bool auto_reload;
    eos_timer_callback_t callback;
    void *ctx;
} eos_timer_config_t;

int  eos_timer_init(const eos_timer_config_t *cfg);
void eos_timer_deinit(uint8_t timer_id);
int  eos_timer_start(uint8_t timer_id);
int  eos_timer_stop(uint8_t timer_id);
uint32_t eos_timer_get_count(uint8_t timer_id);

/* ============================================================
 * Interrupt Control Interface
 * ============================================================ */

void eos_irq_disable(void);
void eos_irq_enable(void);
int  eos_irq_register(uint16_t irq_num, void (*handler)(void), uint8_t priority);
void eos_irq_unregister(uint16_t irq_num);
void eos_irq_set_priority(uint16_t irq_num, uint8_t priority);

/* ============================================================
 * Platform Backend Registration (internal)
 * ============================================================ */

typedef struct {
    const char *name;

    int  (*init)(void);
    void (*deinit)(void);
    void (*delay_ms)(uint32_t ms);
    uint32_t (*get_tick_ms)(void);

    /* GPIO */
    int  (*gpio_init)(const eos_gpio_config_t *cfg);
    void (*gpio_deinit)(uint16_t pin);
    void (*gpio_write)(uint16_t pin, bool value);
    bool (*gpio_read)(uint16_t pin);
    void (*gpio_toggle)(uint16_t pin);
    int  (*gpio_set_irq)(uint16_t pin, eos_gpio_edge_t edge,
                          eos_gpio_callback_t cb, void *ctx);

    /* UART */
    int  (*uart_init)(const eos_uart_config_t *cfg);
    void (*uart_deinit)(uint8_t port);
    int  (*uart_write)(uint8_t port, const uint8_t *data, size_t len);
    int  (*uart_read)(uint8_t port, uint8_t *data, size_t len, uint32_t timeout_ms);

    /* SPI */
    int  (*spi_init)(const eos_spi_config_t *cfg);
    void (*spi_deinit)(uint8_t port);
    int  (*spi_transfer)(uint8_t port, const uint8_t *tx, uint8_t *rx, size_t len);

    /* I2C */
    int  (*i2c_init)(const eos_i2c_config_t *cfg);
    void (*i2c_deinit)(uint8_t port);
    int  (*i2c_write)(uint8_t port, uint16_t addr, const uint8_t *data, size_t len);
    int  (*i2c_read)(uint8_t port, uint16_t addr, uint8_t *data, size_t len);

    /* Timer */
    int  (*timer_init)(const eos_timer_config_t *cfg);
    void (*timer_deinit)(uint8_t timer_id);
    int  (*timer_start)(uint8_t timer_id);
    int  (*timer_stop)(uint8_t timer_id);

    /* Interrupt */
    void (*irq_disable)(void);
    void (*irq_enable)(void);
} eos_hal_backend_t;

/**
 * Register a platform HAL backend. Called by platform init code.
 */
void eos_hal_register_backend(const eos_hal_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* EOS_HAL_H */
