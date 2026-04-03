// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/layer.h"
#include "eos/log.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static char *ly_trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

EosResult eos_layer_load(EosLayer *layer, const char *yaml_path) {
    FILE *fp = fopen(yaml_path, "r");
    if (!fp) {
        EOS_WARN("Cannot open layer: %s", yaml_path);
        layer->loaded = 0;
        return EOS_ERR_IO;
    }

    char line[1024];
    int in_provides = 0;

    while (fgets(line, sizeof(line), fp)) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        nl = strchr(line, '\r');
        if (nl) *nl = '\0';

        char *t = ly_trim(line);
        if (t[0] == '\0' || t[0] == '#') continue;

        /* List item inside provides: */
        if (t[0] == '-' && in_provides) {
            char *item = ly_trim(t + 1);
            if (layer->provides_count < EOS_MAX_DEPS) {
                strncpy(layer->provides[layer->provides_count], item, EOS_MAX_NAME - 1);
                layer->provides_count++;
            }
            continue;
        }

        char *colon = strchr(t, ':');
        if (!colon) continue;

        *colon = '\0';
        char *key = ly_trim(t);
        char *val = ly_trim(colon + 1);

        in_provides = 0;

        if (strcmp(key, "name") == 0)        strncpy(layer->name, val, EOS_MAX_NAME - 1);
        else if (strcmp(key, "type") == 0)    layer->type = eos_layer_type_from_str(val);
        else if (strcmp(key, "description") == 0) strncpy(layer->description, val, EOS_MAX_PATH - 1);
        else if (strcmp(key, "board") == 0)   strncpy(layer->board, val, EOS_MAX_NAME - 1);
        else if (strcmp(key, "toolchain") == 0) strncpy(layer->toolchain, val, EOS_MAX_NAME - 1);
        else if (strcmp(key, "provider") == 0) strncpy(layer->provider, val, EOS_MAX_NAME - 1);
        else if (strcmp(key, "provides") == 0) in_provides = 1;
    }

    fclose(fp);
    strncpy(layer->path, yaml_path, EOS_MAX_PATH - 1);
    layer->loaded = 1;

    EOS_DEBUG("Layer loaded: %s (%s, type=%s)",
              layer->name, yaml_path, eos_layer_type_str(layer->type));
    return EOS_OK;
}

EosResult eos_layer_set_load(EosLayerSet *set, const char (*paths)[EOS_MAX_PATH],
                             int path_count) {
    memset(set, 0, sizeof(*set));

    for (int i = 0; i < path_count && i < EOS_MAX_LAYERS; i++) {
        char yaml_path[EOS_MAX_PATH];
        snprintf(yaml_path, sizeof(yaml_path), "%s/layer.yaml", paths[i]);

        EosLayer *layer = &set->layers[set->count];
        memset(layer, 0, sizeof(*layer));
        layer->type = EOS_LAYER_UNKNOWN;

        EosResult res = eos_layer_load(layer, yaml_path);
        if (res == EOS_OK) {
            set->count++;
        } else {
            /* Try to infer type from path */
            if (strstr(paths[i], "/bsp/"))         layer->type = EOS_LAYER_BSP;
            else if (strstr(paths[i], "/distro/"))  layer->type = EOS_LAYER_DISTRO;
            else if (strstr(paths[i], "/vendor/"))  layer->type = EOS_LAYER_VENDOR;
            else if (strstr(paths[i], "/product/")) layer->type = EOS_LAYER_PRODUCT;
            else if (strstr(paths[i], "/rtos/"))    layer->type = EOS_LAYER_RTOS;
            else if (strstr(paths[i], "/core"))     layer->type = EOS_LAYER_CORE;

            strncpy(layer->path, paths[i], EOS_MAX_PATH - 1);
            const char *last_slash = strrchr(paths[i], '/');
            if (last_slash)
                strncpy(layer->name, last_slash + 1, EOS_MAX_NAME - 1);
            layer->loaded = 0;
            set->count++;
            EOS_WARN("Layer %s: yaml not found, inferred type=%s",
                     paths[i], eos_layer_type_str(layer->type));
        }
    }

    EOS_INFO("Loaded %d layers", set->count);
    return EOS_OK;
}

const EosLayer *eos_layer_find_by_type(const EosLayerSet *set, EosLayerType type) {
    for (int i = 0; i < set->count; i++) {
        if (set->layers[i].type == type) return &set->layers[i];
    }
    return NULL;
}

const EosLayer *eos_layer_find_by_name(const EosLayerSet *set, const char *name) {
    for (int i = 0; i < set->count; i++) {
        if (strcmp(set->layers[i].name, name) == 0) return &set->layers[i];
    }
    return NULL;
}

void eos_layer_set_dump(const EosLayerSet *set) {
    printf("Layers (%d):\n", set->count);
    for (int i = 0; i < set->count; i++) {
        const EosLayer *l = &set->layers[i];
        printf("  [%d] %s (%s) %s\n", i, l->name,
               eos_layer_type_str(l->type),
               l->loaded ? "" : "[not loaded]");
        if (l->description[0])  printf("       desc: %s\n", l->description);
        if (l->board[0])        printf("       board: %s\n", l->board);
        if (l->toolchain[0])    printf("       toolchain: %s\n", l->toolchain);
        if (l->provider[0])     printf("       provider: %s\n", l->provider);
        if (l->provides_count > 0) {
            printf("       provides:");
            for (int p = 0; p < l->provides_count; p++)
                printf(" %s", l->provides[p]);
            printf("\n");
        }
    }
}
