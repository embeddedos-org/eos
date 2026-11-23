// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file hal_esp32_internal.h
 * @brief Shared helpers of the ESP32 HAL sources (not a public API)
 *
 * Register addresses are from ESP-IDF v5.4, components/soc/esp32/register/soc.
 */

#ifndef EOS_HAL_ESP32_INTERNAL_H
#define EOS_HAL_ESP32_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include "board_esp32.h"

/* A host unit test defines ESP32_REG32 to point at fake registers. */
#ifndef ESP32_REG32
#define ESP32_REG32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))
#endif

/* APB clock. The port leaves the CPU on the 40 MHz crystal the boot ROM
 * selects, and with the CPU on the crystal APB runs at the same 40 MHz. */
#define ESP32_APB_HZ 40000000U

/* DPORT peripheral clock gates and resets (dport_reg.h) */
#define ESP32_DPORT_PERIP_CLK_EN  0x3FF000C0U
#define ESP32_DPORT_PERIP_RST_EN  0x3FF000C4U

/* Enable a peripheral's clock and take it out of reset. */
static inline void esp32_periph_enable(uint32_t bit)
{
    ESP32_REG32(ESP32_DPORT_PERIP_CLK_EN) |= bit;
    ESP32_REG32(ESP32_DPORT_PERIP_RST_EN) &= ~bit;
}

/** True if @p pin may be configured: it exists on the module and is not
 *  the console (GPIO1/3) or the module's flash (GPIO6-11). */
bool esp32_pin_usable(uint16_t pin);

/** Route peripheral output signal @p signal (gpio_sig_map.h) to @p pin
 *  through the GPIO matrix and enable the pad's output driver. */
int  esp32_pin_route_output(uint16_t pin, uint32_t signal);

/** Feed @p pin into peripheral input signal @p signal through the matrix. */
int  esp32_pin_route_input(uint16_t pin, uint32_t signal);

/** Hand an RTC-capable pad back to the digital IO_MUX (no-op for other
 *  pins). An ADC or DAC leaves the pad on the RTC mux, which disconnects the
 *  digital GPIO and matrix paths until this clears it. */
void esp32_rtc_pad_release(uint16_t gpio);

/** Convert a @p bits-wide raw reading of ADC @p unit (1 or 2) at 12 dB
 *  attenuation to millivolts with the chip's eFuse calibration
 *  (hal_esp32_adc_cal.c). */
uint32_t esp32_adc_raw_to_mv(int unit, uint32_t raw, uint32_t bits);

#endif /* EOS_HAL_ESP32_INTERNAL_H */
