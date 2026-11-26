// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/*
 * STM32F4 arithmetic that runs on the host: the flash address-to-sector map
 * (static inline in hal_extended.h) and the temperature-sensor conversion
 * (hal_stm32f4_adc.c, compiled into this suite; only its register accessors
 * touch hardware, and they are not called).
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

/* Flash is off by default (eos_config.h); the helper lives in the
 * #if EOS_ENABLE_FLASH section of hal_extended.h. */
#define EOS_ENABLE_FLASH 1
#include <eos/hal_extended.h>

int32_t eos_stm32f4_adc_temp_from_raw(uint32_t raw);

/* eos_stm32f4_adc_init() references the RCC driver. It is never called
 * here; this only satisfies the linker. */
void eos_stm32f4_rcc_enable_apb2(uint32_t periph_mask);
void eos_stm32f4_rcc_enable_apb2(uint32_t periph_mask) { (void)periph_mask; }

/* Every address from 0x08060000 up mapped to sector 7, so erasing in
 * sectors 8-11 erased sector 7. */
static void test_flash_sector_map(void)
{
    static const struct { uint32_t addr; int sector; } cases[] = {
        {0x08000000U, 0},  {0x08003FFFU, 0},  {0x08004000U, 1},
        {0x0800C000U, 3},  {0x08010000U, 4},  {0x0801FFFFU, 4},
        {0x08020000U, 5},  {0x08040000U, 6},  {0x08060000U, 7},
        {0x0807FFFFU, 7},  {0x08080000U, 8},  {0x080A0000U, 9},
        {0x080C0000U, 10}, {0x080E0000U, 11}, {0x080FFFFFU, 11},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        assert(eos_flash_stm32f4_sector(cases[i].addr) == cases[i].sector);

    /* Outside the 1 MB array: nothing to erase. */
    assert(eos_flash_stm32f4_sector(0x07FFFFFFU) == -1);
    assert(eos_flash_stm32f4_sector(0x08100000U) == -1);
    assert(eos_flash_stm32f4_sector(0x00000000U) == -1);
    printf("[PASS] flash sector map\n");
}

/* Raw sample for a sensor voltage, 12-bit, 3.3 V reference. */
static uint32_t raw_for_uv(uint32_t uv)
{
    return (uint32_t)(((uint64_t)uv * 4096 + 1650000) / 3300000);
}

/* Readings stayed near 25 °C: whole degrees were added to centi-degrees. */
static void test_temperature_conversion(void)
{
    /* RM0090: 0.76 V at 25 °C, 2.5 mV/°C. Tolerance is one ADC step
     * (~0.8 mV, ~0.33 °C), plus rounding. */
    static const struct { uint32_t uv; int32_t centi; } cases[] = {
        {760000, 2500}, {785000, 3500}, {860000, 6500}, {710000, 500},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int32_t t = eos_stm32f4_adc_temp_from_raw(raw_for_uv(cases[i].uv));
        int32_t err = t - cases[i].centi;
        assert(err >= -40 && err <= 40);
    }
    /* Monotonic in the sample: one ADC step changes the reading. */
    assert(eos_stm32f4_adc_temp_from_raw(975) > eos_stm32f4_adc_temp_from_raw(974));
    printf("[PASS] temperature conversion\n");
}

int main(void)
{
    printf("=== STM32F4 host-side conversions ===\n");
    test_flash_sector_map();
    test_temperature_conversion();
    printf("=== ALL STM32F4 CONVERSION TESTS PASSED ===\n");
    return 0;
}
