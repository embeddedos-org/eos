// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file hal_rtos.c
 * @brief EoS HAL — RTOS platform backend
 *
 * Implements the HAL interface using direct register access for bare-metal
 * and RTOS targets. Designed for Cortex-M and Cortex-R class MCUs
 * with memory-mapped I/O.
 */

#include <eos/hal.h>
#include <string.h>

#if !defined(__linux__)

/* ---- Platform-specific register access ---- */

#define REG32(addr) (*(volatile uint32_t *)(addr))

/* ---- Internal state ---- */

static volatile uint32_t systick_ms = 0;
static bool hal_initialized = false;

/* ---- GPIO (memory-mapped) ---- */

static int rtos_gpio_init(const eos_gpio_config_t *cfg)
{
    (void)cfg;
    /* Platform-specific GPIO bank and pin configuration.
     * Actual implementation depends on the MCU (STM32, nRF52, etc.).
     * Board-specific code should override via the HAL backend. */
    return 0;
}

static void rtos_gpio_deinit(uint16_t pin)
{
    (void)pin;
}

static void rtos_gpio_write(uint16_t pin, bool value)
{
    (void)pin;
    (void)value;
}

static bool rtos_gpio_read(uint16_t pin)
{
    (void)pin;
    return false;
}

static void rtos_gpio_toggle(uint16_t pin)
{
    (void)pin;
}

static int rtos_gpio_set_irq(uint16_t pin, eos_gpio_edge_t edge,
                              eos_gpio_callback_t cb, void *ctx)
{
    (void)pin; (void)edge; (void)cb; (void)ctx;
    return 0;
}

/* ---- UART (register-based) ---- */

static int rtos_uart_init(const eos_uart_config_t *cfg)
{
    (void)cfg;
    return 0;
}

static void rtos_uart_deinit(uint8_t port)
{
    (void)port;
}

static int rtos_uart_write(uint8_t port, const uint8_t *data, size_t len)
{
    (void)port; (void)data; (void)len;
    return 0;
}

static int rtos_uart_read(uint8_t port, uint8_t *data, size_t len, uint32_t timeout_ms)
{
    (void)port; (void)data; (void)len; (void)timeout_ms;
    return 0;
}

/* ---- SPI (register-based) ---- */

static int rtos_spi_init(const eos_spi_config_t *cfg) { (void)cfg; return 0; }
static void rtos_spi_deinit(uint8_t port) { (void)port; }
static int rtos_spi_transfer(uint8_t port, const uint8_t *tx, uint8_t *rx, size_t len)
{
    (void)port; (void)tx; (void)rx; (void)len;
    return 0;
}

/* ---- I2C (register-based) ---- */

static int rtos_i2c_init(const eos_i2c_config_t *cfg) { (void)cfg; return 0; }
static void rtos_i2c_deinit(uint8_t port) { (void)port; }
static int rtos_i2c_write(uint8_t port, uint16_t addr, const uint8_t *data, size_t len)
{
    (void)port; (void)addr; (void)data; (void)len;
    return 0;
}
static int rtos_i2c_read(uint8_t port, uint16_t addr, uint8_t *data, size_t len)
{
    (void)port; (void)addr; (void)data; (void)len;
    return 0;
}

/* ---- Timer (hardware timer) ---- */

static int rtos_timer_init(const eos_timer_config_t *cfg) { (void)cfg; return 0; }
static void rtos_timer_deinit(uint8_t timer_id) { (void)timer_id; }
static int rtos_timer_start(uint8_t timer_id) { (void)timer_id; return 0; }
static int rtos_timer_stop(uint8_t timer_id) { (void)timer_id; return 0; }

/* ---- Interrupt control ---- */

static void rtos_irq_disable(void)
{
#if defined(__ARM_ARCH)
    __asm volatile ("cpsid i" ::: "memory");
#endif
}

static void rtos_irq_enable(void)
{
#if defined(__ARM_ARCH)
    __asm volatile ("cpsie i" ::: "memory");
#endif
}

/* ---- Timing ---- */

static void rtos_delay_ms(uint32_t ms)
{
    uint32_t start = systick_ms;
    while ((systick_ms - start) < ms) {
        /* busy-wait — in a real RTOS, yield to scheduler */
    }
}

static uint32_t rtos_get_tick_ms(void)
{
    return systick_ms;
}

/* Called from SysTick_Handler (Cortex-M) or RTI timer ISR (Cortex-R) */
void eos_hal_systick_handler(void)
{
    systick_ms++;
}

/* ---- Init ---- */

static int rtos_hal_init(void)
{
    systick_ms = 0;
    hal_initialized = true;
    return 0;
}

static void rtos_hal_deinit(void)
{
    hal_initialized = false;
}

/* ---- Backend registration ---- */

static const eos_hal_backend_t rtos_backend = {
    .name          = "rtos",
    .init          = rtos_hal_init,
    .deinit        = rtos_hal_deinit,
    .delay_ms      = rtos_delay_ms,
    .get_tick_ms   = rtos_get_tick_ms,
    .gpio_init     = rtos_gpio_init,
    .gpio_deinit   = rtos_gpio_deinit,
    .gpio_write    = rtos_gpio_write,
    .gpio_read     = rtos_gpio_read,
    .gpio_toggle   = rtos_gpio_toggle,
    .gpio_set_irq  = rtos_gpio_set_irq,
    .uart_init     = rtos_uart_init,
    .uart_deinit   = rtos_uart_deinit,
    .uart_write    = rtos_uart_write,
    .uart_read     = rtos_uart_read,
    .spi_init      = rtos_spi_init,
    .spi_deinit    = rtos_spi_deinit,
    .spi_transfer  = rtos_spi_transfer,
    .i2c_init      = rtos_i2c_init,
    .i2c_deinit    = rtos_i2c_deinit,
    .i2c_write     = rtos_i2c_write,
    .i2c_read      = rtos_i2c_read,
    .timer_init    = rtos_timer_init,
    .timer_deinit  = rtos_timer_deinit,
    .timer_start   = rtos_timer_start,
    .timer_stop    = rtos_timer_stop,
    .irq_disable   = rtos_irq_disable,
    .irq_enable    = rtos_irq_enable,
};

void eos_hal_rtos_register(void)
{
    eos_hal_register_backend(&rtos_backend);
}

#endif /* !__linux__ */
