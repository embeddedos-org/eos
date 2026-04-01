// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/backend.h"
#include "eos/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EosResult cmake_configure(EosBackend *self, const char *src_dir,
                                    const char *build_dir, const char *toolchain_file,
                                    const EosKeyValue *options, int option_count) {
    (void)self;
    char cmd[2048];
    int offset = snprintf(cmd, sizeof(cmd), "cmake -S \"%s\" -B \"%s\" -G Ninja",
                          src_dir, build_dir);

    if (toolchain_file && toolchain_file[0]) {
        offset += snprintf(cmd + offset, sizeof(cmd) - (size_t)offset,
                          " -DCMAKE_TOOLCHAIN_FILE=\"%s\"", toolchain_file);
    }

    for (int i = 0; i < option_count && offset < (int)sizeof(cmd) - 64; i++) {
        offset += snprintf(cmd + offset, sizeof(cmd) - (size_t)offset,
                          " -D%s=%s", options[i].key, options[i].value);
    }

    EOS_INFO("CMake configure: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult cmake_build(EosBackend *self, const char *build_dir, int jobs) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cmake --build \"%s\" -j %d", build_dir, jobs > 0 ? jobs : 4);
    EOS_INFO("CMake build: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult cmake_install(EosBackend *self, const char *build_dir,
                                  const char *install_dir) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cmake --install \"%s\" --prefix \"%s\"",
             build_dir, install_dir);
    EOS_INFO("CMake install: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult cmake_clean(EosBackend *self, const char *build_dir) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cmake --build \"%s\" --target clean", build_dir);
    EOS_INFO("CMake clean: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

void eos_backend_cmake_init(EosBackend *b) {
    memset(b, 0, sizeof(*b));
    strncpy(b->name, "cmake", EOS_MAX_NAME - 1);
    b->type = EOS_BUILD_CMAKE;
    b->configure = cmake_configure;
    b->build = cmake_build;
    b->install = cmake_install;
    b->clean = cmake_clean;
}
