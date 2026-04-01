// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/* Medical device product profile — ADC (bio-sensor), BLE, Display, Crypto, Safety, Audit */
#ifndef EOS_PRODUCT_MEDICAL_H
#define EOS_PRODUCT_MEDICAL_H

#define EOS_PRODUCT_NAME    "medical"

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

/* Services */
#define EOS_ENABLE_CRYPTO    1
#define EOS_ENABLE_SECURITY  1

/* Frameworks */
#define EOS_ENABLE_POWER     1
#define EOS_ENABLE_SENSOR    1
#define EOS_ENABLE_SAFETY    1
#define EOS_ENABLE_AUDIT     1

#endif /* EOS_PRODUCT_MEDICAL_H */
