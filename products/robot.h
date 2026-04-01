// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/* Robot product profile — Motor, IMU, Camera, WiFi, BLE, UART, SPI, I2C, PWM, ADC */
#ifndef EOS_PRODUCT_ROBOT_H
#define EOS_PRODUCT_ROBOT_H

#define EOS_PRODUCT_NAME    "robot"

/* Core peripherals */
#define EOS_ENABLE_GPIO      1
#define EOS_ENABLE_UART      1
#define EOS_ENABLE_SPI       1
#define EOS_ENABLE_I2C       1
#define EOS_ENABLE_TIMER     1

/* Extended peripherals */
#define EOS_ENABLE_ADC       1
#define EOS_ENABLE_PWM       1
#define EOS_ENABLE_WIFI      1
#define EOS_ENABLE_BLE       1
#define EOS_ENABLE_CAMERA    1
#define EOS_ENABLE_MOTOR     1
#define EOS_ENABLE_IMU       1

/* Frameworks */
#define EOS_ENABLE_NET       1
#define EOS_ENABLE_SENSOR    1
#define EOS_ENABLE_MOTOR_CTRL 1

#endif /* EOS_PRODUCT_ROBOT_H */
