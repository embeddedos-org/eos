// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file eos_config.h
 * @brief EoS kernel and subsystem feature configuration
 *
 * Define feature flags before including any EoS headers,
 * or pass them as compiler defines (-DEOS_ENABLE_MULTICORE=1).
 */

#ifndef EOS_CONFIG_H
#define EOS_CONFIG_H

/* ---- Kernel Features ---- */
#ifndef EOS_ENABLE_MULTICORE
#define EOS_ENABLE_MULTICORE    0
#endif

#ifndef EOS_ENABLE_NET
#define EOS_ENABLE_NET          0
#endif

/* ---- HAL Extended Peripherals ---- */
#ifndef EOS_ENABLE_ADC
#define EOS_ENABLE_ADC          0
#endif
#ifndef EOS_ENABLE_DAC
#define EOS_ENABLE_DAC          0
#endif
#ifndef EOS_ENABLE_PWM
#define EOS_ENABLE_PWM          0
#endif
#ifndef EOS_ENABLE_CAN
#define EOS_ENABLE_CAN          0
#endif
#ifndef EOS_ENABLE_USB
#define EOS_ENABLE_USB          0
#endif
#ifndef EOS_ENABLE_ETHERNET
#define EOS_ENABLE_ETHERNET     0
#endif
#ifndef EOS_ENABLE_WIFI
#define EOS_ENABLE_WIFI         0
#endif
#ifndef EOS_ENABLE_BLE
#define EOS_ENABLE_BLE          0
#endif
#ifndef EOS_ENABLE_CAMERA
#define EOS_ENABLE_CAMERA       0
#endif
#ifndef EOS_ENABLE_AUDIO
#define EOS_ENABLE_AUDIO        0
#endif
#ifndef EOS_ENABLE_DISPLAY
#define EOS_ENABLE_DISPLAY      0
#endif
#ifndef EOS_ENABLE_MOTOR
#define EOS_ENABLE_MOTOR        0
#endif
#ifndef EOS_ENABLE_GNSS
#define EOS_ENABLE_GNSS         0
#endif
#ifndef EOS_ENABLE_IMU
#define EOS_ENABLE_IMU          0
#endif
#ifndef EOS_ENABLE_TOUCH
#define EOS_ENABLE_TOUCH        0
#endif
#ifndef EOS_ENABLE_RTC
#define EOS_ENABLE_RTC          0
#endif
#ifndef EOS_ENABLE_DMA
#define EOS_ENABLE_DMA          0
#endif
#ifndef EOS_ENABLE_FLASH
#define EOS_ENABLE_FLASH        0
#endif
#ifndef EOS_ENABLE_WDT
#define EOS_ENABLE_WDT          0
#endif
#ifndef EOS_ENABLE_NFC
#define EOS_ENABLE_NFC          0
#endif
#ifndef EOS_ENABLE_IR
#define EOS_ENABLE_IR           0
#endif
#ifndef EOS_ENABLE_CELLULAR
#define EOS_ENABLE_CELLULAR     0
#endif
#ifndef EOS_ENABLE_RADAR
#define EOS_ENABLE_RADAR        0
#endif
#ifndef EOS_ENABLE_GPU
#define EOS_ENABLE_GPU          0
#endif
#ifndef EOS_ENABLE_HDMI
#define EOS_ENABLE_HDMI         0
#endif
#ifndef EOS_ENABLE_PCIE
#define EOS_ENABLE_PCIE         0
#endif
#ifndef EOS_ENABLE_SDIO
#define EOS_ENABLE_SDIO         0
#endif
#ifndef EOS_ENABLE_HAPTICS
#define EOS_ENABLE_HAPTICS      0
#endif

/* ---- Driver Scaffolds ---- */
#ifndef EOS_ENABLE_USB_HOST
#define EOS_ENABLE_USB_HOST     0
#endif
#ifndef EOS_ENABLE_ETHERNET_DRV
#define EOS_ENABLE_ETHERNET_DRV 0
#endif
#ifndef EOS_ENABLE_DISPLAY_DRV
#define EOS_ENABLE_DISPLAY_DRV  0
#endif
#ifndef EOS_ENABLE_PCIE_DRV
#define EOS_ENABLE_PCIE_DRV     0
#endif

#endif /* EOS_CONFIG_H */
