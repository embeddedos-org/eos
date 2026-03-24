// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file eos_config.h
 * @brief EoS Product Configuration — Compile-time feature selection
 *
 * Include this header to conditionally enable HAL modules, services, and
 * drivers based on the target product. Set EOS_PRODUCT via CMake:
 *
 *   cmake -B build -DEOS_PRODUCT=robot
 *
 * If no product is selected, all features are enabled (development build).
 */

#ifndef EOS_CONFIG_H
#define EOS_CONFIG_H

/* ============================================================
 * Product Profile Includes
 *
 * Each product header defines which EOS_ENABLE_* flags are set.
 * If EOS_PRODUCT is not defined, everything is enabled below.
 * ============================================================ */

#if defined(EOS_PRODUCT_ROBOT)
#   include "products/robot.h"
#elif defined(EOS_PRODUCT_VACUUM)
#   include "products/vacuum.h"
#elif defined(EOS_PRODUCT_INDUSTRIAL)
#   include "products/industrial.h"
#elif defined(EOS_PRODUCT_VOICE)
#   include "products/voice.h"
#elif defined(EOS_PRODUCT_AEROSPACE)
#   include "products/aerospace.h"
#elif defined(EOS_PRODUCT_SATELLITE)
#   include "products/satellite.h"
#elif defined(EOS_PRODUCT_MOBILE)
#   include "products/mobile.h"
#elif defined(EOS_PRODUCT_WATCH)
#   include "products/watch.h"
#elif defined(EOS_PRODUCT_ADAPTER)
#   include "products/adapter.h"
#elif defined(EOS_PRODUCT_GATEWAY)
#   include "products/gateway.h"
#elif defined(EOS_PRODUCT_MEDICAL)
#   include "products/medical.h"
#elif defined(EOS_PRODUCT_AUTOMOTIVE)
#   include "products/automotive.h"
#elif defined(EOS_PRODUCT_DRONE)
#   include "products/drone.h"
#elif defined(EOS_PRODUCT_IOT)
#   include "products/iot.h"
#elif defined(EOS_PRODUCT_HMI)
#   include "products/hmi.h"
#elif defined(EOS_PRODUCT_WEARABLE)
#   include "products/wearable.h"
#elif defined(EOS_PRODUCT_POS)
#   include "products/pos.h"
#elif defined(EOS_PRODUCT_PRINTER)
#   include "products/printer.h"
#elif defined(EOS_PRODUCT_COCKPIT)
#   include "products/cockpit.h"
#elif defined(EOS_PRODUCT_BANKING)
#   include "products/banking.h"
#elif defined(EOS_PRODUCT_CRYPTO_HW)
#   include "products/crypto_hw.h"
#elif defined(EOS_PRODUCT_TELECOM)
#   include "products/telecom.h"
#elif defined(EOS_PRODUCT_DIAGNOSTIC)
#   include "products/diagnostic.h"
#elif defined(EOS_PRODUCT_TELEMEDICINE)
#   include "products/telemedicine.h"
#elif defined(EOS_PRODUCT_GROUND_CONTROL)
#   include "products/ground_control.h"
#elif defined(EOS_PRODUCT_SPACE_COMM)
#   include "products/space_comm.h"
#elif defined(EOS_PRODUCT_PLC)
#   include "products/plc.h"
#elif defined(EOS_PRODUCT_AUTONOMOUS)
#   include "products/autonomous.h"
#elif defined(EOS_PRODUCT_INFOTAINMENT)
#   include "products/infotainment.h"
#elif defined(EOS_PRODUCT_THERMOSTAT)
#   include "products/thermostat.h"
#elif defined(EOS_PRODUCT_SECURITY_CAM)
#   include "products/security_cam.h"
#elif defined(EOS_PRODUCT_SMART_HOME)
#   include "products/smart_home.h"
#elif defined(EOS_PRODUCT_XR_HEADSET)
#   include "products/xr_headset.h"
#elif defined(EOS_PRODUCT_COMPUTER)
#   include "products/computer.h"
#elif defined(EOS_PRODUCT_SMART_TV)
#   include "products/smart_tv.h"
#elif defined(EOS_PRODUCT_GAMING)
#   include "products/gaming.h"
#elif defined(EOS_PRODUCT_SERVER)
#   include "products/server.h"
#elif defined(EOS_PRODUCT_AI_EDGE)
#   include "products/ai_edge.h"
#elif defined(EOS_PRODUCT_ROUTER)
#   include "products/router.h"
#elif defined(EOS_PRODUCT_EV)
#   include "products/ev.h"
#elif defined(EOS_PRODUCT_FITNESS)
#   include "products/fitness.h"
#else
/* No product selected — enable everything (development build) */
#   define EOS_PRODUCT_NAME "all"

/* Core peripherals */
#   define EOS_ENABLE_GPIO      1
#   define EOS_ENABLE_UART      1
#   define EOS_ENABLE_SPI       1
#   define EOS_ENABLE_I2C       1
#   define EOS_ENABLE_TIMER     1

/* Extended peripherals */
#   define EOS_ENABLE_ADC       1
#   define EOS_ENABLE_DAC       1
#   define EOS_ENABLE_PWM       1
#   define EOS_ENABLE_CAN       1
#   define EOS_ENABLE_USB       1
#   define EOS_ENABLE_ETHERNET  1
#   define EOS_ENABLE_WIFI      1
#   define EOS_ENABLE_BLE       1
#   define EOS_ENABLE_CAMERA    1
#   define EOS_ENABLE_AUDIO     1
#   define EOS_ENABLE_DISPLAY   1
#   define EOS_ENABLE_MOTOR     1
#   define EOS_ENABLE_GNSS      1
#   define EOS_ENABLE_IMU       1
#   define EOS_ENABLE_TOUCH     1
#   define EOS_ENABLE_RTC       1
#   define EOS_ENABLE_DMA       1
#   define EOS_ENABLE_FLASH     1
#   define EOS_ENABLE_WDT       1
#   define EOS_ENABLE_NFC       1
#   define EOS_ENABLE_IR        1
#   define EOS_ENABLE_CELLULAR  1
#   define EOS_ENABLE_RADAR     1
#   define EOS_ENABLE_GPU       1
#   define EOS_ENABLE_HDMI      1
#   define EOS_ENABLE_PCIE      1
#   define EOS_ENABLE_SDIO      1
#   define EOS_ENABLE_HAPTICS   1

/* UI Framework (requires display) */
#   define EOS_ENABLE_UI        1

/* Multicore */
#   define EOS_ENABLE_MULTICORE 1

/* Services */
#   define EOS_ENABLE_CRYPTO    1
#   define EOS_ENABLE_SECURITY  1
#   define EOS_ENABLE_OTA       1
#   define EOS_ENABLE_WATCHDOG  1
#   define EOS_ENABLE_FILESYSTEM 1

/* Frameworks */
#   define EOS_ENABLE_POWER     1
#   define EOS_ENABLE_NET       1
#   define EOS_ENABLE_SENSOR    1
#   define EOS_ENABLE_MOTOR_CTRL 1

/* Safety & reliability */
#   define EOS_ENABLE_SAFETY    1
#   define EOS_ENABLE_REDUNDANCY 1
#   define EOS_ENABLE_AUDIT     1

/* OS Compatibility Layers */
#   define EOS_ENABLE_COMPAT    1
#   define EOS_ENABLE_POSIX     1
#   define EOS_ENABLE_VXWORKS   1
#   define EOS_ENABLE_LINUX_IPC 1
#endif

#ifndef EOS_ENABLE_MULTICORE
#   define EOS_ENABLE_MULTICORE 0
#endif

/* ============================================================
 * Default disabled — ensure all flags have a value
 * ============================================================ */

#ifndef EOS_ENABLE_GPIO
#   define EOS_ENABLE_GPIO      0
#endif
#ifndef EOS_ENABLE_UART
#   define EOS_ENABLE_UART      0
#endif
#ifndef EOS_ENABLE_SPI
#   define EOS_ENABLE_SPI       0
#endif
#ifndef EOS_ENABLE_I2C
#   define EOS_ENABLE_I2C       0
#endif
#ifndef EOS_ENABLE_TIMER
#   define EOS_ENABLE_TIMER     0
#endif
#ifndef EOS_ENABLE_ADC
#   define EOS_ENABLE_ADC       0
#endif
#ifndef EOS_ENABLE_DAC
#   define EOS_ENABLE_DAC       0
#endif
#ifndef EOS_ENABLE_PWM
#   define EOS_ENABLE_PWM       0
#endif
#ifndef EOS_ENABLE_CAN
#   define EOS_ENABLE_CAN       0
#endif
#ifndef EOS_ENABLE_USB
#   define EOS_ENABLE_USB       0
#endif
#ifndef EOS_ENABLE_ETHERNET
#   define EOS_ENABLE_ETHERNET  0
#endif
#ifndef EOS_ENABLE_WIFI
#   define EOS_ENABLE_WIFI      0
#endif
#ifndef EOS_ENABLE_BLE
#   define EOS_ENABLE_BLE       0
#endif
#ifndef EOS_ENABLE_CAMERA
#   define EOS_ENABLE_CAMERA    0
#endif
#ifndef EOS_ENABLE_AUDIO
#   define EOS_ENABLE_AUDIO     0
#endif
#ifndef EOS_ENABLE_DISPLAY
#   define EOS_ENABLE_DISPLAY   0
#endif
#ifndef EOS_ENABLE_MOTOR
#   define EOS_ENABLE_MOTOR     0
#endif
#ifndef EOS_ENABLE_GNSS
#   define EOS_ENABLE_GNSS      0
#endif
#ifndef EOS_ENABLE_IMU
#   define EOS_ENABLE_IMU       0
#endif
#ifndef EOS_ENABLE_TOUCH
#   define EOS_ENABLE_TOUCH     0
#endif
#ifndef EOS_ENABLE_RTC
#   define EOS_ENABLE_RTC       0
#endif
#ifndef EOS_ENABLE_DMA
#   define EOS_ENABLE_DMA       0
#endif
#ifndef EOS_ENABLE_FLASH
#   define EOS_ENABLE_FLASH     0
#endif
#ifndef EOS_ENABLE_WDT
#   define EOS_ENABLE_WDT       0
#endif
#ifndef EOS_ENABLE_NFC
#   define EOS_ENABLE_NFC       0
#endif
#ifndef EOS_ENABLE_IR
#   define EOS_ENABLE_IR        0
#endif
#ifndef EOS_ENABLE_CELLULAR
#   define EOS_ENABLE_CELLULAR  0
#endif
#ifndef EOS_ENABLE_RADAR
#   define EOS_ENABLE_RADAR     0
#endif
#ifndef EOS_ENABLE_GPU
#   define EOS_ENABLE_GPU       0
#endif
#ifndef EOS_ENABLE_HDMI
#   define EOS_ENABLE_HDMI      0
#endif
#ifndef EOS_ENABLE_PCIE
#   define EOS_ENABLE_PCIE      0
#endif
#ifndef EOS_ENABLE_SDIO
#   define EOS_ENABLE_SDIO      0
#endif
#ifndef EOS_ENABLE_HAPTICS
#   define EOS_ENABLE_HAPTICS   0
#endif
#ifndef EOS_ENABLE_UI
#   define EOS_ENABLE_UI        0
#endif
/* UI requires display — force-disable if display is off */
#if EOS_ENABLE_UI && !EOS_ENABLE_DISPLAY
#   undef  EOS_ENABLE_UI
#   define EOS_ENABLE_UI        0
#endif
#ifndef EOS_ENABLE_CRYPTO
#   define EOS_ENABLE_CRYPTO    0
#endif
#ifndef EOS_ENABLE_SECURITY
#   define EOS_ENABLE_SECURITY  0
#endif
#ifndef EOS_ENABLE_OTA
#   define EOS_ENABLE_OTA       0
#endif
#ifndef EOS_ENABLE_WATCHDOG
#   define EOS_ENABLE_WATCHDOG  0
#endif
#ifndef EOS_ENABLE_FILESYSTEM
#   define EOS_ENABLE_FILESYSTEM 0
#endif
#ifndef EOS_ENABLE_POWER
#   define EOS_ENABLE_POWER     0
#endif
#ifndef EOS_ENABLE_NET
#   define EOS_ENABLE_NET       0
#endif
#ifndef EOS_ENABLE_SENSOR
#   define EOS_ENABLE_SENSOR    0
#endif
#ifndef EOS_ENABLE_MOTOR_CTRL
#   define EOS_ENABLE_MOTOR_CTRL 0
#endif
#ifndef EOS_ENABLE_SAFETY
#   define EOS_ENABLE_SAFETY    0
#endif
#ifndef EOS_ENABLE_REDUNDANCY
#   define EOS_ENABLE_REDUNDANCY 0
#endif
#ifndef EOS_ENABLE_AUDIT
#   define EOS_ENABLE_AUDIT     0
#endif
#ifndef EOS_ENABLE_COMPAT
#   define EOS_ENABLE_COMPAT    0
#endif
#ifndef EOS_ENABLE_POSIX
#   define EOS_ENABLE_POSIX     0
#endif
#ifndef EOS_ENABLE_VXWORKS
#   define EOS_ENABLE_VXWORKS   0
#endif
#ifndef EOS_ENABLE_LINUX_IPC
#   define EOS_ENABLE_LINUX_IPC 0
#endif

#ifndef EOS_PRODUCT_NAME
#   define EOS_PRODUCT_NAME "unknown"
#endif

#endif /* EOS_CONFIG_H */
