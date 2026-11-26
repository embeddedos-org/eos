// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
/**
 * @file scheduler_test.c
 * @brief Proves the Xtensa LX6 port schedules, on QEMU's esp32 machine and on
 *        a real ESP-WROOM-32 board, and blinks the board's LED through the HAL.
 *
 * Same shape as sim/qemu_cortex_m3/scheduler_test.c. Two workers block in
 * eos_task_delay_ms() and can only make progress if CCOMPARE0 ticks and the
 * software interrupt switches context. A third task toggles the user LED
 * through eos_gpio_toggle() every 50 ms. A higher-priority monitor checks all
 * three after 400 ms and prints a verdict on UART0.
 *
 * There is no semihosting exit on the ESP32, so the image does not stop: after
 * the verdict the LED keeps blinking and the monitor prints a heartbeat once a
 * second. A host can time the heartbeats to check the tick against wall time.
 */
#include <stdbool.h>
#include <stdint.h>
#include "eos/kernel.h"
#include "eos/hal.h"
#include "board_esp32.h"

/*
 * On a board, a toggle that does not reach the pad fails the test. QEMU's
 * esp32 machine does not feed GPIO outputs back into GPIO_IN (every read is
 * 0), so the QEMU build passes -DEOS_TEST_PAD_READBACK=0; the errors are still
 * printed there.
 */
#ifndef EOS_TEST_PAD_READBACK
#define EOS_TEST_PAD_READBACK 1
#endif

static volatile unsigned a_runs, b_runs, led_toggles, led_readback_errors;

static void worker_a(void *arg) { (void)arg; for (;;) { a_runs++; eos_task_delay_ms(10); } }
static void worker_b(void *arg) { (void)arg; for (;;) { b_runs++; eos_task_delay_ms(20); } }

/*
 * Toggles the LED and reads the pad back. The HAL enables the pad's input
 * path, so eos_gpio_read() reports the pin's electrical level: a mismatch
 * means the toggle did not reach the pin, which a counter alone cannot show.
 */
static void blinker(void *arg)
{
    (void)arg;
    bool on = false;
    for (;;) {
        eos_gpio_toggle(EOS_BOARD_LED_GPIO);
        on = !on;
        led_toggles++;
        if (eos_gpio_read(EOS_BOARD_LED_GPIO) != on) led_readback_errors++;
        eos_task_delay_ms(50);
    }
}

static void put_kv(const char *key, uint32_t v)
{
    eos_esp32_console_puts(key);
    eos_esp32_console_put_u32(v);
}

static void monitor(void *arg)
{
    (void)arg;
    uint32_t c0 = eos_esp32_ccount();
    eos_task_delay_ms(400);
    uint32_t cycles = eos_esp32_ccount() - c0;
    unsigned a = a_runs, b = b_runs, t = led_toggles;

    put_kv("scheduler: worker_a=", a);
    put_kv(" worker_b=", b);
    put_kv(" led_toggles=", t);
    put_kv(" led_readback_errors=", led_readback_errors);
    put_kv(" ticks=", eos_tick_get());
    put_kv(" cycles_per_400ms=", cycles);
    eos_esp32_console_puts("\n");

    /* 400 ms at 10 ms / 20 ms / 50 ms periods: about 40, 20 and 8. The margin
     * is wide for emulator jitter; a scheduler that never switches leaves
     * them at 0 or 1. */
    int pad_ok = !EOS_TEST_PAD_READBACK || led_readback_errors == 0;
    if (a >= 10 && b >= 5 && a > b && t >= 4 && pad_ok) {
        eos_esp32_console_puts("SCHEDULER TEST PASSED\n");
    } else {
        eos_esp32_console_puts("SCHEDULER TEST FAILED\n");
    }

    for (;;) {
        eos_task_delay_ms(1000);
        put_kv("heartbeat tick=", eos_tick_get());
        put_kv(" led_toggles=", led_toggles);
        put_kv(" led_readback_errors=", led_readback_errors);
        eos_esp32_console_puts("\n");
    }
}

int main(void)
{
    eos_esp32_console_puts("\nEoS on ESP32 (Xtensa LX6): booting\n");

    if (eos_hal_init() != 0) {
        eos_esp32_console_puts("hal init failed\n");
        return 1;
    }
    eos_gpio_config_t led = {
        .pin = EOS_BOARD_LED_GPIO,
        .mode = EOS_GPIO_OUTPUT,
        .pull = EOS_GPIO_PULL_NONE,
        .speed = EOS_GPIO_SPEED_LOW,
        .af_num = 0,
    };
    if (eos_gpio_init(&led) != 0) {
        eos_esp32_console_puts("led gpio init failed\n");
        return 1;
    }

    if (eos_kernel_init() != EOS_KERN_OK) {
        eos_esp32_console_puts("kernel init failed\n");
        return 1;
    }
    if (eos_task_create("worker_a", worker_a, NULL, 3, 512) < 0 ||
        eos_task_create("worker_b", worker_b, NULL, 3, 512) < 0 ||
        eos_task_create("blinker", blinker, NULL, 3, 512) < 0 ||
        eos_task_create("monitor", monitor, NULL, 1, 1024) < 0) {
        eos_esp32_console_puts("task create failed\n");
        return 1;
    }
    eos_esp32_console_puts("scheduler: starting kernel\n");
    eos_kernel_start();
    return 1; /* not reached */
}
