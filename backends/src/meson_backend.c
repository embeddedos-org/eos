// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/backend.h"
#include "eos/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EosResult meson_configure(EosBackend *self, const char *src_dir,
                                 const char *build_dir, const char *toolchain_file,
                                 const EosKeyValue *options, int option_count) {
    (void)self;
    char cmd[2048];
    int offset = snprintf(cmd, sizeof(cmd), "meson setup \"%s\" \"%s\"",
                          build_dir, src_dir);

    if (toolchain_file && toolchain_file[0]) {
        offset += snprintf(cmd + offset, sizeof(cmd) - (size_t)offset,
                          " --cross-file \"%s\"", toolchain_file);
    }

    for (int i = 0; i < option_count && offset < (int)sizeof(cmd) - 64; i++) {
        offset += snprintf(cmd + offset, sizeof(cmd) - (size_t)offset,
                          " -D%s=%s", options[i].key, options[i].value);
    }

    EOS_INFO("Meson configure: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult meson_build(EosBackend *self, const char *build_dir, int jobs) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "ninja -C \"%s\" -j %d", build_dir, jobs > 0 ? jobs : 4);
    EOS_INFO("Meson build: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult meson_install(EosBackend *self, const char *build_dir,
                               const char *install_dir) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "DESTDIR=\"%s\" ninja -C \"%s\" install",
             install_dir, build_dir);
    EOS_INFO("Meson install: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult meson_clean(EosBackend *self, const char *build_dir) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "ninja -C \"%s\" -t clean", build_dir);
    EOS_INFO("Meson clean: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

void eos_backend_meson_init(EosBackend *b) {
    memset(b, 0, sizeof(*b));
    strncpy(b->name, "meson", EOS_MAX_NAME - 1);
    b->type = EOS_BUILD_MAKE;
    b->configure = meson_configure;
    b->build = meson_build;
    b->install = meson_install;
    b->clean = meson_clean;
}
