// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/backend.h"
#include "eos/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EosResult autotools_configure(EosBackend *self, const char *src_dir,
                                     const char *build_dir, const char *toolchain_file,
                                     const EosKeyValue *options, int option_count) {
    (void)self;
    (void)build_dir;
    char cmd[2048];
    int offset = snprintf(cmd, sizeof(cmd), "cd \"%s\" && ./configure", src_dir);

    if (toolchain_file && toolchain_file[0]) {
        offset += snprintf(cmd + offset, sizeof(cmd) - (size_t)offset,
                          " --host=%s", toolchain_file);
    }

    for (int i = 0; i < option_count && offset < (int)sizeof(cmd) - 64; i++) {
        offset += snprintf(cmd + offset, sizeof(cmd) - (size_t)offset,
                          " --%s=%s", options[i].key, options[i].value);
    }

    EOS_INFO("Autotools configure: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult autotools_build(EosBackend *self, const char *build_dir, int jobs) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "make -C \"%s\" -j%d", build_dir, jobs > 0 ? jobs : 4);
    EOS_INFO("Autotools build: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult autotools_install(EosBackend *self, const char *build_dir,
                                   const char *install_dir) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "make -C \"%s\" install DESTDIR=\"%s\"",
             build_dir, install_dir);
    EOS_INFO("Autotools install: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult autotools_clean(EosBackend *self, const char *build_dir) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "make -C \"%s\" distclean", build_dir);
    EOS_INFO("Autotools clean: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

void eos_backend_autotools_init(EosBackend *b) {
    memset(b, 0, sizeof(*b));
    strncpy(b->name, "autotools", EOS_MAX_NAME - 1);
    b->type = EOS_BUILD_AUTOTOOLS;
    b->configure = autotools_configure;
    b->build = autotools_build;
    b->install = autotools_install;
    b->clean = autotools_clean;
}
