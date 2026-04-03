// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/backend.h"
#include "eos/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EosResult ninja_configure(EosBackend *self, const char *src_dir,
                                    const char *build_dir, const char *toolchain_file,
                                    const EosKeyValue *options, int option_count) {
    (void)self;
    (void)src_dir;
    (void)build_dir;
    (void)toolchain_file;
    (void)options;
    (void)option_count;
    EOS_DEBUG("Ninja: no separate configure step (expects build.ninja in place)");
    return EOS_OK;
}

static EosResult ninja_build(EosBackend *self, const char *build_dir, int jobs) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "ninja -C \"%s\" -j %d", build_dir, jobs > 0 ? jobs : 4);
    EOS_INFO("Ninja build: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult ninja_install(EosBackend *self, const char *build_dir,
                                  const char *install_dir) {
    (void)self;
    (void)install_dir;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "ninja -C \"%s\" install", build_dir);
    EOS_INFO("Ninja install: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult ninja_clean(EosBackend *self, const char *build_dir) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "ninja -C \"%s\" -t clean", build_dir);
    EOS_INFO("Ninja clean: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

void eos_backend_ninja_init(EosBackend *b) {
    memset(b, 0, sizeof(*b));
    strncpy(b->name, "ninja", EOS_MAX_NAME - 1);
    b->type = EOS_BUILD_NINJA;
    b->configure = ninja_configure;
    b->build = ninja_build;
    b->install = ninja_install;
    b->clean = ninja_clean;
}
