// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/* Industrial controller product profile — CAN, Ethernet, ADC, DAC, UART, SPI, Timer */
#ifndef EOS_PRODUCT_INDUSTRIAL_H
#define EOS_PRODUCT_INDUSTRIAL_H

#define EOS_PRODUCT_NAME    "industrial"

/* Core peripherals */
#define EOS_ENABLE_GPIO      1
#define EOS_ENABLE_UART      1
#define EOS_ENABLE_SPI       1
#define EOS_ENABLE_I2C       1
#define EOS_ENABLE_TIMER     1

/* Extended peripherals */
#define EOS_ENABLE_ADC       1
#define EOS_ENABLE_DAC       1
#define EOS_ENABLE_CAN       1
#define EOS_ENABLE_ETHERNET  1

/* Services */
#define EOS_ENABLE_WATCHDOG  1

/* Frameworks */
#define EOS_ENABLE_NET       1
#define EOS_ENABLE_SENSOR    1
#define EOS_ENABLE_SAFETY    1

#endif /* EOS_PRODUCT_INDUSTRIAL_H */
