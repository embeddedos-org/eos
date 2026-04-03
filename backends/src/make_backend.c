// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/backend.h"
#include "eos/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EosResult make_configure(EosBackend *self, const char *src_dir,
                                   const char *build_dir, const char *toolchain_file,
                                   const EosKeyValue *options, int option_count) {
    (void)self;
    (void)build_dir;
    (void)toolchain_file;
    (void)options;
    (void)option_count;

    /* Check for configure script (autotools-style) */
    char configure_path[EOS_MAX_PATH];
    snprintf(configure_path, sizeof(configure_path), "%s/configure", src_dir);

    FILE *fp = fopen(configure_path, "r");
    if (fp) {
        fclose(fp);
        char cmd[2048];
        int offset = snprintf(cmd, sizeof(cmd), "cd \"%s\" && ./configure", src_dir);

        if (toolchain_file && toolchain_file[0]) {
            offset += snprintf(cmd + offset, sizeof(cmd) - (size_t)offset,
                              " --host=%s", toolchain_file);
        }

        EOS_INFO("Make configure: %s", cmd);
        int rc = system(cmd);
        return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
    }

    EOS_DEBUG("Make: no configure script found, skipping configure");
    return EOS_OK;
}

static EosResult make_build(EosBackend *self, const char *build_dir, int jobs) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "make -C \"%s\" -j%d", build_dir, jobs > 0 ? jobs : 4);
    EOS_INFO("Make build: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult make_install(EosBackend *self, const char *build_dir,
                                 const char *install_dir) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "make -C \"%s\" install DESTDIR=\"%s\"",
             build_dir, install_dir);
    EOS_INFO("Make install: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult make_clean(EosBackend *self, const char *build_dir) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "make -C \"%s\" clean", build_dir);
    EOS_INFO("Make clean: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

void eos_backend_make_init(EosBackend *b) {
    memset(b, 0, sizeof(*b));
    strncpy(b->name, "make", EOS_MAX_NAME - 1);
    b->type = EOS_BUILD_MAKE;
    b->configure = make_configure;
    b->build = make_build;
    b->install = make_install;
    b->clean = make_clean;
}
