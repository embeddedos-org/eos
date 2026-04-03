// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/* Automotive product profile — CAN, LIN, Ethernet, ADC, PWM, Motor, Safety */
#ifndef EOS_PRODUCT_AUTOMOTIVE_H
#define EOS_PRODUCT_AUTOMOTIVE_H

#define EOS_PRODUCT_NAME    "automotive"

/* Core peripherals */
#define EOS_ENABLE_GPIO      1
#define EOS_ENABLE_UART      1
#define EOS_ENABLE_SPI       1
#define EOS_ENABLE_I2C       1
#define EOS_ENABLE_TIMER     1

/* Extended peripherals */
#define EOS_ENABLE_ADC       1
#define EOS_ENABLE_DAC       1
#define EOS_ENABLE_PWM       1
#define EOS_ENABLE_CAN       1
#define EOS_ENABLE_ETHERNET  1
#define EOS_ENABLE_MOTOR     1
#define EOS_ENABLE_IMU       1

/* Services */
#define EOS_ENABLE_CRYPTO    1
#define EOS_ENABLE_WATCHDOG  1

/* Frameworks */
#define EOS_ENABLE_SENSOR    1
#define EOS_ENABLE_MOTOR_CTRL 1
#define EOS_ENABLE_SAFETY    1
#define EOS_ENABLE_AUDIT     1

#endif /* EOS_PRODUCT_AUTOMOTIVE_H */
