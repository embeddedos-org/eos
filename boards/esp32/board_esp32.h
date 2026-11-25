// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file board_esp32.h
 * @brief ESP32 (ESP-WROOM-32 DevKit) board support for EoS
 *
 * Register addresses are from ESP-IDF v5.4, components/soc/esp32/register/soc
 * (reg_base.h, uart_reg.h, gpio_reg.h, io_mux_reg.h, timer_group_reg.h,
 * rtc_cntl_reg.h) and components/hal/esp32/include/hal/rwdt_ll.h.
 */

#ifndef EOS_BOARD_ESP32_H
#define EOS_BOARD_ESP32_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * On-board user LED. HiLetGo / DOIT-style ESP-WROOM-32 DevKit boards wire a
 * blue LED to GPIO2. Espressif's own DevKitC V4 has no user LED, and the EoS
 * ESP32 DevKit (eCAD-Hardware-Products) puts it on GPIO4: build with
 * -DEOS_BOARD_LED_GPIO=4 for that board.
 */
#ifndef EOS_BOARD_LED_GPIO
#define EOS_BOARD_LED_GPIO  2U
#endif

/* UART0 is the CP210x USB-serial port; the boot ROM has already set it to
 * 115200 8N1. */
#define EOS_BOARD_CONSOLE_UART  0U

/* UART1 and UART2 pins, routed through the GPIO matrix. UART2 uses the DOIT /
 * HiLetGo DevKit's labelled RX2/TX2 (GPIO16/17). UART1's own IO_MUX pins are
 * the module's flash, so it is moved to GPIO19/18, which are not analog pins. */
#ifndef EOS_BOARD_UART1_TX_GPIO
#define EOS_BOARD_UART1_TX_GPIO 19U
#endif
#ifndef EOS_BOARD_UART1_RX_GPIO
#define EOS_BOARD_UART1_RX_GPIO 18U
#endif
#ifndef EOS_BOARD_UART2_TX_GPIO
#define EOS_BOARD_UART2_TX_GPIO 17U
#endif
#ifndef EOS_BOARD_UART2_RX_GPIO
#define EOS_BOARD_UART2_RX_GPIO 16U
#endif

/** Disable the watchdogs the boot ROM armed. Called by _start before .bss is
 *  zeroed, so it must not use .bss. */
void eos_esp32_early_init(void);

/** Blocking console output on UART0. */
void eos_esp32_console_write(const char *s, size_t len);
void eos_esp32_console_puts(const char *s);
void eos_esp32_console_put_u32(uint32_t v);

/*
 * PWM channel n (LEDC high-speed channel n) drives EOS_BOARD_PWM_GPIO[n].
 * Channel 0 is the LED. Override the whole list with
 * -DEOS_BOARD_PWM_GPIOS="{...}" (eight entries).
 */
#ifndef EOS_BOARD_PWM_GPIOS
#define EOS_BOARD_PWM_GPIOS { 2U, 4U, 5U, 13U, 14U, 15U, 21U, 22U }
#endif

/** Connect UART @p port's TX to its own RX inside the peripheral (ports 1
 *  and 2, after eos_uart_init()). For self-tests; -1 for any other port. */
int  eos_esp32_uart_set_loopback(uint8_t port, bool enable);

/** Read the raw CPU cycle counter (CCOUNT). */
uint32_t eos_esp32_ccount(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_BOARD_ESP32_H */
