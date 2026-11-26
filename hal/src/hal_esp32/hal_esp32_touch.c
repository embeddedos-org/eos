// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file hal_esp32_touch.c
 * @brief ESP32 capacitive touch pads T0-T9 (board API, board_esp32.h)
 *
 * The extended HAL's touch API describes a touch panel (coordinates over
 * I2C), not touch pads, so the ESP32's ten pads are a board API:
 * eos_esp32_touch_init() and eos_esp32_touch_read(). A reading is the number
 * of charge/discharge cycles the pad completes in a fixed window (0x7FFF
 * cycles of the 8 MHz clock, about 4 ms); a finger adds capacitance and
 * lowers it.
 *
 * Software-started measurement, configured as ESP-IDF v5.4's legacy driver
 * does (driver/touch_sensor/esp32/touch_sensor.c, hal/esp32/touch_sensor_hal.c
 * and touch_sensor_ll.h): measure time 0x7FFF, slope 7, tie option float,
 * pad on the RTC mux with input and pulls off. The ESP32 swaps T8 and T9 in
 * some registers (TOUCH_LL_BITS_SWAP, touch_ll_num_wrap); so does this.
 * Nothing from ESP-IDF is linked.
 */

#include "board_esp32.h"
#include "hal_esp32_internal.h"

#define REG32 ESP32_REG32

#define SENS_SAR_TOUCH_CTRL1   0x3FF48858U
#define SENS_SAR_TOUCH_OUT1    0x3FF48870U
#define SENS_SAR_TOUCH_CTRL2   0x3FF48884U
#define SENS_SAR_TOUCH_ENABLE  0x3FF4888CU
#define RTC_IO_TOUCH_PAD0      0x3FF48494U
#define RTC_CNTL_STATE0        0x3FF48018U

#define CTRL2_MEAS_DONE        (1U << 10)
#define CTRL2_START_FSM_EN     (1U << 11)
#define CTRL2_START_EN         (1U << 12)
#define CTRL2_START_FORCE      (1U << 13)   /* software start */
#define CTRL2_SLEEP_S          14
#define TOUCH_SLP_TIMER_EN     (1U << 23)
#define PAD_DAC_S              23           /* slope */
#define PAD_TIE_OPT            (1U << 21)
#define PAD_XPD                (1U << 20)
#define MEAS_CYCLES            0x7FFFU
#define XPD_WAIT               0xFFU

static const uint8_t s_touch_gpio[10] = { 4, 0, 2, 15, 13, 12, 14, 27, 33, 32 };
static bool s_touch_ready;

/* T8 and T9 trade places in the enable masks and the result registers. */
static uint32_t wrap(uint32_t pad) { return pad == 8U ? 9U : pad == 9U ? 8U : pad; }

int eos_esp32_touch_init(void)
{
    REG32(RTC_CNTL_STATE0) &= ~TOUCH_SLP_TIMER_EN;          /* FSM stopped */
    REG32(SENS_SAR_TOUCH_ENABLE) = 0;                        /* no pad enabled */
    REG32(SENS_SAR_TOUCH_CTRL1) = (REG32(SENS_SAR_TOUCH_CTRL1) & ~0x00FFFFFFU) |
                                  (XPD_WAIT << 16) | MEAS_CYCLES;
    uint32_t c2 = REG32(SENS_SAR_TOUCH_CTRL2);
    c2 &= ~((0xFFFFU << CTRL2_SLEEP_S) | CTRL2_START_EN);
    c2 |= (0x1000U << CTRL2_SLEEP_S) | CTRL2_START_FSM_EN | CTRL2_START_FORCE;
    REG32(SENS_SAR_TOUCH_CTRL2) = c2;
    REG32(RTC_CNTL_STATE0) |= TOUCH_SLP_TIMER_EN;            /* FSM running */
    for (uint32_t pad = 0; pad < 10U; pad++) {
        if (esp32_rtc_pad_analog(s_touch_gpio[pad]) != 0) return -1;
        uint32_t r = RTC_IO_TOUCH_PAD0 + 4U * pad;           /* slope: not swapped */
        REG32(r) = (REG32(r) & ~(7U << PAD_DAC_S)) | (7U << PAD_DAC_S);
        uint32_t t = RTC_IO_TOUCH_PAD0 + 4U * wrap(pad);     /* tie option: swapped */
        REG32(t) &= ~(PAD_XPD | PAD_TIE_OPT);                /* float */
    }
    s_touch_ready = true;
    return 0;
}

int eos_esp32_touch_read(uint8_t pad, uint16_t *value)
{
    if (!s_touch_ready || pad > 9U || value == NULL) return -1;
    uint32_t bit = 1U << wrap(pad);
    /* worken [9:0], outen2 [19:10], outen1 [29:20] */
    REG32(SENS_SAR_TOUCH_ENABLE) = bit | (bit << 10) | (bit << 20);
    REG32(SENS_SAR_TOUCH_CTRL2) &= ~CTRL2_START_EN;
    REG32(SENS_SAR_TOUCH_CTRL2) |= CTRL2_START_EN;
    /* One measurement is 0x7FFF / 8 MHz = 4.1 ms; give up after 20 ms. */
    uint32_t t0 = eos_esp32_ccount();
    while ((REG32(SENS_SAR_TOUCH_CTRL2) & CTRL2_MEAS_DONE) == 0U) {
        if (eos_esp32_ccount() - t0 > ESP32_CPU_HZ / 50U) {
            REG32(SENS_SAR_TOUCH_ENABLE) = 0;
            return -1;
        }
    }
    uint32_t w = wrap(pad);
    uint32_t out = REG32(SENS_SAR_TOUCH_OUT1 + 4U * (w / 2U));
    *value = (uint16_t)((w & 1U) ? (out & 0xFFFFU) : (out >> 16));
    REG32(SENS_SAR_TOUCH_ENABLE) = 0;
    return 0;
}

uint16_t eos_esp32_touch_gpio(uint8_t pad)
{
    return pad <= 9U ? s_touch_gpio[pad] : 0xFFFFU;
}
