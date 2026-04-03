// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/* Adapter/dongle product profile — USB, UART, SPI, Ethernet, WiFi, BLE */
#ifndef EOS_PRODUCT_ADAPTER_H
#define EOS_PRODUCT_ADAPTER_H

#define EOS_PRODUCT_NAME    "adapter"

/* Core peripherals */
#define EOS_ENABLE_GPIO      1
#define EOS_ENABLE_UART      1
#define EOS_ENABLE_SPI       1
#define EOS_ENABLE_I2C       1
#define EOS_ENABLE_TIMER     1

/* Extended peripherals */
#define EOS_ENABLE_USB       1
#define EOS_ENABLE_ETHERNET  1
#define EOS_ENABLE_WIFI      1
#define EOS_ENABLE_BLE       1

/* Frameworks */
#define EOS_ENABLE_NET       1

#endif /* EOS_PRODUCT_ADAPTER_H */
