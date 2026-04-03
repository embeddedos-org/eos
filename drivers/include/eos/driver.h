// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EOS_DRIVER_H
#define EOS_DRIVER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EOS_DRV_MAX_DRIVERS 64
#define EOS_DRV_NAME_MAX    32
#define EOS_DRV_VERSION     1

typedef enum {
    EOS_DRV_CLASS_GPIO     = 0,
    EOS_DRV_CLASS_UART     = 1,
    EOS_DRV_CLASS_SPI      = 2,
    EOS_DRV_CLASS_I2C      = 3,
    EOS_DRV_CLASS_CAN      = 4,
    EOS_DRV_CLASS_PWM      = 5,
    EOS_DRV_CLASS_ADC      = 6,
    EOS_DRV_CLASS_DAC      = 7,
    EOS_DRV_CLASS_TIMER    = 8,
    EOS_DRV_CLASS_DMA      = 9,
    EOS_DRV_CLASS_ETHERNET = 10,
    EOS_DRV_CLASS_USB      = 11,
    EOS_DRV_CLASS_SDIO     = 12,
    EOS_DRV_CLASS_DISPLAY  = 13,
    EOS_DRV_CLASS_SENSOR   = 14,
    EOS_DRV_CLASS_STORAGE  = 15,
    EOS_DRV_CLASS_NETWORK  = 16,
    EOS_DRV_CLASS_AUDIO    = 17,
    EOS_DRV_CLASS_CAMERA   = 18,
    EOS_DRV_CLASS_CUSTOM   = 0xFF
} EosDrvClass;

typedef enum {
    EOS_DRV_UNLOADED   = 0,
    EOS_DRV_LOADED     = 1,
    EOS_DRV_PROBING    = 2,
    EOS_DRV_BOUND      = 3,
    EOS_DRV_ERROR      = 4
} EosDrvState;

typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint32_t compat_hash;
    char     compat_str[32];
} EosDrvMatchInfo;

typedef struct eos_driver {
    char            name[EOS_DRV_NAME_MAX];
    uint32_t        version;
    EosDrvClass     class_id;
    EosDrvState     state;
    EosDrvMatchInfo match;

    /* Driver operations */
    int  (*probe)(struct eos_driver *drv, void *platform_data);
    void (*remove)(struct eos_driver *drv);
    int  (*suspend)(struct eos_driver *drv);
    int  (*resume)(struct eos_driver *drv);
    int  (*ioctl)(struct eos_driver *drv, uint32_t cmd, void *arg);

    /* Runtime data */
    void   *priv_data;
    void   *platform_data;
    int     ref_count;
    int     irq_num;
    uint32_t base_addr;
    uint32_t reg_size;
} EosDriver;

typedef struct {
    EosDriver *drivers[EOS_DRV_MAX_DRIVERS];
    int        count;
    int        initialized;
} EosDrvRegistry;

/* Registry lifecycle */
int  eos_drv_init(EosDrvRegistry *reg);
void eos_drv_shutdown(EosDrvRegistry *reg);

/* Driver registration (load/unload) */
int  eos_drv_register(EosDrvRegistry *reg, EosDriver *drv);
int  eos_drv_unregister(EosDrvRegistry *reg, const char *name);

/* Probe / bind / unbind */
int  eos_drv_probe(EosDrvRegistry *reg, const char *name, void *platform_data);
int  eos_drv_probe_all(EosDrvRegistry *reg);
int  eos_drv_unbind(EosDrvRegistry *reg, const char *name);

/* Lookup */
EosDriver *eos_drv_find(EosDrvRegistry *reg, const char *name);
EosDriver *eos_drv_find_by_class(EosDrvRegistry *reg, EosDrvClass class_id, int index);
EosDriver *eos_drv_find_by_match(EosDrvRegistry *reg, const EosDrvMatchInfo *match);

/* Power management */
int  eos_drv_suspend_all(EosDrvRegistry *reg);
int  eos_drv_resume_all(EosDrvRegistry *reg);

/* Introspection */
int  eos_drv_count(EosDrvRegistry *reg);
int  eos_drv_count_by_class(EosDrvRegistry *reg, EosDrvClass class_id);
void eos_drv_dump(EosDrvRegistry *reg);

/* I/O control */
int  eos_drv_ioctl(EosDrvRegistry *reg, const char *name, uint32_t cmd, void *arg);

#ifdef __cplusplus
}
#endif

#endif /* EOS_DRIVER_H */