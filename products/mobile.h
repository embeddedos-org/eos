// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/* Mobile device product profile — Display, Camera, Audio, WiFi, BLE, USB, GPS, IMU */
#ifndef EOS_PRODUCT_MOBILE_H
#define EOS_PRODUCT_MOBILE_H

#define EOS_PRODUCT_NAME    "mobile"

/* Core peripherals */
#define EOS_ENABLE_GPIO      1
#define EOS_ENABLE_UART      1
#define EOS_ENABLE_SPI       1
#define EOS_ENABLE_I2C       1
#define EOS_ENABLE_TIMER     1

/* Extended peripherals */
#define EOS_ENABLE_ADC       1
#define EOS_ENABLE_USB       1
#define EOS_ENABLE_WIFI      1
#define EOS_ENABLE_BLE       1
#define EOS_ENABLE_CAMERA    1
#define EOS_ENABLE_AUDIO     1
#define EOS_ENABLE_DISPLAY   1
#define EOS_ENABLE_GNSS      1
#define EOS_ENABLE_IMU       1

/* Services */
#define EOS_ENABLE_CRYPTO    1
#define EOS_ENABLE_SECURITY  1
#define EOS_ENABLE_OTA       1

/* Frameworks */
#define EOS_ENABLE_POWER     1
#define EOS_ENABLE_NET       1
#define EOS_ENABLE_SENSOR    1

#endif /* EOS_PRODUCT_MOBILE_H */
