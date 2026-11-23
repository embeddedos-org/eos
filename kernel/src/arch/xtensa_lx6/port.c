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
 * Windowed ABI (GCC's default for the ESP32): vectors.S saves the
 * interrupted window and spills the older ones to the task's stack.
 */

#include "eos/arch.h"
#include "eos/kernel.h"
#include "xtensa_frame.h"
#include <stddef.h>
#include <string.h>

#if defined(__XTENSA_CALL0_ABI__)
#error "The Xtensa LX6 port uses the windowed ABI: do not build it with -mabi=call0"
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

_Static_assert(XT_STACK_FRAME % 16 == 0, "the ABI keeps a1 16-byte aligned");
_Static_assert(XT_F_EXCCAUSE + 4 <= XT_FRAME_SIZE, "frame fields fit the frame");

/* Scheduler state shared with task.c and vectors.S */
#include "eos/kernel_internal.h"   /* g_current_sp (per CPU on SMP) */
extern volatile uint32_t g_tick;
extern void eos_schedule(void);
extern void eos_task_wake_check(uint32_t current_tick);
extern void eos_swtimer_tick(uint32_t current_tick);

extern void eos_xtensa_start_first_task(void);
void eos_xtensa_irq_dispatch(void);

#if EOS_SMP_CORES > 1
/*
 * SMP (EOS_SMP_CORES 2, the ESP32's PRO and APP CPUs). The board routes a
 * cross-CPU interrupt to line XT_INT_IPI on each CPU (eos_board_ipi_*), and
 * starts the APP CPU at _start_app (boards/esp32), which ends in
 * eos_xtensa_secondary_main() below.
 */
#define XT_INT_IPI 2U
extern void eos_board_ipi_init(uint32_t cpu);
extern void eos_board_ipi_raise(uint32_t cpu);
extern void eos_board_ipi_clear(uint32_t cpu);
void eos_xtensa_secondary_main(void);

uint32_t eos_port_cpu_id(void)
{
    uint32_t prid;
    __asm volatile ("rsr.prid %0" : "=a" (prid));
    return (prid >> 13) & 1U;     /* 0xCDCD: PRO CPU, 0xABAB: APP CPU */
}

/* The kernel lock: 0 when free, else the owning CPU + 1. Recursive on the
 * owning CPU (nested critical sections), with the depth kept per CPU. */
static volatile uint32_t s_klock;
static uint32_t s_kdepth[EOS_SMP_CORES];

/* Compare-and-swap with S32C1I: stores @p desired if *addr == @p expected;
 * returns the value *addr held. */
static inline uint32_t cas32(volatile uint32_t *addr, uint32_t expected, uint32_t desired)
{
    __asm volatile ("wsr.scompare1 %2\n\t"
                    "s32c1i %0, %1, 0"
                    : "+a" (desired) : "a" (addr), "a" (expected) : "memory");
    return desired;
}
#endif

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

extern void eos_xtensa_task_returned(void);

/*
 * A new task's frame makes RFE land on the entry function's ENTRY as if the
 * restored window had just executed CALL4 to it: PS.CALLINC = 1, a4 the
 * return address, a6 the argument (CALL4 passes a callee's a2 in the
 * caller's a6), and a1 the stack top, from which ENTRY carves the function's
 * frame. A task function that returns has no caller to return to: RETW goes
 * to eos_xtensa_task_returned (vectors.S), which reports through the fatal
 * path with a code of its own, so a test can tell this path from any other
 * fault. The top two bits of a windowed return address carry the caller's
 * window increment (1, for CALL4); RETW keeps the PC's own top bits, which
 * are the same 0b01 for every IRAM address.
 */
uint32_t *eos_port_init_stack(uint32_t *stack_top, void (*entry)(void *), void *arg)
{
    uintptr_t top = (uintptr_t)stack_top & ~(uintptr_t)15U;
    uint32_t *frame = (uint32_t *)(top - XT_STACK_FRAME);
    uint32_t ret = (uint32_t)(uintptr_t)eos_xtensa_task_returned;

    /* The base save area too: the entry function's RETW reloads the
     * restored window's a0..a3 from it. */
    memset(frame, 0, XT_STACK_FRAME);
    frame[XT_F_PC / 4] = (uint32_t)(uintptr_t)entry;
    frame[XT_F_PS / 4] = XT_PS_TASK | (1U << XT_PS_CALLINC_SHIFT);
    frame[XT_F_A4 / 4] = (ret & 0x3FFFFFFFU) | (1U << 30);
    frame[XT_F_A6 / 4] = (uint32_t)(uintptr_t)arg;
    return frame;
}

void eos_port_start_first_task(void)
{
    uint32_t mask = (1U << XT_INT_TICK) | (1U << XT_INT_YIELD);
#if EOS_SMP_CORES > 1
    eos_board_ipi_init(eos_port_cpu_id());
    mask |= 1U << XT_INT_IPI;
#endif

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
#if EOS_SMP_CORES > 1
    /* What made this CPU reschedule (a task readied, deleted, moved) may
     * concern the others: let them reschedule too. */
    eos_port_kick_other_cpus();
#endif
}

#if EOS_SMP_CORES > 1
void eos_port_kick_other_cpus(void)
{
    uint32_t me = eos_port_cpu_id();
    for (uint32_t c = 0; c < EOS_SMP_CORES; c++) {
        if (c != me) eos_board_ipi_raise(c);
    }
}
#endif

uint32_t eos_port_enter_critical(void)
{
    uint32_t ps;
    /* XCHAL_EXCM_LEVEL is 3 on the ESP32; every line this port uses is
     * level 1, so level 3 masks them all. Only ever raise the level: a caller
     * already above 3 (the boot code runs at 15) keeps its level, where an
     * unconditional RSIL 3 would lower it. */
    __asm volatile ("rsr.ps %0" : "=a" (ps) :: "memory");
    if ((ps & XT_PS_INTLEVEL_MASK) < 3U) {
        __asm volatile ("rsil %0, 3" : "=a" (ps) :: "memory");
    }
#if EOS_SMP_CORES > 1
    /* Masked first, then locked: the lock is never held with this CPU's
     * interrupts open, so nothing on this CPU can try to take it again
     * except through the recursion below. */
    uint32_t cpu = eos_port_cpu_id(), me = cpu + 1U;
    if (s_klock == me) {
        s_kdepth[cpu]++;
    } else {
        while (cas32(&s_klock, 0U, me) != 0U) {
        }
        s_kdepth[cpu] = 1U;
    }
#endif
    return ps;
}

void eos_port_exit_critical(uint32_t state)
{
#if EOS_SMP_CORES > 1
    uint32_t cpu = eos_port_cpu_id();
    if (s_klock == cpu + 1U && --s_kdepth[cpu] == 0U) {
        __asm volatile ("memw" ::: "memory");
        s_klock = 0U;
        __asm volatile ("memw" ::: "memory");
    }
#endif
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

#if EOS_SMP_CORES > 1
    /* Every CPU's tick time-slices its own tasks; kernel time, delays and
     * software timers advance on CPU 0 only. */
    if (eos_port_cpu_id() != 0U) return;
#endif
    eos_kernel_tick();
    eos_task_wake_check(g_tick);
    eos_swtimer_tick(g_tick);
}

/* Called by vectors.S on the interrupt stack with level-1 interrupts masked. */
void eos_xtensa_irq_dispatch(void)
{
    uint32_t pending = xt_interrupt() & xt_intenable() & XT_INTLEVEL1_MASK;
    int reschedule = 0;
#if EOS_SMP_CORES > 1
    /* The kernel lock for the tick, the handlers and the scheduler. */
    uint32_t crit = eos_port_enter_critical();
    if (pending & (1U << XT_INT_IPI)) {
        eos_board_ipi_clear(eos_port_cpu_id());
        reschedule = 1;
    }
    pending &= ~(1U << XT_INT_IPI);
#endif

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
#if EOS_SMP_CORES > 1
    eos_port_exit_critical(crit);
#endif
}

#if EOS_SMP_CORES > 1
/* The APP CPU, from boards/esp32 _start_app on its own boot stack: join the
 * scheduler. Its tick and cross-CPU interrupt are set up by
 * eos_port_start_first_task(). */
void eos_xtensa_secondary_main(void)
{
    eos_kernel_start_secondary();
}
#endif

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
#if EOS_SMP_CORES > 1
    if (irq_num == XT_INT_IPI) return 0;
#endif
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
