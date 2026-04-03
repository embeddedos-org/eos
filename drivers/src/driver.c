// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/driver.h"
#include <string.h>
#include <stdio.h>

int eos_drv_init(EosDrvRegistry *reg) {
    if (!reg) return -1;
    memset(reg, 0, sizeof(*reg));
    reg->initialized = 1;
    return 0;
}

void eos_drv_shutdown(EosDrvRegistry *reg) {
    if (!reg) return;
    for (int i = reg->count - 1; i >= 0; i--) {
        if (reg->drivers[i] && reg->drivers[i]->state == EOS_DRV_BOUND) {
            eos_drv_unbind(reg, reg->drivers[i]->name);
        }
    }
    reg->initialized = 0;
    reg->count = 0;
}

EosDriver *eos_drv_find(EosDrvRegistry *reg, const char *name) {
    if (!reg || !name) return NULL;
    for (int i = 0; i < reg->count; i++) {
        if (reg->drivers[i] && strcmp(reg->drivers[i]->name, name) == 0)
            return reg->drivers[i];
    }
    return NULL;
}

int eos_drv_register(EosDrvRegistry *reg, EosDriver *drv) {
    if (!reg || !drv || !reg->initialized) return -1;
    if (reg->count >= EOS_DRV_MAX_DRIVERS) return -1;
    if (eos_drv_find(reg, drv->name)) return -1;

    drv->state = EOS_DRV_LOADED;
    drv->ref_count = 0;
    reg->drivers[reg->count++] = drv;
    return 0;
}

int eos_drv_unregister(EosDrvRegistry *reg, const char *name) {
    if (!reg || !name) return -1;
    for (int i = 0; i < reg->count; i++) {
        if (reg->drivers[i] && strcmp(reg->drivers[i]->name, name) == 0) {
            if (reg->drivers[i]->state == EOS_DRV_BOUND)
                eos_drv_unbind(reg, name);
            reg->drivers[i]->state = EOS_DRV_UNLOADED;
            reg->drivers[i] = reg->drivers[--reg->count];
            return 0;
        }
    }
    return -1;
}

int eos_drv_probe(EosDrvRegistry *reg, const char *name, void *platform_data) {
    EosDriver *drv = eos_drv_find(reg, name);
    if (!drv) return -1;
    if (drv->state == EOS_DRV_BOUND) return 0;

    drv->state = EOS_DRV_PROBING;
    drv->platform_data = platform_data;
    int ret = 0;
    if (drv->probe) {
        ret = drv->probe(drv, platform_data);
    }
    drv->state = (ret == 0) ? EOS_DRV_BOUND : EOS_DRV_ERROR;
    if (ret == 0) drv->ref_count++;
    return ret;
}

int eos_drv_probe_all(EosDrvRegistry *reg) {
    if (!reg) return -1;
    int ok = 0;
    for (int i = 0; i < reg->count; i++) {
        if (reg->drivers[i] && reg->drivers[i]->state == EOS_DRV_LOADED) {
            if (eos_drv_probe(reg, reg->drivers[i]->name, NULL) == 0)
                ok++;
        }
    }
    return ok;
}

int eos_drv_unbind(EosDrvRegistry *reg, const char *name) {
    EosDriver *drv = eos_drv_find(reg, name);
    if (!drv || drv->state != EOS_DRV_BOUND) return -1;
    if (drv->remove)
        drv->remove(drv);
    drv->state = EOS_DRV_LOADED;
    drv->ref_count = 0;
    return 0;
}

EosDriver *eos_drv_find_by_class(EosDrvRegistry *reg, EosDrvClass class_id, int index) {
    if (!reg) return NULL;
    int count = 0;
    for (int i = 0; i < reg->count; i++) {
        if (reg->drivers[i] && reg->drivers[i]->class_id == class_id) {
            if (count == index) return reg->drivers[i];
            count++;
        }
    }
    return NULL;
}

EosDriver *eos_drv_find_by_match(EosDrvRegistry *reg, const EosDrvMatchInfo *match) {
    if (!reg || !match) return NULL;
    for (int i = 0; i < reg->count; i++) {
        EosDriver *drv = reg->drivers[i];
        if (!drv) continue;
        if (match->vendor_id && match->device_id) {
            if (drv->match.vendor_id == match->vendor_id &&
                drv->match.device_id == match->device_id)
                return drv;
        }
        if (match->compat_str[0]) {
            if (strcmp(drv->match.compat_str, match->compat_str) == 0)
                return drv;
        }
    }
    return NULL;
}

int eos_drv_suspend_all(EosDrvRegistry *reg) {
    if (!reg) return -1;
    int ok = 0;
    for (int i = reg->count - 1; i >= 0; i--) {
        if (reg->drivers[i] && reg->drivers[i]->state == EOS_DRV_BOUND &&
            reg->drivers[i]->suspend) {
            if (reg->drivers[i]->suspend(reg->drivers[i]) == 0) ok++;
        }
    }
    return ok;
}

int eos_drv_resume_all(EosDrvRegistry *reg) {
    if (!reg) return -1;
    int ok = 0;
    for (int i = 0; i < reg->count; i++) {
        if (reg->drivers[i] && reg->drivers[i]->state == EOS_DRV_BOUND &&
            reg->drivers[i]->resume) {
            if (reg->drivers[i]->resume(reg->drivers[i]) == 0) ok++;
        }
    }
    return ok;
}

int eos_drv_count(EosDrvRegistry *reg) {
    return reg ? reg->count : 0;
}

int eos_drv_count_by_class(EosDrvRegistry *reg, EosDrvClass class_id) {
    if (!reg) return 0;
    int count = 0;
    for (int i = 0; i < reg->count; i++) {
        if (reg->drivers[i] && reg->drivers[i]->class_id == class_id)
            count++;
    }
    return count;
}

int eos_drv_ioctl(EosDrvRegistry *reg, const char *name, uint32_t cmd, void *arg) {
    EosDriver *drv = eos_drv_find(reg, name);
    if (!drv || drv->state != EOS_DRV_BOUND || !drv->ioctl) return -1;
    return drv->ioctl(drv, cmd, arg);
}

static const char *state_str(EosDrvState s) {
    switch (s) {
    case EOS_DRV_UNLOADED: return "unloaded";
    case EOS_DRV_LOADED:   return "loaded";
    case EOS_DRV_PROBING:  return "probing";
    case EOS_DRV_BOUND:    return "bound";
    case EOS_DRV_ERROR:    return "error";
    default:               return "???";
    }
}

static const char *class_str(EosDrvClass c) {
    switch (c) {
    case EOS_DRV_CLASS_GPIO:     return "gpio";
    case EOS_DRV_CLASS_UART:     return "uart";
    case EOS_DRV_CLASS_SPI:      return "spi";
    case EOS_DRV_CLASS_I2C:      return "i2c";
    case EOS_DRV_CLASS_CAN:      return "can";
    case EOS_DRV_CLASS_PWM:      return "pwm";
    case EOS_DRV_CLASS_ADC:      return "adc";
    case EOS_DRV_CLASS_DAC:      return "dac";
    case EOS_DRV_CLASS_TIMER:    return "timer";
    case EOS_DRV_CLASS_DMA:      return "dma";
    case EOS_DRV_CLASS_ETHERNET: return "ethernet";
    case EOS_DRV_CLASS_USB:      return "usb";
    case EOS_DRV_CLASS_SDIO:     return "sdio";
    case EOS_DRV_CLASS_DISPLAY:  return "display";
    case EOS_DRV_CLASS_SENSOR:   return "sensor";
    case EOS_DRV_CLASS_STORAGE:  return "storage";
    case EOS_DRV_CLASS_NETWORK:  return "network";
    case EOS_DRV_CLASS_AUDIO:    return "audio";
    case EOS_DRV_CLASS_CAMERA:   return "camera";
    default:                     return "custom";
    }
}

void eos_drv_dump(EosDrvRegistry *reg) {
    if (!reg) return;
    fprintf(stderr, "=== EoS Driver Registry (%d drivers) ===\n", reg->count);
    for (int i = 0; i < reg->count; i++) {
        EosDriver *drv = reg->drivers[i];
        if (!drv) continue;
        fprintf(stderr, "  %-20s  %-8s  %-8s  refs:%d",
                drv->name, class_str(drv->class_id), state_str(drv->state), drv->ref_count);
        if (drv->base_addr)
            fprintf(stderr, "  base:0x%08X", drv->base_addr);
        if (drv->match.compat_str[0])
            fprintf(stderr, "  compat:%s", drv->match.compat_str);
        fprintf(stderr, "\n");
    }
}