// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
/**
 * @file context_test.c
 * @brief Proves a preempted task gets back every register the port saves.
 *
 * scheduler_test.c shows that tasks switch, but its tasks block almost at
 * once, so the tick nearly always interrupts the idle task in waiti and a
 * frame that lost SAR or the loop state would still pass it. Here three tasks
 * at the same priority never block: each runs eos_test_regcheck()
 * (context_regcheck.S), which keeps a0, a2..a15, SAR and a zero-overhead loop
 * live and then checks them. Only the tick moves the CPU between them, so it
 * preempts them inside the check. Each task uses a different seed, so a
 * register restored from the wrong task's frame is caught too.
 *
 * Passes after 2 s if no check failed and every task ran (proving the tick
 * time-sliced them).
 */
#include <stdint.h>
#include "eos/kernel.h"
#include "board_esp32.h"

#define SPINNERS 3

extern uint32_t eos_test_regcheck(uint32_t seed, uint32_t rounds);

static volatile uint32_t s_errors[SPINNERS], s_rounds[SPINNERS];

static void spinner(void *arg)
{
    uint32_t id = (uint32_t)(uintptr_t)arg;
    /* Low five bits (SAR) 5, 14, 23: distinct, and none is SAR's reset 0. */
    uint32_t seed = 0x1000U * (id + 1U) + 5U + 9U * id;
    for (;;) {
        uint32_t r = eos_test_regcheck(seed, 50);
        if (r != 0U && s_errors[id] == 0U) s_errors[id] = r;
        s_rounds[id]++;
    }
}

static void monitor(void *arg)
{
    (void)arg;
    eos_task_delay_ms(2000);
    int ok = 1;
    for (int i = 0; i < SPINNERS; i++) {
        eos_esp32_console_puts("context: task");
        eos_esp32_console_put_u32((uint32_t)i);
        eos_esp32_console_puts(" rounds=");
        eos_esp32_console_put_u32(s_rounds[i]);
        eos_esp32_console_puts(" error=");
        eos_esp32_console_put_u32(s_errors[i]);
        eos_esp32_console_puts("\n");
        if (s_errors[i] != 0U || s_rounds[i] == 0U) ok = 0;
    }
    eos_esp32_console_puts(ok ? "CONTEXT TEST PASSED\n" : "CONTEXT TEST FAILED\n");
    for (;;) eos_task_delay_ms(1000);
}

int main(void)
{
    if (eos_kernel_init() != EOS_KERN_OK) return 1;
    for (uintptr_t i = 0; i < SPINNERS; i++) {
        if (eos_task_create("spinner", spinner, (void *)i, 5, 512) < 0) return 1;
    }
    if (eos_task_create("monitor", monitor, NULL, 1, 1024) < 0) return 1;
    eos_kernel_start();
    return 1; /* not reached */
}
