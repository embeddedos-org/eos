// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/backend.h"
#include "eos/log.h"
#include <string.h>

static EosBackend *g_backends[EOS_MAX_BACKENDS];
static int g_backend_count = 0;

void eos_backend_registry_init(void) {
    g_backend_count = 0;
    memset(g_backends, 0, sizeof(g_backends));
}

EosResult eos_backend_register(EosBackend *backend) {
    if (g_backend_count >= EOS_MAX_BACKENDS) {
        EOS_ERROR("Backend registry full (max %d)", EOS_MAX_BACKENDS);
        return EOS_ERR_OVERFLOW;
    }

    g_backends[g_backend_count++] = backend;
    EOS_DEBUG("Registered backend: %s", backend->name);
    return EOS_OK;
}

EosBackend *eos_backend_find(const char *name) {
    for (int i = 0; i < g_backend_count; i++) {
        if (strcmp(g_backends[i]->name, name) == 0) {
            return g_backends[i];
        }
    }
    return NULL;
}

EosBackend *eos_backend_find_by_type(EosBuildType type) {
    for (int i = 0; i < g_backend_count; i++) {
        if (g_backends[i]->type == type) {
            return g_backends[i];
        }
    }
    return NULL;
}
