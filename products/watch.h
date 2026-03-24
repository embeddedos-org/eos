// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/* Smartwatch product profile — Display, BLE, IMU, ADC (heart rate), Low-power, Timer */
#ifndef EOS_PRODUCT_WATCH_H
#define EOS_PRODUCT_WATCH_H

#define EOS_PRODUCT_NAME    "watch"

/* Core peripherals */
#define EOS_ENABLE_GPIO      1
#define EOS_ENABLE_UART      1
#define EOS_ENABLE_SPI       1
#define EOS_ENABLE_I2C       1
#define EOS_ENABLE_TIMER     1

/* Extended peripherals */
#define EOS_ENABLE_ADC       1
#define EOS_ENABLE_BLE       1
#define EOS_ENABLE_DISPLAY   1
#define EOS_ENABLE_IMU       1

/* Services */
#define EOS_ENABLE_CRYPTO    1
#define EOS_ENABLE_OTA       1

/* Frameworks */
#define EOS_ENABLE_POWER     1
#define EOS_ENABLE_SENSOR    1

#endif /* EOS_PRODUCT_WATCH_H */
