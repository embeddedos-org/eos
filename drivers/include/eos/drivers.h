// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file drivers.h
 * @brief EoS Driver Registration Framework
 *
 * Provides a pluggable driver model for registering, initializing, and
 * managing peripheral drivers across platforms.
 */

#ifndef EOS_DRIVERS_H
#define EOS_DRIVERS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Driver Types
 * ============================================================ */

typedef enum {
    EOS_DRIVER_GPIO   = 0,
    EOS_DRIVER_UART   = 1,
    EOS_DRIVER_SPI    = 2,
    EOS_DRIVER_I2C    = 3,
    EOS_DRIVER_TIMER  = 4,
    EOS_DRIVER_ADC    = 5,
    EOS_DRIVER_DAC    = 6,
    EOS_DRIVER_PWM    = 7,
    EOS_DRIVER_DMA    = 8,
    EOS_DRIVER_USB    = 9,
    EOS_DRIVER_CAN    = 10,
    EOS_DRIVER_ETH    = 11,
    EOS_DRIVER_FLASH  = 12,
    EOS_DRIVER_RTC    = 13,
    EOS_DRIVER_WDG    = 14,
    EOS_DRIVER_CUSTOM = 0xFF,
} eos_driver_type_t;

typedef enum {
    EOS_DRV_STATE_UNINIT  = 0,
    EOS_DRV_STATE_READY   = 1,
    EOS_DRV_STATE_ACTIVE  = 2,
    EOS_DRV_STATE_ERROR   = 3,
    EOS_DRV_STATE_SUSPEND = 4,
} eos_driver_state_t;

/* ============================================================
 * Driver Operations Interface
 * ============================================================ */

struct eos_driver;

typedef struct {
    int  (*init)(struct eos_driver *drv);
    void (*deinit)(struct eos_driver *drv);
    int  (*open)(struct eos_driver *drv);
    int  (*close)(struct eos_driver *drv);
    int  (*read)(struct eos_driver *drv, void *buf, size_t len);
    int  (*write)(struct eos_driver *drv, const void *buf, size_t len);
    int  (*ioctl)(struct eos_driver *drv, uint32_t cmd, void *arg);
    int  (*suspend)(struct eos_driver *drv);
    int  (*resume)(struct eos_driver *drv);
} eos_driver_ops_t;

/* ============================================================
 * Driver Descriptor
 * ============================================================ */

typedef struct eos_driver {
    const char *name;
    eos_driver_type_t type;
    eos_driver_state_t state;
    const eos_driver_ops_t *ops;
    void *priv_data;
    uint8_t instance;
    uint32_t flags;
} eos_driver_t;

/* ============================================================
 * Driver Framework Configuration
 * ============================================================ */

#ifndef EOS_MAX_DRIVERS
#define EOS_MAX_DRIVERS 32
#endif

/* ============================================================
 * Driver Framework API
 * ============================================================ */

int  eos_driver_register(eos_driver_t *drv);
int  eos_driver_unregister(const char *name);

eos_driver_t *eos_driver_find(const char *name);
eos_driver_t *eos_driver_find_by_type(eos_driver_type_t type, uint8_t instance);

int  eos_driver_init_all(void);
void eos_driver_deinit_all(void);

int  eos_driver_open(eos_driver_t *drv);
int  eos_driver_close(eos_driver_t *drv);
int  eos_driver_read(eos_driver_t *drv, void *buf, size_t len);
int  eos_driver_write(eos_driver_t *drv, const void *buf, size_t len);
int  eos_driver_ioctl(eos_driver_t *drv, uint32_t cmd, void *arg);

int  eos_driver_suspend_all(void);
int  eos_driver_resume_all(void);

uint32_t eos_driver_count(void);
void     eos_driver_list(eos_driver_t **out, uint32_t max, uint32_t *count);

#ifdef __cplusplus
}
#endif

#endif /* EOS_DRIVERS_H */
