// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/backend.h"
#include "eos/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EosResult freertos_configure(EosBackend *self, const char *src_dir,
                                    const char *build_dir, const char *toolchain_file,
                                    const EosKeyValue *options, int option_count) {
    (void)self;
    char cmd[2048];
    int offset = snprintf(cmd, sizeof(cmd),
                          "cmake -S \"%s\" -B \"%s\" -G Ninja",
                          src_dir, build_dir);

    if (toolchain_file && toolchain_file[0]) {
        offset += snprintf(cmd + offset, sizeof(cmd) - (size_t)offset,
                          " -DCMAKE_TOOLCHAIN_FILE=\"%s\"", toolchain_file);
    }

    /* Pass FREERTOS_KERNEL_PATH if available */
    for (int i = 0; i < option_count && offset < (int)sizeof(cmd) - 64; i++) {
        offset += snprintf(cmd + offset, sizeof(cmd) - (size_t)offset,
                          " -D%s=%s", options[i].key, options[i].value);
    }

    EOS_INFO("FreeRTOS configure: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult freertos_build(EosBackend *self, const char *build_dir, int jobs) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cmake --build \"%s\" -j %d", build_dir, jobs > 0 ? jobs : 4);
    EOS_INFO("FreeRTOS build: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult freertos_install(EosBackend *self, const char *build_dir,
                                  const char *install_dir) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
#ifdef _WIN32
             "if not exist \"%s\" mkdir \"%s\" && copy /Y \"%s\\*.bin\" \"%s\\\" 2>nul & "
             "copy /Y \"%s\\*.elf\" \"%s\\\" 2>nul & "
             "copy /Y \"%s\\*.hex\" \"%s\\\" 2>nul",
             install_dir, install_dir,
             build_dir, install_dir,
             build_dir, install_dir,
             build_dir, install_dir
#else
             "mkdir -p \"%s\" && "
             "find \"%s\" -maxdepth 2 \\( -name '*.bin' -o -name '*.elf' -o -name '*.hex' \\) "
             "-exec cp {} \"%s/\" \\; 2>/dev/null || true",
             install_dir, build_dir, install_dir
#endif
    );
    EOS_INFO("FreeRTOS install: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult freertos_clean(EosBackend *self, const char *build_dir) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cmake --build \"%s\" --target clean", build_dir);
    EOS_INFO("FreeRTOS clean: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

void eos_backend_freertos_init(EosBackend *b) {
    memset(b, 0, sizeof(*b));
    strncpy(b->name, "freertos", EOS_MAX_NAME - 1);
    b->type = EOS_BUILD_FREERTOS;
    b->configure = freertos_configure;
    b->build = freertos_build;
    b->install = freertos_install;
    b->clean = freertos_clean;
}
