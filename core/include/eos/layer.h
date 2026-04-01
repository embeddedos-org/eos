// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EOS_LAYER_H
#define EOS_LAYER_H

#include "eos/types.h"
#include "eos/error.h"

typedef enum {
    EOS_LAYER_CORE,
    EOS_LAYER_BSP,
    EOS_LAYER_DISTRO,
    EOS_LAYER_VENDOR,
    EOS_LAYER_PRODUCT,
    EOS_LAYER_RTOS,
    EOS_LAYER_UNKNOWN
} EosLayerType;

typedef struct {
    char name[EOS_MAX_NAME];
    char path[EOS_MAX_PATH];
    EosLayerType type;
    char description[EOS_MAX_PATH];
    char board[EOS_MAX_NAME];
    char toolchain[EOS_MAX_NAME];
    char provider[EOS_MAX_NAME];
    char provides[EOS_MAX_DEPS][EOS_MAX_NAME];
    int provides_count;
    int loaded;
} EosLayer;

typedef struct {
    EosLayer layers[EOS_MAX_LAYERS];
    int count;
} EosLayerSet;

EosResult eos_layer_load(EosLayer *layer, const char *yaml_path);
EosResult eos_layer_set_load(EosLayerSet *set, const char (*paths)[EOS_MAX_PATH],
                             int path_count);
const EosLayer *eos_layer_find_by_type(const EosLayerSet *set, EosLayerType type);
const EosLayer *eos_layer_find_by_name(const EosLayerSet *set, const char *name);
void eos_layer_set_dump(const EosLayerSet *set);

static inline const char *eos_layer_type_str(EosLayerType t) {
    switch (t) {
        case EOS_LAYER_CORE:    return "core";
        case EOS_LAYER_BSP:     return "bsp";
        case EOS_LAYER_DISTRO:  return "distro";
        case EOS_LAYER_VENDOR:  return "vendor";
        case EOS_LAYER_PRODUCT: return "product";
        case EOS_LAYER_RTOS:    return "rtos";
        default:                  return "unknown";
    }
}

static inline EosLayerType eos_layer_type_from_str(const char *s) {
    if (!s) return EOS_LAYER_UNKNOWN;
    if (strcmp(s, "core") == 0)    return EOS_LAYER_CORE;
    if (strcmp(s, "bsp") == 0)     return EOS_LAYER_BSP;
    if (strcmp(s, "distro") == 0)  return EOS_LAYER_DISTRO;
    if (strcmp(s, "vendor") == 0)  return EOS_LAYER_VENDOR;
    if (strcmp(s, "product") == 0) return EOS_LAYER_PRODUCT;
    if (strcmp(s, "rtos") == 0)    return EOS_LAYER_RTOS;
    return EOS_LAYER_UNKNOWN;
}

#endif /* EOS_LAYER_H */
