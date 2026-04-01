// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/* Vacuum cleaner product profile — Motor, ADC (dust sensor), PWM (fan), UART, BLE, Timer */
#ifndef EOS_PRODUCT_VACUUM_H
#define EOS_PRODUCT_VACUUM_H

#define EOS_PRODUCT_NAME    "vacuum"

/* Core peripherals */
#define EOS_ENABLE_GPIO      1
#define EOS_ENABLE_UART      1
#define EOS_ENABLE_TIMER     1

/* Extended peripherals */
#define EOS_ENABLE_ADC       1
#define EOS_ENABLE_PWM       1
#define EOS_ENABLE_BLE       1
#define EOS_ENABLE_MOTOR     1

/* Frameworks */
#define EOS_ENABLE_POWER     1
#define EOS_ENABLE_MOTOR_CTRL 1

#endif /* EOS_PRODUCT_VACUUM_H */
