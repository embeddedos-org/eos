// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/backend.h"
#include "eos/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EosResult zephyr_configure(EosBackend *self, const char *src_dir,
                                  const char *build_dir, const char *toolchain_file,
                                  const EosKeyValue *options, int option_count) {
    (void)self;
    char cmd[2048];
    int offset = 0;

    /* Zephyr uses west + CMake under the hood */
    const char *board = NULL;
    for (int i = 0; i < option_count; i++) {
        if (strcmp(options[i].key, "board") == 0) {
            board = options[i].value;
            break;
        }
    }

    if (board) {
        offset = snprintf(cmd, sizeof(cmd),
                          "west build -b %s -d \"%s\" \"%s\"",
                          board, build_dir, src_dir);
    } else {
        offset = snprintf(cmd, sizeof(cmd),
                          "cmake -S \"%s\" -B \"%s\" -G Ninja -DBOARD=native_posix",
                          src_dir, build_dir);
    }

    if (toolchain_file && toolchain_file[0]) {
        offset += snprintf(cmd + offset, sizeof(cmd) - (size_t)offset,
                          " -DCMAKE_TOOLCHAIN_FILE=\"%s\"", toolchain_file);
    }

    for (int i = 0; i < option_count && offset < (int)sizeof(cmd) - 64; i++) {
        if (strcmp(options[i].key, "board") == 0) continue;
        offset += snprintf(cmd + offset, sizeof(cmd) - (size_t)offset,
                          " -D%s=%s", options[i].key, options[i].value);
    }

    EOS_INFO("Zephyr configure: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult zephyr_build(EosBackend *self, const char *build_dir, int jobs) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "west build -d \"%s\" -- -j%d",
             build_dir, jobs > 0 ? jobs : 4);
    EOS_INFO("Zephyr build: %s", cmd);
    int rc = system(cmd);

    if (rc != 0) {
        snprintf(cmd, sizeof(cmd), "cmake --build \"%s\" -j %d",
                 build_dir, jobs > 0 ? jobs : 4);
        EOS_INFO("Zephyr fallback build: %s", cmd);
        rc = system(cmd);
    }

    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult zephyr_install(EosBackend *self, const char *build_dir,
                                const char *install_dir) {
    (void)self;
    char cmd[1024];
    /* Copy firmware outputs to install dir */
    snprintf(cmd, sizeof(cmd),
#ifdef _WIN32
             "if not exist \"%s\" mkdir \"%s\" && copy /Y \"%s\\zephyr\\zephyr.bin\" \"%s\\firmware.bin\"",
             install_dir, install_dir, build_dir, install_dir
#else
             "mkdir -p \"%s\" && cp -f \"%s/zephyr/zephyr.bin\" \"%s/firmware.bin\" 2>/dev/null || "
             "cp -f \"%s/zephyr/zephyr.elf\" \"%s/firmware.elf\" 2>/dev/null || true",
             install_dir, build_dir, install_dir, build_dir, install_dir
#endif
    );
    EOS_INFO("Zephyr install: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult zephyr_clean(EosBackend *self, const char *build_dir) {
    (void)self;
    char cmd[1024];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "if exist \"%s\" rmdir /s /q \"%s\"", build_dir, build_dir);
#else
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", build_dir);
#endif
    EOS_INFO("Zephyr clean: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

void eos_backend_zephyr_init(EosBackend *b) {
    memset(b, 0, sizeof(*b));
    strncpy(b->name, "zephyr", EOS_MAX_NAME - 1);
    b->type = EOS_BUILD_ZEPHYR;
    b->configure = zephyr_configure;
    b->build = zephyr_build;
    b->install = zephyr_install;
    b->clean = zephyr_clean;
}
