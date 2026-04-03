// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/backend.h"
#include "eos/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EosResult nuttx_configure(EosBackend *self, const char *src_dir,
                                 const char *build_dir, const char *toolchain_file,
                                 const EosKeyValue *options, int option_count) {
    (void)self;
    (void)build_dir;
    char cmd[2048];

    /* NuttX uses its own tools/configure.sh <board>:<config> flow */
    const char *board_config = "sim:nsh";
    for (int i = 0; i < option_count; i++) {
        if (strcmp(options[i].key, "board_config") == 0) {
            board_config = options[i].value;
            break;
        }
    }

    if (toolchain_file && toolchain_file[0]) {
        snprintf(cmd, sizeof(cmd),
                 "cd \"%s\" && CROSS_COMPILE=%s- ./tools/configure.sh %s",
                 src_dir, toolchain_file, board_config);
    } else {
        snprintf(cmd, sizeof(cmd),
                 "cd \"%s\" && ./tools/configure.sh %s",
                 src_dir, board_config);
    }

    EOS_INFO("NuttX configure: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult nuttx_build(EosBackend *self, const char *build_dir, int jobs) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "make -C \"%s\" -j%d", build_dir, jobs > 0 ? jobs : 4);
    EOS_INFO("NuttX build: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult nuttx_install(EosBackend *self, const char *build_dir,
                               const char *install_dir) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
#ifdef _WIN32
             "if not exist \"%s\" mkdir \"%s\" && copy /Y \"%s\\nuttx.bin\" \"%s\\firmware.bin\" 2>nul",
             install_dir, install_dir, build_dir, install_dir
#else
             "mkdir -p \"%s\" && cp -f \"%s/nuttx.bin\" \"%s/firmware.bin\" 2>/dev/null || "
             "cp -f \"%s/nuttx\" \"%s/firmware.elf\" 2>/dev/null || true",
             install_dir, build_dir, install_dir, build_dir, install_dir
#endif
    );
    EOS_INFO("NuttX install: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult nuttx_clean(EosBackend *self, const char *build_dir) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "make -C \"%s\" distclean", build_dir);
    EOS_INFO("NuttX clean: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

void eos_backend_nuttx_init(EosBackend *b) {
    memset(b, 0, sizeof(*b));
    strncpy(b->name, "nuttx", EOS_MAX_NAME - 1);
    b->type = EOS_BUILD_NUTTX;
    b->configure = nuttx_configure;
    b->build = nuttx_build;
    b->install = nuttx_install;
    b->clean = nuttx_clean;
}
