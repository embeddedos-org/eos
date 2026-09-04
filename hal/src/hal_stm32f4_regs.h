// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file hal_stm32f4_regs.h
 * @brief Shared STM32F4 register-access primitives for hal/src translation units
 *
 * Not a public header: included only by hal/src/*.c files that need direct
 * register access on STM32F4. Do not add to hal/include/eos/.
 */

#ifndef EOS_HAL_STM32F4_REGS_H
#define EOS_HAL_STM32F4_REGS_H

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define RCC_APB1ENR_REG REG32(0x40023840U)

#endif /* EOS_HAL_STM32F4_REGS_H */
