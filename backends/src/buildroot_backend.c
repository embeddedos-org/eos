// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/backend.h"
#include "eos/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EosResult buildroot_configure(EosBackend *self, const char *src_dir,
                                     const char *build_dir, const char *toolchain_file,
                                     const EosKeyValue *options, int option_count) {
    (void)self;
    char cmd[2048];

    const char *defconfig = "qemu_aarch64_virt_defconfig";
    for (int i = 0; i < option_count; i++) {
        if (strcmp(options[i].key, "defconfig") == 0) {
            defconfig = options[i].value;
            break;
        }
    }

    int offset = snprintf(cmd, sizeof(cmd),
                          "make -C \"%s\" O=\"%s\" %s",
                          src_dir, build_dir, defconfig);

    if (toolchain_file && toolchain_file[0]) {
        offset += snprintf(cmd + offset, sizeof(cmd) - (size_t)offset,
                          " BR2_TOOLCHAIN_EXTERNAL_PATH=\"%s\"", toolchain_file);
    }

    EOS_INFO("Buildroot configure: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult buildroot_build(EosBackend *self, const char *build_dir, int jobs) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "make -C \"%s\" -j%d", build_dir, jobs > 0 ? jobs : 4);
    EOS_INFO("Buildroot build: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult buildroot_install(EosBackend *self, const char *build_dir,
                                   const char *install_dir) {
    (void)self;
    char cmd[1024];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd),
             "if not exist \"%s\" mkdir \"%s\" && copy /Y \"%s\\images\\*\" \"%s\\\"",
             install_dir, install_dir, build_dir, install_dir);
#else
    snprintf(cmd, sizeof(cmd),
             "mkdir -p \"%s\" && cp -r \"%s/images/\"* \"%s/\" 2>/dev/null || true",
             install_dir, build_dir, install_dir);
#endif
    EOS_INFO("Buildroot install: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult buildroot_clean(EosBackend *self, const char *build_dir) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "make -C \"%s\" clean", build_dir);
    EOS_INFO("Buildroot clean: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

void eos_backend_buildroot_init(EosBackend *b) {
    memset(b, 0, sizeof(*b));
    strncpy(b->name, "buildroot", EOS_MAX_NAME - 1);
    b->type = EOS_BUILD_KBUILD;
    b->configure = buildroot_configure;
    b->build = buildroot_build;
    b->install = buildroot_install;
    b->clean = buildroot_clean;
}
