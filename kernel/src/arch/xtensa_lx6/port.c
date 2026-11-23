// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file port.c
 * @brief Xtensa LX6 (ESP32) port layer for EoS kernel
 *
 * Counterpart of kernel/src/arch/arm_cm/: the CCOMPARE0 timer stands in for
 * SysTick and CPU software interrupt 7 for PendSV. Both are level-1 lines
 * handled by vectors.S, which calls eos_xtensa_irq_dispatch() here.
 *
 * Requires -mabi=call0: vectors.S saves a0..a15 and nothing else, which is
 * the whole register state only when no register windows are in use.
 */

#include "eos/arch.h"
#include "eos/kernel.h"
#include "xtensa_frame.h"
#include <stddef.h>
#include <string.h>

#if !defined(__XTENSA_CALL0_ABI__)
#error "The Xtensa LX6 port needs the CALL0 ABI: build with -mabi=call0"
#endif

/*
 * CCOUNT runs at the CPU clock. After a power-on reset the ESP32 boot ROM
 * leaves the CPU on its 40 MHz crystal, and this port does not change the
 * clock (ESP-IDF, bootloader_clock_init.c: "when ROM bootloader is running at
 * 40 MHz").
 */
#ifndef EOS_SYS_CLOCK_HZ
#define EOS_SYS_CLOCK_HZ   40000000U
#endif

/* eos_task_delay_ms() adds milliseconds to g_tick, so the tick must be 1 kHz. */
#ifndef EOS_TICK_RATE_HZ
#define EOS_TICK_RATE_HZ   1000U
#endif

#define XT_TICK_CYCLES     (EOS_SYS_CLOCK_HZ / EOS_TICK_RATE_HZ)

#define XT_NUM_INTERRUPTS  32U

_Static_assert(XT_FRAME_SIZE % 16 == 0, "CALL0 ABI keeps a1 16-byte aligned");
_Static_assert(XT_F_LCOUNT + 4 <= XT_FRAME_SIZE, "frame fields fit the frame");

/* Scheduler state shared with task.c and vectors.S */
extern uint32_t **g_current_sp;
extern volatile uint32_t g_tick;
extern void eos_schedule(void);
extern void eos_task_wake_check(uint32_t current_tick);
extern void eos_swtimer_tick(uint32_t current_tick);

extern void eos_xtensa_start_first_task(void);

void eos_xtensa_irq_dispatch(void);

static eos_irq_handler_t s_irq_handlers[XT_NUM_INTERRUPTS];

/* ---- special-register access ------------------------------------------ */

static inline uint32_t xt_ccount(void)
{
    uint32_t v;
    __asm volatile ("rsr.ccount %0" : "=a" (v));
    return v;
}

static inline uint32_t xt_ccompare0(void)
{
    uint32_t v;
    __asm volatile ("rsr.ccompare0 %0" : "=a" (v));
    return v;
}

static inline void xt_set_ccompare0(uint32_t v)
{
    /* Writing CCOMPARE0 also clears a pending CCOMPARE0 interrupt. */
    __asm volatile ("wsr.ccompare0 %0\n\tesync" :: "a" (v) : "memory");
}

static inline uint32_t xt_interrupt(void)
{
    uint32_t v;
    __asm volatile ("rsr.interrupt %0" : "=a" (v));
    return v;
}

static inline uint32_t xt_intenable(void)
{
    uint32_t v;
    __asm volatile ("rsr.intenable %0" : "=a" (v));
    return v;
}

static inline void xt_set_intenable(uint32_t v)
{
    __asm volatile ("wsr.intenable %0\n\trsync" :: "a" (v) : "memory");
}

/* ---- task frames -------------------------------------------------------- */

/*
 * A task function that returns has no caller to return to. The Cortex-M port
 * gives it an invalid LR so that it faults; this does the same explicitly,
 * reporting through the fatal path rather than running off into memory.
 */
static void task_returned(void)
{
    __asm volatile (
        "movi a2, %0\n\t"
        "j    _xt_fatal_entry"
        :: "i" (XT_FATAL_USER_EXC) : "a2");
    for (;;) {}
}

uint32_t *eos_port_init_stack(uint32_t *stack_top, void (*entry)(void *), void *arg)
{
    uintptr_t top = (uintptr_t)stack_top & ~(uintptr_t)15U;
    uint32_t *frame = (uint32_t *)(top - XT_FRAME_SIZE);

    memset(frame, 0, XT_FRAME_SIZE);
    frame[XT_F_PC / 4] = (uint32_t)(uintptr_t)entry;
    frame[XT_F_PS / 4] = XT_PS_TASK;
    frame[XT_F_A0 / 4] = (uint32_t)(uintptr_t)task_returned;  /* return address */
    frame[XT_F_A2 / 4] = (uint32_t)(uintptr_t)arg;            /* first argument */
    return frame;
}

void eos_port_start_first_task(void)
{
    uint32_t mask = (1U << XT_INT_TICK) | (1U << XT_INT_YIELD);

    xt_set_ccompare0(xt_ccount() + XT_TICK_CYCLES);
    xt_set_intenable(xt_intenable() | mask);
    eos_xtensa_start_first_task();
}

void eos_port_yield(void)
{
    /* Pends software interrupt 7. Inside a critical section it stays pending
     * until the section ends, as PendSV does on Cortex-M. */
    uint32_t bit = 1U << XT_INT_YIELD;
    __asm volatile ("wsr.intset %0\n\trsync" :: "a" (bit) : "memory");
}

uint32_t eos_port_enter_critical(void)
{
    uint32_t ps;
    /* XCHAL_EXCM_LEVEL is 3 on the ESP32; every line this port uses is
     * level 1, so level 3 masks them all. */
    __asm volatile ("rsil %0, 3" : "=a" (ps) :: "memory");
    return ps;
}

void eos_port_exit_critical(uint32_t state)
{
    __asm volatile ("wsr.ps %0\n\trsync" :: "a" (state) : "memory");
}

/* ---- interrupt dispatch ------------------------------------------------- */

static void tick_handler(void)
{
    uint32_t next = xt_ccompare0() + XT_TICK_CYCLES;

    xt_set_ccompare0(next);
    /* If the handler ran so late that CCOUNT is already past the next
     * deadline, CCOMPARE0 would not match again for 2^32 cycles (107 s at
     * 40 MHz). Re-arm from now instead; the late tick is lost, not the timer. */
    if ((int32_t)(next - xt_ccount()) <= 0) {
        xt_set_ccompare0(xt_ccount() + XT_TICK_CYCLES);
    }

    eos_kernel_tick();
    eos_task_wake_check(g_tick);
    eos_swtimer_tick(g_tick);
}

/* Called by vectors.S on the interrupt stack with level-1 interrupts masked. */
void eos_xtensa_irq_dispatch(void)
{
    uint32_t pending = xt_interrupt() & xt_intenable() & XT_INTLEVEL1_MASK;
    int reschedule = 0;

    if (pending & (1U << XT_INT_TICK)) {
        tick_handler();
        reschedule = 1;
    }
    if (pending & (1U << XT_INT_YIELD)) {
        uint32_t bit = 1U << XT_INT_YIELD;
        __asm volatile ("wsr.intclear %0\n\trsync" :: "a" (bit) : "memory");
        reschedule = 1;
    }

    pending &= ~((1U << XT_INT_TICK) | (1U << XT_INT_YIELD));
    for (uint32_t n = 0; pending != 0U; n++, pending >>= 1) {
        if ((pending & 1U) == 0U) continue;
        if (s_irq_handlers[n] != NULL) {
            s_irq_handlers[n]();
        } else {
            /* Enabled without a handler: nothing will ever clear it, so it
             * would re-fire forever. Disable the line instead. */
            xt_set_intenable(xt_intenable() & ~(1U << n));
        }
    }

    /* The tick always reschedules, as SysTick_Handler always pends PendSV. */
    if (reschedule && g_current_sp != NULL) {
        eos_schedule();
    }
}

/* ---- arch IRQ API (eos/arch.h) ------------------------------------------ */

/*
 * irq_num is a CPU interrupt line, 0..31. Lines 6 and 7 belong to the kernel.
 * Only level-1 lines can be registered, because only the level-1 vector
 * dispatches; routing a peripheral to a line through the ESP32 interrupt
 * matrix is the board's job.
 */
static int irq_line_usable(uint32_t irq_num)
{
    if (irq_num >= XT_NUM_INTERRUPTS) return 0;
    if (irq_num == XT_INT_TICK || irq_num == XT_INT_YIELD) return 0;
    return (XT_INTLEVEL1_MASK & (1U << irq_num)) != 0U;
}

int eos_arch_irq_register(uint32_t irq_num, eos_irq_handler_t handler)
{
    if (!irq_line_usable(irq_num) || handler == NULL) return -1;
    uint32_t crit = eos_port_enter_critical();
    s_irq_handlers[irq_num] = handler;
    eos_port_exit_critical(crit);
    return 0;
}

void eos_arch_irq_enable(uint32_t irq_num)
{
    if (!irq_line_usable(irq_num)) return;
    uint32_t crit = eos_port_enter_critical();
    xt_set_intenable(xt_intenable() | (1U << irq_num));
    eos_port_exit_critical(crit);
}

void eos_arch_irq_disable(uint32_t irq_num)
{
    if (!irq_line_usable(irq_num)) return;
    uint32_t crit = eos_port_enter_critical();
    xt_set_intenable(xt_intenable() & ~(1U << irq_num));
    eos_port_exit_critical(crit);
}

void eos_arch_irq_set_priority(uint32_t irq_num, uint8_t priority)
{
    /* Xtensa fixes each CPU line's level in hardware (XCHAL_INTn_LEVEL);
     * there is no register to change it. A different priority means using a
     * different line. */
    (void)irq_num;
    (void)priority;
}
