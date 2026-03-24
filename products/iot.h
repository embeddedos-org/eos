// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/* IoT device product profile — Sensor, WiFi/BLE/Cellular, Low-power, OTA */
#ifndef EOS_PRODUCT_IOT_H
#define EOS_PRODUCT_IOT_H

#define EOS_PRODUCT_NAME    "iot"

/* Core peripherals */
#define EOS_ENABLE_GPIO      1
#define EOS_ENABLE_UART      1
#define EOS_ENABLE_SPI       1
#define EOS_ENABLE_I2C       1
#define EOS_ENABLE_TIMER     1

/* Extended peripherals */
#define EOS_ENABLE_ADC       1
#define EOS_ENABLE_WIFI      1
#define EOS_ENABLE_BLE       1
#define EOS_ENABLE_CELLULAR  1
#define EOS_ENABLE_FLASH     1
#define EOS_ENABLE_RTC       1
#define EOS_ENABLE_WDT       1

/* Services */
#define EOS_ENABLE_CRYPTO    1
#define EOS_ENABLE_OTA       1
#define EOS_ENABLE_FILESYSTEM 1

/* Frameworks */
#define EOS_ENABLE_POWER     1
#define EOS_ENABLE_NET       1
#define EOS_ENABLE_SENSOR    1

#endif /* EOS_PRODUCT_IOT_H */
