// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file nvic.c
 * @brief ARM Cortex-M NVIC interrupt controller driver
 */

#include "eos/arch.h"
#include <string.h>

#define NVIC_ISER_BASE  ((volatile uint32_t *)0xE000E100)
#define NVIC_ICER_BASE  ((volatile uint32_t *)0xE000E180)
#define NVIC_ISPR_BASE  ((volatile uint32_t *)0xE000E200)
#define NVIC_ICPR_BASE  ((volatile uint32_t *)0xE000E280)
#define NVIC_IPR_BASE   ((volatile uint8_t  *)0xE000E400)
#define VTOR            (*(volatile uint32_t *)0xE000ED08)

#ifndef EOS_MAX_IRQS
#define EOS_MAX_IRQS    240
#endif

static eos_irq_handler_t irq_handlers[EOS_MAX_IRQS];

void eos_arch_irq_enable(uint32_t irq_num)
{
    if (irq_num >= EOS_MAX_IRQS) return;
    NVIC_ISER_BASE[irq_num >> 5] = (1U << (irq_num & 0x1F));
}

void eos_arch_irq_disable(uint32_t irq_num)
{
    if (irq_num >= EOS_MAX_IRQS) return;
    NVIC_ICER_BASE[irq_num >> 5] = (1U << (irq_num & 0x1F));
}

void eos_arch_irq_set_priority(uint32_t irq_num, uint8_t priority)
{
    if (irq_num >= EOS_MAX_IRQS) return;
    NVIC_IPR_BASE[irq_num] = priority;
}

int eos_arch_irq_register(uint32_t irq_num, eos_irq_handler_t handler)
{
    if (irq_num >= EOS_MAX_IRQS || !handler) return -1;
    irq_handlers[irq_num] = handler;
    return 0;
}

void eos_irq_set_pending(uint32_t irq_num)
{
    if (irq_num >= EOS_MAX_IRQS) return;
    NVIC_ISPR_BASE[irq_num >> 5] = (1U << (irq_num & 0x1F));
}

void eos_irq_clear_pending(uint32_t irq_num)
{
    if (irq_num >= EOS_MAX_IRQS) return;
    NVIC_ICPR_BASE[irq_num >> 5] = (1U << (irq_num & 0x1F));
}

void eos_nvic_set_vector_table(uint32_t base_addr)
{
    VTOR = base_addr;
    __asm volatile ("dsb");
    __asm volatile ("isb");
}

void eos_irq_dispatch(uint32_t irq_num)
{
    if (irq_num < EOS_MAX_IRQS && irq_handlers[irq_num]) {
        irq_handlers[irq_num]();
    }
}
