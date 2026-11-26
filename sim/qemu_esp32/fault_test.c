// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
/**
 * @file fault_test.c
 * @brief Proves the Xtensa LX6 port stops on a fault instead of running on.
 *
 * A task that returns from its entry function has nowhere to go. The port
 * gives every task task_returned() as its return address, which reports
 * through the fatal path with a code of its own (XT_FATAL_TASK_RETURN = 7),
 * so a task returning to anywhere else (address 0 faults as code 2) does not
 * pass. The check is that the report appears and that nothing scheduled after
 * it runs: the task after the fault would print "RAN AFTER FAULT" if the
 * kernel had carried on.
 *
 *   sim/qemu_esp32/run.sh OUT.flash.bin 30 "EOS FATAL code=7"
 */
#include "eos/kernel.h"
#include "board_esp32.h"

static void returns_immediately(void *arg)
{
    (void)arg;
    eos_esp32_console_puts("fault_test: task returning\n");
}

static void runs_later(void *arg)
{
    (void)arg;
    eos_task_delay_ms(100);
    eos_esp32_console_puts("RAN AFTER FAULT\n");
    for (;;) eos_task_delay_ms(1000);
}

int main(void)
{
    if (eos_kernel_init() != EOS_KERN_OK) return 1;
    if (eos_task_create("returns", returns_immediately, NULL, 1, 512) < 0 ||
        eos_task_create("later", runs_later, NULL, 2, 512) < 0) {
        return 1;
    }
    eos_kernel_start();
    return 1; /* not reached */
}
