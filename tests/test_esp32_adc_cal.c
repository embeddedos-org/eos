// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file test_esp32_adc_cal.c
 * @brief ESP32 ADC eFuse calibration (hal/src/hal_esp32/hal_esp32_adc_cal.c)
 *
 * The expected millivolts were produced by ESP-IDF v5.4.2's own
 * adc_cali_line_fitting.c, compiled on the host with the same eFuse values
 * (12 dB attenuation). A full sweep (both units, 9-12 bits, every raw value,
 * seven eFuse settings) matched with no differences when this port was
 * written; these rows keep the edges of each region.
 *
 * One deliberate difference: ESP-IDF v5.4 leaves Vref at 0 for a two-point
 * chip, so its lookup table above raw 2880 returns falling voltages; the
 * port uses the chip's Vref there. Two-point rows stop below 2880, and
 * test_two_point_lut_monotonic() checks the port's behaviour above it.
 */

#include <stdint.h>
#include <stdio.h>

static uint32_t fake_efuse[0x100 / 4];
#define ESP32_REG32(addr) fake_efuse[((addr) - 0x3FF5A000U) / 4U]
#include "../hal/src/hal_esp32/hal_esp32_adc_cal.c"

typedef struct {
    int cfg;
    uint32_t vref, part, tp1l, tp1h, tp2l, tp2h;
    int unit;
    uint32_t bits, raw, mv;
} vec_t;

static const vec_t s_vec[] = {
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 1, 12,    0,  142 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 1, 12,    1,  143 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 1, 12,  100,  223 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 1000,  953 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 2000, 1764 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 2879, 2476 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 2880, 2477 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 2900, 2490 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 2944, 2516 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 2945, 2517 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 3000, 2556 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 4000, 3099 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 4095, 3139 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 2, 12,    0,  128 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 2, 12,    1,  129 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 2, 12,  100,  209 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 1000,  941 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 2000, 1754 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 2879, 2469 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 2880, 2470 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 2900, 2483 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 2944, 2510 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 2945, 2510 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 3000, 2550 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 4000, 3087 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 4095, 3126 },
    { 0, 0x01, 0, 0x00, 0x000, 0x00, 0x000, 1, 9, 300, 2088 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 1, 12,    0,  142 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 1, 12,    1,  143 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 1, 12,  100,  223 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 1000,  948 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 2000, 1753 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 2879, 2461 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 2880, 2462 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 2900, 2475 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 2944, 2502 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 2945, 2502 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 3000, 2543 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 4000, 3094 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 4095, 3134 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 2, 12,    0,  128 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 2, 12,    1,  129 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 2, 12,  100,  209 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 1000,  936 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 2000, 1744 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 2879, 2454 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 2880, 2455 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 2900, 2469 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 2944, 2496 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 2945, 2496 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 3000, 2537 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 4000, 3082 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 4095, 3122 },
    { 1, 0x00, 0, 0x00, 0x000, 0x00, 0x000, 1, 9, 300, 2076 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 1, 12,    0,  142 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 1, 12,    1,  143 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 1, 12,  100,  215 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 1000,  871 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 2000, 1599 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 2879, 2240 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 2880, 2241 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 2900, 2253 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 2944, 2287 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 2945, 2288 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 3000, 2335 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 4000, 3011 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 1, 12, 4095, 3056 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 2, 12,    0,  128 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 2, 12,    1,  129 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 2, 12,  100,  201 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 1000,  859 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 2000, 1590 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 2879, 2232 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 2880, 2233 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 2900, 2246 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 2944, 2283 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 2945, 2284 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 3000, 2330 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 4000, 3011 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 2, 12, 4095, 3055 },
    { 2, 0x1F, 0, 0x00, 0x000, 0x00, 0x000, 1, 9, 300, 1891 },
    { 3, 0x01, 1, 0x02, 0x003, 0x7E, 0x1FD, 1, 12,    0,  137 },
    { 3, 0x01, 1, 0x02, 0x003, 0x7E, 0x1FD, 1, 12,    1,  138 },
    { 3, 0x01, 1, 0x02, 0x003, 0x7E, 0x1FD, 1, 12,  100,  217 },
    { 3, 0x01, 1, 0x02, 0x003, 0x7E, 0x1FD, 1, 12, 1000,  938 },
    { 3, 0x01, 1, 0x02, 0x003, 0x7E, 0x1FD, 1, 12, 2000, 1739 },
    { 3, 0x01, 1, 0x02, 0x003, 0x7E, 0x1FD, 1, 12, 2879, 2443 },
    { 3, 0x01, 1, 0x02, 0x003, 0x7E, 0x1FD, 2, 12,    0,  119 },
    { 3, 0x01, 1, 0x02, 0x003, 0x7E, 0x1FD, 2, 12,    1,  120 },
    { 3, 0x01, 1, 0x02, 0x003, 0x7E, 0x1FD, 2, 12,  100,  200 },
    { 3, 0x01, 1, 0x02, 0x003, 0x7E, 0x1FD, 2, 12, 1000,  924 },
    { 3, 0x01, 1, 0x02, 0x003, 0x7E, 0x1FD, 2, 12, 2000, 1729 },
    { 3, 0x01, 1, 0x02, 0x003, 0x7E, 0x1FD, 2, 12, 2879, 2437 },
    { 3, 0x01, 1, 0x02, 0x003, 0x7E, 0x1FD, 1, 9, 300, 2059 },
};

static void load(const vec_t *v)
{
    for (unsigned i = 0; i < sizeof(fake_efuse) / sizeof(fake_efuse[0]); i++) fake_efuse[i] = 0;
    fake_efuse[0x10 / 4] = v->vref << 8;
    fake_efuse[0x0C / 4] = v->part << 14;
    fake_efuse[0x84 / 4] = v->tp1l | (v->tp1h << 7) | (v->tp2l << 16) | (v->tp2h << 23);
    s_ready = 0;
}

static int test_vectors(void)
{
    int bad = 0;
    for (unsigned i = 0; i < sizeof(s_vec) / sizeof(s_vec[0]); i++) {
        load(&s_vec[i]);
        uint32_t got = esp32_adc_raw_to_mv(s_vec[i].unit, s_vec[i].raw, s_vec[i].bits);
        if (got != s_vec[i].mv) {
            printf("FAIL cfg %d unit %d bits %u raw %u: %u mV, ESP-IDF %u mV\n",
                   s_vec[i].cfg, s_vec[i].unit, s_vec[i].bits, s_vec[i].raw, got, s_vec[i].mv);
            bad++;
        }
    }
    return bad;
}

static int test_sources(void)
{
    int bad = 0;
    uint32_t vref;
    vec_t v = { 0, 0x01, 0, 0, 0, 0, 0, 1, 12, 0, 0 };
    load(&v);
    if (eos_esp32_adc_cal_source(&vref) != EOS_ESP32_ADC_CAL_EFUSE_VREF || vref != 1107U) bad++;
    v.vref = 0x11;   /* sign-magnitude: -1 step */
    load(&v);
    if (eos_esp32_adc_cal_source(&vref) != EOS_ESP32_ADC_CAL_EFUSE_VREF || vref != 1093U) bad++;
    v.vref = 0;
    load(&v);
    if (eos_esp32_adc_cal_source(&vref) != EOS_ESP32_ADC_CAL_DEFAULT_VREF || vref != 1100U) bad++;
    /* Two-point values without BLK3_PART_RESERVE are not trusted. */
    v.tp1l = 0x02; v.tp1h = 0x003; v.tp2l = 0x7E; v.tp2h = 0x1FD;
    load(&v);
    if (eos_esp32_adc_cal_source(NULL) != EOS_ESP32_ADC_CAL_DEFAULT_VREF) bad++;
    v.part = 1;
    load(&v);
    if (eos_esp32_adc_cal_source(NULL) != EOS_ESP32_ADC_CAL_TWO_POINT) bad++;
    if (bad) printf("FAIL calibration source selection (%d)\n", bad);
    return bad;
}

/* ESP-IDF's blend from a chip's line into the shared table can step back a
 * millivolt or two where they meet; the v5.4 defect this guards against
 * drops by hundreds. */
#define MONOTONIC_SLACK_MV 5U

static int test_two_point_lut_monotonic(void)
{
    vec_t v = { 0, 0x01, 1, 0x02, 0x003, 0x7E, 0x1FD, 1, 12, 0, 0 };
    load(&v);
    for (int unit = 1; unit <= 2; unit++) {
        uint32_t prev = 0;
        for (uint32_t raw = 2800; raw < 4096; raw++) {
            uint32_t mv = esp32_adc_raw_to_mv(unit, raw, 12);
            if (mv + MONOTONIC_SLACK_MV < prev) {
                printf("FAIL two-point unit %d: %u mV at raw %u after %u mV\n", unit, mv, raw, prev);
                return 1;
            }
            prev = mv;
        }
    }
    return 0;
}

int main(void)
{
    int bad = test_vectors() + test_sources() + test_two_point_lut_monotonic();
    printf("%s: %u vectors, %d failures\n", bad ? "FAIL" : "PASS",
           (unsigned)(sizeof(s_vec) / sizeof(s_vec[0])), bad);
    return bad != 0;
}
