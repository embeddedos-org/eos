// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2015-2023 Espressif Systems (Shanghai) CO LTD
// Copyright (c) 2026 EoS Project

/**
 * @file hal_esp32_adc_cal.c
 * @brief ESP32 ADC raw-to-millivolt conversion from the chip's eFuse calibration
 *
 * A port of ESP-IDF v5.4 components/esp_adc/esp32/adc_cali_line_fitting.c
 * (Apache-2.0, hence this file's licence): the characterisation constants,
 * the eFuse decoding and the 12 dB lookup table are Espressif's; the code
 * around them is rewritten for EoS (no heap, no ESP-IDF headers).
 *
 * Calibration source, in ESP-IDF's order of preference:
 *   1. Two-point values (ADC readings at 150 mV and 850 mV) in BLOCK3, valid
 *      only when BLK3_PART_RESERVE is burned.
 *   2. The chip's measured Vref in BLOCK0 (most chips since 2018).
 *   3. The nominal 1100 mV Vref.
 * At 12 dB attenuation, readings of 2880 and above use the lookup table
 * (ESP-IDF's CONFIG_ADC_CALI_LUT_ENABLE, on by default), because the
 * ADC is non-linear at the top of that range.
 */

#include <stdint.h>
#include "hal_esp32_internal.h"

#define EFUSE_BLK0_RDATA3   0x3FF5A00CU
#define EFUSE_BLK0_RDATA4   0x3FF5A010U
#define EFUSE_BLK3_RDATA3   0x3FF5A084U
#define BLK3_PART_RESERVE   (1U << 14)   /* in BLK0_RDATA3 */

#define VREF_OFFSET         1100
#define VREF_STEP           7
#define TP_LOW1_OFFSET      278
#define TP_LOW2_OFFSET      421
#define TP_HIGH1_OFFSET     3265
#define TP_HIGH2_OFFSET     3406
#define TP_STEP             4
#define TP_LOW_MV           150U
#define TP_HIGH_MV          850U

#define COEFF_A_SCALE       65536U
#define LUT_VREF_LOW        1000
#define LUT_VREF_HIGH       1200
#define LUT_STEP            64
#define LUT_POINTS          20
#define LUT_LOW_THRESH      2880U
#define LUT_HIGH_THRESH     (LUT_LOW_THRESH + LUT_STEP)

/* Index [unit-1][atten]; this port only uses atten 3 (12 dB), the others are
 * kept so the table reads as ESP-IDF's does. */
static const uint32_t s_tp_scale[2][4]   = { { 65504, 86975, 120389, 224310 },
                                             { 65467, 86861, 120416, 224708 } };
static const uint32_t s_tp_offset[2][4]  = { { 0, 1, 27, 54 }, { 0, 9, 26, 66 } };
static const uint32_t s_vref_scale[2][4] = { { 57431, 76236, 105481, 196602 },
                                             { 57236, 76175, 105678, 197170 } };
static const uint32_t s_vref_offset[2][4] = { { 75, 78, 107, 142 }, { 63, 66, 89, 128 } };

/* 12 dB, readings 2880..4096 in steps of 64, for Vref 1000 mV (low) and
 * 1200 mV (high). */
static const uint16_t s_lut_low[2][LUT_POINTS] = {
    { 2240, 2297, 2352, 2405, 2457, 2512, 2564, 2616, 2664, 2709,
      2754, 2795, 2832, 2868, 2903, 2937, 2969, 3000, 3030, 3060 },
    { 2238, 2293, 2347, 2399, 2451, 2507, 2561, 2613, 2662, 2710,
      2754, 2792, 2831, 2869, 2904, 2937, 2968, 2999, 3029, 3059 },
};
static const uint16_t s_lut_high[2][LUT_POINTS] = {
    { 2667, 2706, 2745, 2780, 2813, 2844, 2873, 2901, 2928, 2956,
      2982, 3006, 3032, 3059, 3084, 3110, 3135, 3160, 3184, 3209 },
    { 2657, 2698, 2738, 2774, 2807, 2838, 2867, 2894, 2921, 2946,
      2971, 2996, 3020, 3043, 3067, 3092, 3116, 3139, 3162, 3185 },
};

/* eFuse fields hold sign-magnitude (Vref) or two's complement (two-point)
 * values whose sign bit is the top bit of the field. */
static int32_t decode(uint32_t bits, uint32_t mask, int twos_compl)
{
    uint32_t mag = bits & (mask >> 1);
    if ((bits & ~(mask >> 1) & mask) == 0U) return (int32_t)mag;
    if (twos_compl) return -(int32_t)((~bits + 1U) & (mask >> 1));
    return -(int32_t)mag;
}

static uint32_t efuse_field(uint32_t reg, uint32_t shift, uint32_t mask)
{
    return (ESP32_REG32(reg) >> shift) & mask;
}

typedef struct {
    uint32_t coeff_a;          /* mV per count, scaled by 65536 */
    uint32_t coeff_b;          /* mV */
    uint32_t vref;             /* mV, for the lookup table */
    eos_esp32_adc_cal_t source;
} adc_line_t;

static adc_line_t s_line[2];
static int s_ready;

static int two_point_burned(void)
{
    if ((ESP32_REG32(EFUSE_BLK0_RDATA3) & BLK3_PART_RESERVE) == 0U) return 0;
    return efuse_field(EFUSE_BLK3_RDATA3, 0, 0x7F) != 0U &&
           efuse_field(EFUSE_BLK3_RDATA3, 7, 0x1FF) != 0U &&
           efuse_field(EFUSE_BLK3_RDATA3, 16, 0x7F) != 0U &&
           efuse_field(EFUSE_BLK3_RDATA3, 23, 0x1FF) != 0U;
}

static void characterise(void)
{
    uint32_t vref_bits = efuse_field(EFUSE_BLK0_RDATA4, 8, 0x1F);
    uint32_t vref = (uint32_t)(VREF_OFFSET + decode(vref_bits, 0x1F, 0) * VREF_STEP);
    int tp = two_point_burned();

    for (int u = 0; u < 2; u++) {
        adc_line_t *l = &s_line[u];
        if (tp) {
            uint32_t low_bits = efuse_field(EFUSE_BLK3_RDATA3, u == 0 ? 0 : 16, 0x7F);
            uint32_t high_bits = efuse_field(EFUSE_BLK3_RDATA3, u == 0 ? 7 : 23, 0x1FF);
            uint32_t low = (uint32_t)((u == 0 ? TP_LOW1_OFFSET : TP_LOW2_OFFSET) +
                                      decode(low_bits, 0x7F, 1) * TP_STEP);
            uint32_t high = (uint32_t)((u == 0 ? TP_HIGH1_OFFSET : TP_HIGH2_OFFSET) +
                                       decode(high_bits, 0x1FF, 1) * TP_STEP);
            uint32_t dx = high - low;
            uint32_t dv = TP_HIGH_MV - TP_LOW_MV;
            l->coeff_a = (dv * s_tp_scale[u][3] + dx / 2U) / dx;
            l->coeff_b = TP_HIGH_MV - ((dv * high + dx / 2U) / dx) + s_tp_offset[u][3];
            l->source = EOS_ESP32_ADC_CAL_TWO_POINT;
        } else {
            l->coeff_a = vref * s_vref_scale[u][3] / 4096U;
            l->coeff_b = s_vref_offset[u][3];
            l->source = vref_bits != 0U ? EOS_ESP32_ADC_CAL_EFUSE_VREF
                                        : EOS_ESP32_ADC_CAL_DEFAULT_VREF;
        }
        /* ESP-IDF interpolates the table on Vref; a two-point chip still
         * uses its (possibly nominal) Vref here, as ESP-IDF does. */
        l->vref = vref;
    }
    s_ready = 1;
}

static uint32_t linear(const adc_line_t *l, uint32_t raw)
{
    return ((l->coeff_a * raw + COEFF_A_SCALE / 2U) / COEFF_A_SCALE) + l->coeff_b;
}

static uint32_t lut(int u, uint32_t vref, uint32_t raw)
{
    uint32_t i = (raw - LUT_LOW_THRESH) / LUT_STEP;
    int32_t x2 = LUT_VREF_HIGH - (int32_t)vref;
    int32_t x1 = (int32_t)vref - LUT_VREF_LOW;
    int32_t y2 = (int32_t)((i + 1U) * LUT_STEP + LUT_LOW_THRESH) - (int32_t)raw;
    int32_t y1 = (int32_t)raw - (int32_t)(i * LUT_STEP + LUT_LOW_THRESH);
    int32_t v = s_lut_low[u][i] * x2 * y2 + s_lut_high[u][i] * x1 * y2 +
                s_lut_low[u][i + 1U] * x2 * y1 + s_lut_high[u][i + 1U] * x1 * y1;
    v += ((LUT_VREF_HIGH - LUT_VREF_LOW) * LUT_STEP) / 2;
    v /= (LUT_VREF_HIGH - LUT_VREF_LOW) * LUT_STEP;
    return (uint32_t)v;
}

uint32_t esp32_adc_raw_to_mv(int unit, uint32_t raw, uint32_t bits)
{
    if (!s_ready) characterise();
    int u = unit == 1 ? 0 : 1;
    const adc_line_t *l = &s_line[u];
    raw <<= 12U - bits;
    if (raw > 4095U) raw = 4095U;
    if (raw < LUT_LOW_THRESH) return linear(l, raw);
    uint32_t t = lut(u, l->vref, raw);
    if (raw > LUT_HIGH_THRESH) return t;
    /* Blend from the line into the table across the first table step. */
    uint32_t x = raw - LUT_LOW_THRESH;
    return (linear(l, raw) * (LUT_STEP - x) + t * x + LUT_STEP / 2U) / LUT_STEP;
}

eos_esp32_adc_cal_t eos_esp32_adc_cal_source(uint32_t *vref_mv)
{
    if (!s_ready) characterise();
    if (vref_mv != 0) *vref_mv = s_line[0].vref;
    return s_line[0].source;
}
