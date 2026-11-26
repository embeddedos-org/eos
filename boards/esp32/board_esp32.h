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

/*
 * CPU clock, set by eos_esp32_early_init(): 240, 160 or 80 MHz from the PLL
 * (APB then runs at 80 MHz), or 40 to stay on the crystal as the ROM leaves
 * it (APB 40 MHz). EOS_SYS_CLOCK_HZ, which the tick is computed from, must
 * match (board_esp32.c checks). A board whose crystal is not 40 MHz stops
 * at boot unless this is 40.
 */
#ifndef EOS_ESP32_CPU_MHZ
#define EOS_ESP32_CPU_MHZ 240U
#endif

/** Disable the watchdogs the boot ROM armed and switch the CPU clock. Called by _start before .bss is
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

/*
 * SPI ports 2 (SPI2, "HSPI") and 3 (SPI3, "VSPI"): the IO_MUX pins of each,
 * routed here through the GPIO matrix. SPI3's defaults share GPIO18/19 with
 * UART1's; use one or the other, or move either with -D. Chip select is any
 * GPIO (eos_spi_config_t.cs_pin), or EOS_ESP32_SPI_NO_CS.
 */
#ifndef EOS_BOARD_SPI2_SCLK_GPIO
#define EOS_BOARD_SPI2_SCLK_GPIO 14U
#endif
#ifndef EOS_BOARD_SPI2_MOSI_GPIO
#define EOS_BOARD_SPI2_MOSI_GPIO 13U
#endif
#ifndef EOS_BOARD_SPI2_MISO_GPIO
#define EOS_BOARD_SPI2_MISO_GPIO 12U
#endif
#ifndef EOS_BOARD_SPI3_SCLK_GPIO
#define EOS_BOARD_SPI3_SCLK_GPIO 18U
#endif
#ifndef EOS_BOARD_SPI3_MOSI_GPIO
#define EOS_BOARD_SPI3_MOSI_GPIO 23U
#endif
#ifndef EOS_BOARD_SPI3_MISO_GPIO
#define EOS_BOARD_SPI3_MISO_GPIO 19U
#endif
#define EOS_ESP32_SPI_NO_CS 0xFFFFU

/** Feed SPI @p port's MISO from its own MOSI pad (ports 2 and 3, after
 *  eos_spi_init()). For self-tests; -1 for any other port. */
int  eos_esp32_spi_set_loopback(uint8_t port, bool enable);

/** SPI @p port's actual clock in Hz (the divider's nearest), 0 if the port
 *  is not initialised. */
uint32_t eos_esp32_spi_get_hz(uint8_t port);

/*
 * I2C ports 0 and 1. GPIO21/22 are the DevKit's usual I2C pins; I2C1 uses
 * GPIO33/32. Both are open drain with the internal pull-ups.
 */
#ifndef EOS_BOARD_I2C0_SDA_GPIO
#define EOS_BOARD_I2C0_SDA_GPIO 21U
#endif
#ifndef EOS_BOARD_I2C0_SCL_GPIO
#define EOS_BOARD_I2C0_SCL_GPIO 22U
#endif
#ifndef EOS_BOARD_I2C1_SDA_GPIO
#define EOS_BOARD_I2C1_SDA_GPIO 33U
#endif
#ifndef EOS_BOARD_I2C1_SCL_GPIO
#define EOS_BOARD_I2C1_SCL_GPIO 32U
#endif

/** Make I2C @p port a polled slave at 7-bit @p addr (the portable API has
 *  no slave mode). */
int  eos_esp32_i2c_slave_init(uint8_t port, uint16_t addr, uint32_t clock_hz);
/** Queue up to 32 bytes for the master to read; returns how many fit. */
int  eos_esp32_i2c_slave_queue(uint8_t port, const uint8_t *data, size_t len);
/** Take up to @p max bytes the master wrote; returns how many. */
int  eos_esp32_i2c_slave_take(uint8_t port, uint8_t *data, size_t max);
/** True once slave @p port has matched its address on the bus. */
bool eos_esp32_i2c_slave_addressed(uint8_t port);
/** Self-test without wires: feed slave @p slave's inputs from master
 *  @p master's pads, so it hears the master but cannot answer it (see
 *  hal_esp32_i2c.c). */
int  eos_esp32_i2c_slave_listen(uint8_t slave, uint8_t master);

/*
 * I2S0 (audio id 0): BCK, WS and DOUT are outputs, DIN an input (GPIO35 is
 * input-only, which suits it). GPIO25/26 are also the DAC outputs.
 */
#ifndef EOS_BOARD_I2S_BCK_GPIO
#define EOS_BOARD_I2S_BCK_GPIO  27U
#endif
#ifndef EOS_BOARD_I2S_WS_GPIO
#define EOS_BOARD_I2S_WS_GPIO   26U
#endif
#ifndef EOS_BOARD_I2S_DOUT_GPIO
#define EOS_BOARD_I2S_DOUT_GPIO 25U
#endif
#ifndef EOS_BOARD_I2S_DIN_GPIO
#define EOS_BOARD_I2S_DIN_GPIO  35U
#endif

/** Feed I2S0's serial input from its own DOUT pad (after eos_audio_init()),
 *  for self-tests. */
int  eos_esp32_i2s_set_loopback(bool enable);
/** I2S0's actual sample rate from the clock divider, 0 if not running. */
uint32_t eos_esp32_i2s_get_rate(void);

/*
 * Capacitive touch pads T0-T9 on GPIO4, 0, 2, 15, 13, 12, 14, 27, 33, 32
 * (hal_esp32_touch.c). A reading counts charge cycles in a ~4 ms window and
 * drops when a finger is on the pad. On DevKit boards T1 (GPIO0, the BOOT
 * button) and T2 (GPIO2, the LED) have parts on the pad that hold it, so
 * their readings mean little.
 */
int  eos_esp32_touch_init(void);
int  eos_esp32_touch_read(uint8_t pad, uint16_t *value);
/** GPIO of touch pad @p pad (0-9), 0xFFFF for any other. */
uint16_t eos_esp32_touch_gpio(uint8_t pad);

/*
 * Sleep with an RTC-timer wake-up (hal_esp32_sleep.c). Light sleep returns
 * 0 after waking (CCOUNT and the kernel tick stand still meanwhile); deep
 * sleep does not return: the chip resets on wake-up, with
 * eos_esp32_reset_reason() 5 (DEEPSLEEP_RESET). Both return -1 if the sleep
 * could not start. EOS_ESP32_RTC_NOINIT places a variable in RTC slow
 * memory, which keeps its value through deep sleep; after power-on it holds
 * garbage, so keep a tag next to it.
 */
#define EOS_ESP32_RTC_NOINIT __attribute__((section(".rtc.noinit")))
#define EOS_ESP32_RESET_POWERON    1U
#define EOS_ESP32_RESET_DEEPSLEEP  5U
int      eos_esp32_light_sleep_us(uint64_t us);
int      eos_esp32_deep_sleep_us(uint64_t us);
uint32_t eos_esp32_reset_reason(void);
/** Wake-up sources that fired (RTC_CNTL wakeup_cause; bit 3 is the timer). */
uint32_t eos_esp32_wakeup_cause(void);
/** RTC timer (slow clock ticks, 48 bits): runs through every sleep. */
uint64_t eos_esp32_rtc_time(void);
/** RTC slow clock rate in Hz, measured against CCOUNT. */
uint32_t eos_esp32_rtc_slow_hz(void);
/** Mask interrupts (to level 3, the port's critical level); returns the
 *  previous PS for eos_esp32_irq_restore(). */
uint32_t eos_esp32_irq_save(void);
void     eos_esp32_irq_restore(uint32_t ps);

/** Connect UART @p port's TX to its own RX inside the peripheral (ports 1
 *  and 2, after eos_uart_init()). For self-tests; -1 for any other port. */
int  eos_esp32_uart_set_loopback(uint8_t port, bool enable);

/** Where eos_adc_read_mv()'s calibration comes from (hal_esp32_adc_cal.c). */
typedef enum {
    EOS_ESP32_ADC_CAL_TWO_POINT    = 0,  /* BLOCK3 readings at 150/850 mV */
    EOS_ESP32_ADC_CAL_EFUSE_VREF   = 1,  /* the chip's measured Vref */
    EOS_ESP32_ADC_CAL_DEFAULT_VREF = 2,  /* nothing burned: nominal 1100 mV */
} eos_esp32_adc_cal_t;

/** Report the ADC calibration source and, through @p vref_mv (may be NULL),
 *  the Vref in use. */
eos_esp32_adc_cal_t eos_esp32_adc_cal_source(uint32_t *vref_mv);

/** Read the raw CPU cycle counter (CCOUNT). */
uint32_t eos_esp32_ccount(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_BOARD_ESP32_H */
