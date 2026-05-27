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

/* Include product profile if defined */
#if defined(EOS_PRODUCT_COMPUTER)
#include "products/computer.h"
#elif defined(EOS_PRODUCT_VBOX_TEST)
#include "products/vbox_test.h"
#elif defined(EOS_PRODUCT_ROBOT)
#include "products/robot.h"
#elif defined(EOS_PRODUCT_VACUUM)
#include "products/vacuum.h"
#elif defined(EOS_PRODUCT_INDUSTRIAL)
#include "products/industrial.h"
#elif defined(EOS_PRODUCT_VOICE)
#include "products/voice.h"
#elif defined(EOS_PRODUCT_AEROSPACE)
#include "products/aerospace.h"
#elif defined(EOS_PRODUCT_SATELLITE)
#include "products/satellite.h"
#elif defined(EOS_PRODUCT_MOBILE)
#include "products/mobile.h"
#elif defined(EOS_PRODUCT_WATCH)
#include "products/watch.h"
#elif defined(EOS_PRODUCT_ADAPTER)
#include "products/adapter.h"
#elif defined(EOS_PRODUCT_GATEWAY)
#include "products/gateway.h"
#elif defined(EOS_PRODUCT_MEDICAL)
#include "products/medical.h"
#elif defined(EOS_PRODUCT_AUTOMOTIVE)
#include "products/automotive.h"
#elif defined(EOS_PRODUCT_DRONE)
#include "products/drone.h"
#elif defined(EOS_PRODUCT_IOT)
#include "products/iot.h"
#elif defined(EOS_PRODUCT_HMI)
#include "products/hmi.h"
#elif defined(EOS_PRODUCT_WEARABLE)
#include "products/wearable.h"
#elif defined(EOS_PRODUCT_POS)
#include "products/pos.h"
#elif defined(EOS_PRODUCT_PRINTER)
#include "products/printer.h"
#elif defined(EOS_PRODUCT_COCKPIT)
#include "products/cockpit.h"
#elif defined(EOS_PRODUCT_BANKING)
#include "products/banking.h"
#elif defined(EOS_PRODUCT_CRYPTO_HW)
#include "products/crypto_hw.h"
#elif defined(EOS_PRODUCT_TELECOM)
#include "products/telecom.h"
#elif defined(EOS_PRODUCT_DIAGNOSTIC)
#include "products/diagnostic.h"
#elif defined(EOS_PRODUCT_TELEMEDICINE)
#include "products/telemedicine.h"
#elif defined(EOS_PRODUCT_GROUND_CONTROL)
#include "products/ground_control.h"
#elif defined(EOS_PRODUCT_SPACE_COMM)
#include "products/space_comm.h"
#elif defined(EOS_PRODUCT_PLC)
#include "products/plc.h"
#elif defined(EOS_PRODUCT_AUTONOMOUS)
#include "products/autonomous.h"
#elif defined(EOS_PRODUCT_INFOTAINMENT)
#include "products/infotainment.h"
#elif defined(EOS_PRODUCT_THERMOSTAT)
#include "products/thermostat.h"
#elif defined(EOS_PRODUCT_SECURITY_CAM)
#include "products/security_cam.h"
#elif defined(EOS_PRODUCT_SMART_HOME)
#include "products/smart_home.h"
#elif defined(EOS_PRODUCT_XR_HEADSET)
#include "products/xr_headset.h"
#elif defined(EOS_PRODUCT_SMART_TV)
#include "products/smart_tv.h"
#elif defined(EOS_PRODUCT_GAMING)
#include "products/gaming.h"
#elif defined(EOS_PRODUCT_SERVER)
#include "products/server.h"
#elif defined(EOS_PRODUCT_AI_EDGE)
#include "products/ai_edge.h"
#elif defined(EOS_PRODUCT_ROUTER)
#include "products/router.h"
#elif defined(EOS_PRODUCT_EV)
#include "products/ev.h"
#elif defined(EOS_PRODUCT_FITNESS)
#include "products/fitness.h"
#endif

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
