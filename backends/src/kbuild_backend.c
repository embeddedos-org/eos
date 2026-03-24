// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/backend.h"
#include "eos/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EosResult kbuild_configure(EosBackend *self, const char *src_dir,
                                     const char *build_dir, const char *toolchain_file,
                                     const EosKeyValue *options, int option_count) {
    (void)self;
    (void)build_dir;
    (void)options;
    (void)option_count;

    /* Find defconfig from options */
    const char *defconfig = "defconfig";
    for (int i = 0; i < option_count; i++) {
        if (strcmp(options[i].key, "defconfig") == 0) {
            defconfig = options[i].value;
            break;
        }
    }

    char cmd[2048];
    if (toolchain_file && toolchain_file[0]) {
        snprintf(cmd, sizeof(cmd),
                 "make -C \"%s\" O=\"%s\" ARCH=%s CROSS_COMPILE=%s- %s",
                 src_dir, build_dir, "arm64", toolchain_file, defconfig);
    } else {
        snprintf(cmd, sizeof(cmd), "make -C \"%s\" O=\"%s\" %s",
                 src_dir, build_dir, defconfig);
    }

    EOS_INFO("Kbuild configure: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult kbuild_build(EosBackend *self, const char *build_dir, int jobs) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "make -C \"%s\" -j%d", build_dir, jobs > 0 ? jobs : 4);
    EOS_INFO("Kbuild build: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult kbuild_install(EosBackend *self, const char *build_dir,
                                   const char *install_dir) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "make -C \"%s\" install INSTALL_PATH=\"%s\"",
             build_dir, install_dir);
    EOS_INFO("Kbuild install: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult kbuild_clean(EosBackend *self, const char *build_dir) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "make -C \"%s\" mrproper", build_dir);
    EOS_INFO("Kbuild clean: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

void eos_backend_kbuild_init(EosBackend *b) {
    memset(b, 0, sizeof(*b));
    strncpy(b->name, "kbuild", EOS_MAX_NAME - 1);
    b->type = EOS_BUILD_KBUILD;
    b->configure = kbuild_configure;
    b->build = kbuild_build;
    b->install = kbuild_install;
    b->clean = kbuild_clean;
}
