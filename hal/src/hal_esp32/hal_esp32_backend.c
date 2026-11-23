// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file hal_esp32_backend.c
 * @brief ESP32 HAL backend: GPIO and UART0
 *
 * Fills the GPIO and UART slots of eos_hal_backend_t; the rest stay NULL, and
 * hal_common.c reports -1 for them rather than pretending to succeed. Register
 * addresses are from ESP-IDF v5.4 (components/soc/esp32/register/soc).
 */

#include <eos/hal.h>
#include "board_esp32.h"
#include "hal_esp32_internal.h"

#define REG32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))

#define GPIO_BASE             0x3FF44000U
#define GPIO_OUT_REG          (GPIO_BASE + 0x0004U)
#define GPIO_OUT_W1TS         (GPIO_BASE + 0x0008U)
#define GPIO_OUT_W1TC         (GPIO_BASE + 0x000CU)
#define GPIO_OUT1_REG         (GPIO_BASE + 0x0010U)
#define GPIO_OUT1_W1TS        (GPIO_BASE + 0x0014U)
#define GPIO_OUT1_W1TC        (GPIO_BASE + 0x0018U)
#define GPIO_ENABLE_W1TS      (GPIO_BASE + 0x0024U)
#define GPIO_ENABLE_W1TC      (GPIO_BASE + 0x0028U)
#define GPIO_ENABLE1_W1TS     (GPIO_BASE + 0x0030U)
#define GPIO_ENABLE1_W1TC     (GPIO_BASE + 0x0034U)
#define GPIO_IN_REG           (GPIO_BASE + 0x003CU)
#define GPIO_IN1_REG          (GPIO_BASE + 0x0040U)
#define GPIO_FUNC_OUT_SEL(n)  (GPIO_BASE + 0x0530U + 4U * (n))
#define SIG_GPIO_OUT_IDX      256U   /* gpio_sig_map.h: plain GPIO output */

#define IO_MUX_BASE           0x3FF49000U
#define FUN_PD                (1U << 7)
#define FUN_PU                (1U << 8)
#define FUN_IE                (1U << 9)
#define MCU_SEL_S             12
#define MCU_SEL_M             (7U << MCU_SEL_S)
#define PIN_FUNC_GPIO         2U

#define UART0_FIFO_AHB        0x60000000U
#define UART0_STATUS          0x3FF4001CU
#define UART_RXFIFO_CNT(st)   ((st) & 0xFFU)

#define ESP32_GPIO_COUNT      40U

/*
 * IO_MUX register offset for each GPIO (io_mux_reg.h, PERIPHS_IO_MUX_*_U, in
 * the order of ESP-IDF's GPIO_PIN_MUX_REG table). 0 means the GPIO does not
 * exist on the ESP32-D0WD in an ESP-WROOM-32 module.
 */
static const uint8_t s_iomux_offset[ESP32_GPIO_COUNT] = {
    0x44, 0x88, 0x40, 0x84, 0x48, 0x6C, 0x60, 0x64,   /*  0- 7 */
    0x68, 0x54, 0x58, 0x5C, 0x34, 0x38, 0x30, 0x3C,   /*  8-15 */
    0x4C, 0x50, 0x70, 0x74, 0x00, 0x7C, 0x80, 0x8C,   /* 16-23 (20: PICO-V3 only) */
    0x00, 0x24, 0x28, 0x2C, 0x00, 0x00, 0x00, 0x00,   /* 24-31 */
    0x1C, 0x20, 0x14, 0x18, 0x04, 0x08, 0x0C, 0x10,   /* 32-39 */
};

/*
 * Pins the backend refuses: GPIO1 and GPIO3 are UART0, the console; GPIO6-11
 * are the module's SPI flash, and reconfiguring them stops the chip fetching
 * from flash.
 */
static int gpio_reserved(uint16_t pin)
{
    return pin == 1U || pin == 3U || (pin >= 6U && pin <= 11U);
}

bool esp32_pin_usable(uint16_t pin)
{
    return pin < ESP32_GPIO_COUNT && s_iomux_offset[pin] != 0U && !gpio_reserved(pin);
}

#define GPIO_FUNC_IN_SEL(sig) (GPIO_BASE + 0x0130U + 4U * (sig))
#define GPIO_SIG_IN_SEL       (1U << 7)   /* route through the matrix, not IO_MUX */

int esp32_pin_route_output(uint16_t pin, uint32_t signal)
{
    if (!esp32_pin_usable(pin) || pin >= 34U || signal > SIG_GPIO_OUT_IDX) return -1;
    uint32_t mux = REG32(IO_MUX_BASE + s_iomux_offset[pin]);
    mux &= ~(MCU_SEL_M | FUN_PU | FUN_PD);
    mux |= (PIN_FUNC_GPIO << MCU_SEL_S) | FUN_IE;   /* IE: the pad can be read back */
    REG32(IO_MUX_BASE + s_iomux_offset[pin]) = mux;
    REG32(GPIO_FUNC_OUT_SEL(pin)) = signal;
    if (pin < 32U) REG32(GPIO_ENABLE_W1TS) = 1U << pin;
    else           REG32(GPIO_ENABLE1_W1TS) = 1U << (pin - 32U);
    return 0;
}

int esp32_pin_route_input(uint16_t pin, uint32_t signal)
{
    if (!esp32_pin_usable(pin) || signal >= SIG_GPIO_OUT_IDX) return -1;
    uint32_t mux = REG32(IO_MUX_BASE + s_iomux_offset[pin]);
    mux &= ~MCU_SEL_M;
    mux |= (PIN_FUNC_GPIO << MCU_SEL_S) | FUN_IE;
    REG32(IO_MUX_BASE + s_iomux_offset[pin]) = mux;
    REG32(GPIO_FUNC_IN_SEL(signal)) = GPIO_SIG_IN_SEL | pin;
    return 0;
}

static int esp32_gpio_init(const eos_gpio_config_t *cfg)
{
    if (cfg == NULL || !esp32_pin_usable(cfg->pin)) return -1;
    uint16_t pin = cfg->pin;

    if (cfg->mode != EOS_GPIO_INPUT && cfg->mode != EOS_GPIO_OUTPUT) return -1;
    /* GPIO34-39 are input-only pads with no internal pull resistors: an
     * output, or a pull that would silently do nothing, is refused. */
    if (pin >= 34U && (cfg->mode == EOS_GPIO_OUTPUT || cfg->pull != EOS_GPIO_PULL_NONE)) {
        return -1;
    }

    uint32_t mux = REG32(IO_MUX_BASE + s_iomux_offset[pin]);
    mux &= ~(MCU_SEL_M | FUN_PU | FUN_PD);
    mux |= (PIN_FUNC_GPIO << MCU_SEL_S) | FUN_IE;   /* IE: GPIO_IN reads the pad */
    if (cfg->pull == EOS_GPIO_PULL_UP) mux |= FUN_PU;
    else if (cfg->pull == EOS_GPIO_PULL_DOWN) mux |= FUN_PD;
    REG32(IO_MUX_BASE + s_iomux_offset[pin]) = mux;

    if (cfg->mode == EOS_GPIO_OUTPUT) {
        REG32(GPIO_FUNC_OUT_SEL(pin)) = SIG_GPIO_OUT_IDX;
        if (pin < 32U) REG32(GPIO_ENABLE_W1TS) = 1U << pin;
        else           REG32(GPIO_ENABLE1_W1TS) = 1U << (pin - 32U);
    } else {
        if (pin < 32U) REG32(GPIO_ENABLE_W1TC) = 1U << pin;
        else           REG32(GPIO_ENABLE1_W1TC) = 1U << (pin - 32U);
    }
    return 0;
}

static void esp32_gpio_write(uint16_t pin, bool value)
{
    if (!esp32_pin_usable(pin) || pin >= 34U) return;
    if (pin < 32U) REG32(value ? GPIO_OUT_W1TS : GPIO_OUT_W1TC) = 1U << pin;
    else           REG32(value ? GPIO_OUT1_W1TS : GPIO_OUT1_W1TC) = 1U << (pin - 32U);
}

static bool esp32_gpio_read(uint16_t pin)
{
    if (!esp32_pin_usable(pin)) return false;
    if (pin < 32U) return (REG32(GPIO_IN_REG) >> pin) & 1U;
    return (REG32(GPIO_IN1_REG) >> (pin - 32U)) & 1U;
}

static void esp32_gpio_toggle(uint16_t pin)
{
    if (!esp32_pin_usable(pin) || pin >= 34U) return;
    /* Toggle the output latch, not the pad: an external load cannot make
     * this flip the wrong way. */
    bool on = (pin < 32U) ? ((REG32(GPIO_OUT_REG) >> pin) & 1U)
                          : ((REG32(GPIO_OUT1_REG) >> (pin - 32U)) & 1U);
    esp32_gpio_write(pin, !on);
}

static int esp32_uart_init(const eos_uart_config_t *cfg)
{
    /* The ROM has configured UART0 for 115200 8N1 from the 40 MHz APB clock,
     * and this backend does not reprogram it. Anything else is refused rather
     * than silently left at 115200. */
    if (cfg == NULL || cfg->port != EOS_BOARD_CONSOLE_UART) return -1;
    if (cfg->baudrate != 115200U || cfg->data_bits != 8U) return -1;
    if (cfg->parity != EOS_UART_PARITY_NONE || cfg->stop_bits != EOS_UART_STOP_1) return -1;
    return 0;
}

static int esp32_uart_write(uint8_t port, const uint8_t *data, size_t len)
{
    if (port != EOS_BOARD_CONSOLE_UART || (data == NULL && len != 0U)) return -1;
    eos_esp32_console_write((const char *)data, len);
    return (int)len;
}

static int esp32_uart_read(uint8_t port, uint8_t *data, size_t len, uint32_t timeout_ms)
{
    if (port != EOS_BOARD_CONSOLE_UART || (data == NULL && len != 0U)) return -1;
    /* CCOUNT runs at the 40 MHz the ROM leaves the CPU on and wraps every
     * 107 s, so elapsed time is accumulated from wrap-safe deltas rather than
     * measured from the start: any timeout_ms up to UINT32_MAX expires. */
    const uint32_t cycles_per_ms = 40000U;
    uint32_t last = eos_esp32_ccount();
    uint32_t cycles = 0;
    uint32_t elapsed_ms = 0;
    size_t got = 0;
    while (got < len) {
        if (UART_RXFIFO_CNT(REG32(UART0_STATUS)) != 0U) {
            data[got++] = (uint8_t)REG32(UART0_FIFO_AHB);
            continue;
        }
        uint32_t now = eos_esp32_ccount();
        cycles += now - last;
        last = now;
        while (cycles >= cycles_per_ms) {
            cycles -= cycles_per_ms;
            elapsed_ms++;
        }
        if (elapsed_ms >= timeout_ms) break;
    }
    return (int)got;
}

static int esp32_hal_backend_init(void)
{
    return 0;
}

static const eos_hal_backend_t s_esp32_backend = {
    .name        = "esp32",
    .init        = esp32_hal_backend_init,
    .gpio_init   = esp32_gpio_init,
    .gpio_write  = esp32_gpio_write,
    .gpio_read   = esp32_gpio_read,
    .gpio_toggle = esp32_gpio_toggle,
    .uart_init   = esp32_uart_init,
    .uart_write  = esp32_uart_write,
    .uart_read   = esp32_uart_read,
};

/* Selected by eos_hal_init() when built with -DEOS_HAL_BACKEND_ESP32. */
void eos_hal_esp32_register(void)
{
    eos_hal_register_backend(&s_esp32_backend);
}
