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
#include <stddef.h>
#include "eos/hal.h"
#include "board_esp32.h"

/* A host unit test defines ESP32_REG32 to point at fake registers. */
#ifndef ESP32_REG32
#define ESP32_REG32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))
#endif

/*
 * Read one of the five FIFO registers named by ESP32 erratum CPU-3.21 with
 * this core's interrupts masked.
 *
 * "When a CPU is interrupted while accessing five specific FIFO registers,
 * subsequent CPU accesses will get halted": if an interrupt arrives while
 * the CPU is reading UART0 0x3FF40000, UART1 0x3FF50000, UART2 0x3FF6E000,
 * I2S0 0x3FF4F004 or I2S1 0x3FF6D004, the read request is cut short and the
 * bus bridge stays waiting for it to end. Every later APB access by *either*
 * CPU (0x3FF40000-0x3FF7FFFF, 0x60000000-0x6003FFFF) is then rejected and
 * halted -- the chip stops talking to its peripherals. Affects revisions
 * v0.0 through v3.1; Espressif lists no fix scheduled. Writing to these
 * registers is unaffected, so only reads need this.
 *
 * Espressif's stated workaround is to disable CPU interrupts before the read
 * and enable them after, which is what this does.
 *
 * Only the local core is masked. The erratum is about an interrupt cutting
 * short *this* CPU's read, so no inter-core lock is required; taking the
 * kernel's would put a scheduler lock underneath the console UART. Callers
 * keep their polling loops outside this helper -- the mask covers a single
 * load, not a wait.
 *
 * Level 3 is XCHAL_EXCM_LEVEL on the ESP32 and masks every line this port
 * uses, matching eos_port_enter_critical() in
 * kernel/src/arch/xtensa_lx6/port.c. The level is only raised, never
 * lowered, so a caller already above 3 keeps its own level.
 */
static inline uint32_t eos_esp32_fifo_read(uint32_t fifo_reg)
{
#ifdef __XTENSA__
    uint32_t ps, v;
    __asm volatile ("rsr.ps %0" : "=a" (ps) :: "memory");
    if ((ps & 0xFU) < 3U) {
        __asm volatile ("rsil %0, 3" : "=a" (ps) :: "memory");
    }
    v = ESP32_REG32(fifo_reg);
    __asm volatile ("wsr.ps %0\n\trsync" :: "a" (ps) : "memory");
    return v;
#else
    /* Host builds read the test's fake registers; there is no erratum and
     * no PS register to touch. */
    return ESP32_REG32(fifo_reg);
#endif
}

/* CPU clock (CCOUNT rate) and APB clock. On the PLL, APB is 80 MHz whatever
 * the CPU runs at; on the 40 MHz crystal both run at 40 MHz (ESP32 TRM,
 * "Reset and Clock"). board_esp32.c sets the clock. */
#define ESP32_CPU_HZ (EOS_ESP32_CPU_MHZ * 1000000U)
#if EOS_ESP32_CPU_MHZ == 40U
#define ESP32_APB_HZ 40000000U
#else
#define ESP32_APB_HZ 80000000U
#endif

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

/** Give an RTC-capable pad to the RTC block for analog use (ADC, DAC,
 *  touch): RTC function 0, digital input and pulls off. -1 if @p gpio has
 *  no RTC pad. */
int  esp32_rtc_pad_analog(uint16_t gpio);

/** Set the pulls of an RTC-capable pad (GPIO0, 2, 4, 12-15, 25-27, 32-39),
 *  which live in its RTC IO register: on these pads the IO_MUX pull bits
 *  have no effect (ESP-IDF gpio_ll_pullup_en() does the same). Returns 1 if
 *  @p gpio is an RTC pad and was set, 0 if it is not one. */
int  esp32_rtc_pad_pull(uint16_t gpio, bool up, bool down);

/** GPIO through the backend (pin checks, RTC pad release) for the other
 *  ESP32 sources. */
int  esp32_gpio_configure(const eos_gpio_config_t *cfg);
void esp32_gpio_set(uint16_t pin, bool value);

/** Turn @p pin's IO_MUX pull-up on (pull-down off) or both off. */
void esp32_pin_pull_up(uint16_t pin, bool enable);

/* I2C master (hal_esp32_i2c.c) */
int  esp32_i2c_init(const eos_i2c_config_t *cfg);
void esp32_i2c_deinit(uint8_t port);
int  esp32_i2c_write(uint8_t port, uint16_t addr, const uint8_t *data, size_t len);
int  esp32_i2c_read(uint8_t port, uint16_t addr, uint8_t *data, size_t len);

/* SPI master (hal_esp32_spi.c). Through the GPIO matrix the master samples
 * MISO too early above APB/3 (26.67 MHz); the driver refuses faster clocks. */
#define ESP32_SPI_MAX_HZ (ESP32_APB_HZ / 3U + 1U)
int  esp32_spi_init(const eos_spi_config_t *cfg);
void esp32_spi_deinit(uint8_t port);
int  esp32_spi_transfer(uint8_t port, const uint8_t *tx, uint8_t *rx, size_t len);

/* CPU clock switches (board_esp32.c), for sleep. */
void esp32_clock_to_pll(uint32_t cpu_mhz);
void esp32_clock_to_xtal(void);

/** Convert a @p bits-wide raw reading of ADC @p unit (1 or 2) at 12 dB
 *  attenuation to millivolts with the chip's eFuse calibration
 *  (hal_esp32_adc_cal.c). */
uint32_t esp32_adc_raw_to_mv(int unit, uint32_t raw, uint32_t bits);

#endif /* EOS_HAL_ESP32_INTERNAL_H */
