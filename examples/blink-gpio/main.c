// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file main.c
 * @brief EoS Example: GPIO Blink — Simplest possible application
 *
 * Toggles an LED on/off every 500ms using the EoS HAL GPIO interface.
 * This is the "Hello World" of embedded systems.
 */

#include <eos/hal.h>
#include <stdio.h>

#define LED_PIN 13

int main(void)
{
    eos_hal_init();

    eos_gpio_config_t led_cfg = {
        .pin   = LED_PIN,
        .mode  = EOS_GPIO_OUTPUT,
        .pull  = EOS_GPIO_PULL_NONE,
        .speed = EOS_GPIO_SPEED_LOW,
    };
    eos_gpio_init(&led_cfg);

    printf("[blink] Starting LED blink on pin %d\n", LED_PIN);

    while (1) {
        eos_gpio_write(LED_PIN, true);
        printf("[blink] LED ON\n");
        eos_delay_ms(500);

        eos_gpio_write(LED_PIN, false);
        printf("[blink] LED OFF\n");
        eos_delay_ms(500);
    }

    return 0;
}
