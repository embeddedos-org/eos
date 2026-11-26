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
 * Each round also runs deep(): a 24-level recursion that keeps four values
 * live in every level's window, so the chain is longer than the register
 * file's 16 windows. The core spills and reloads windows as it goes, and the
 * tick preempts tasks inside the chain, where the interrupt handler must
 * spill the task's older windows to its own stack. The result is compared
 * with deep_expected(), an iterative computation of the same value (so a
 * broken window handler cannot also corrupt the reference). GCC emits only
 * CALL8, so eos_test_win4() and eos_test_win12() (context_regcheck.S) do the
 * same through CALL4 and CALL12. Error 4, 5 or 6 marks a wrong result from
 * deep(), win4 or win12.
 *
 * Passes after 2 s if no check failed and the tick shared the CPU fairly:
 * every task completed at least MIN_ROUNDS rounds and none fewer than half
 * as many as another. A frame that corrupts the loop registers can slow a
 * task down without failing a value check (seen with the base save area
 * gap removed: 1, 8 and 4 rounds against about 7000).
 */
#include <stdint.h>
#include "eos/kernel.h"
#include "board_esp32.h"

#define SPINNERS 3

extern uint32_t eos_test_regcheck(uint32_t seed, uint32_t rounds);
extern uint32_t eos_test_win4(uint32_t n, uint32_t x);
extern uint32_t eos_test_win12(uint32_t n, uint32_t x);

static volatile uint32_t s_errors[SPINNERS], s_rounds[SPINNERS];

#define DEPTH 24U
#define MIN_ROUNDS 10U

static uint32_t mix(uint32_t n, uint32_t x, uint32_t k)
{
    return (x ^ (n * 0x9E3779B9U)) * (2U * k + 1U) + (k << n % 13U);
}

/* Four values per level stay live across the recursive call, so they sit in
 * the window registers (or the window's save area once spilled). The call
 * goes through a volatile pointer: called directly, GCC turns the recursion
 * into a loop and no window is ever spilled. */
static uint32_t deep(uint32_t n, uint32_t x);
static uint32_t (*volatile s_deep)(uint32_t, uint32_t) = deep;

static uint32_t deep(uint32_t n, uint32_t x)
{
    uint32_t a = mix(n, x, 1), b = mix(n, x, 2), c = mix(n, x, 3), d = mix(n, x, 4);
    if (n == 0U) return a ^ b ^ c ^ d;
    uint32_t r = s_deep(n - 1U, a + b);
    return r * 3U + a + (b ^ c) - d;
}

static uint32_t win4_expected(uint32_t n, uint32_t x)
{
    uint32_t xs[DEPTH + 1U];
    for (uint32_t i = n;; i--) { xs[i] = x; if (i == 0U) break; x += i; }
    uint32_t r = xs[0];
    for (uint32_t i = 1U; i <= n; i++) r = (r ^ xs[i]) + i;
    return r;
}

static uint32_t win12_expected(uint32_t n, uint32_t x)
{
    uint32_t xs[DEPTH + 1U];
    for (uint32_t i = n;; i--) { xs[i] = x; if (i == 0U) break; x ^= i; }
    uint32_t r = xs[0];
    for (uint32_t i = 1U; i <= n; i++) {
        uint32_t v = xs[i];
        r = ((((((((r + (v + 4U)) ^ (v + 5U)) + (v + 6U)) ^ (v + 7U)) + (v + 8U)) ^
               (v + 9U)) + (v + 10U)) ^ (v + 11U)) + i;
    }
    return r;
}

static uint32_t deep_expected(uint32_t x)
{
    uint32_t a[DEPTH + 1U], b[DEPTH + 1U], c[DEPTH + 1U], d[DEPTH + 1U];
    for (uint32_t n = DEPTH;; n--) {
        a[n] = mix(n, x, 1); b[n] = mix(n, x, 2); c[n] = mix(n, x, 3); d[n] = mix(n, x, 4);
        if (n == 0U) break;
        x = a[n] + b[n];
    }
    uint32_t r = a[0] ^ b[0] ^ c[0] ^ d[0];
    for (uint32_t n = 1U; n <= DEPTH; n++) r = r * 3U + a[n] + (b[n] ^ c[n]) - d[n];
    return r;
}

static void spinner(void *arg)
{
    uint32_t id = (uint32_t)(uintptr_t)arg;
    /* Low five bits (SAR) 5, 14, 23: distinct, and none is SAR's reset 0. */
    uint32_t seed = 0x1000U * (id + 1U) + 5U + 9U * id;
    uint32_t want = deep_expected(seed);
    uint32_t want4 = win4_expected(DEPTH, seed), want12 = win12_expected(DEPTH, seed);
    for (;;) {
        uint32_t r = eos_test_regcheck(seed, 50);
        if (r == 0U && deep(DEPTH, seed) != want) r = 4;
        if (r == 0U && eos_test_win4(DEPTH, seed) != want4) r = 5;
        if (r == 0U && eos_test_win12(DEPTH, seed) != want12) r = 6;
        if (r != 0U && s_errors[id] == 0U) s_errors[id] = r;
        s_rounds[id]++;
    }
}

static void monitor(void *arg)
{
    (void)arg;
    eos_task_delay_ms(2000);
    int ok = 1;
    uint32_t lo = UINT32_MAX, hi = 0;
    for (int i = 0; i < SPINNERS; i++) {
        uint32_t r = s_rounds[i];
        if (r < lo) lo = r;
        if (r > hi) hi = r;
    }
    if (lo < MIN_ROUNDS || lo * 2U < hi) ok = 0;
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
        if (eos_task_create("spinner", spinner, (void *)i, 5, 3072) < 0) return 1;
    }
    if (eos_task_create("monitor", monitor, NULL, 1, 1024) < 0) return 1;
    eos_kernel_start();
    return 1; /* not reached */
}
