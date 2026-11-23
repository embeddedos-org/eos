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

/** Disable the watchdogs the boot ROM armed. Called by _start before .bss is
 *  zeroed, so it must not use .bss. */
void eos_esp32_early_init(void);

/** Blocking console output on UART0. */
void eos_esp32_console_write(const char *s, size_t len);
void eos_esp32_console_puts(const char *s);
void eos_esp32_console_put_u32(uint32_t v);

/** Read the raw CPU cycle counter (CCOUNT). */
uint32_t eos_esp32_ccount(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_BOARD_ESP32_H */
