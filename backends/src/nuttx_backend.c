// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/backend.h"
#include "eos/shell_cmd.h"
#include <stdio.h>
#include <string.h>

static EosResult nuttx_configure(EosBackend *self, const char *src_dir,
                                 const char *build_dir, const char *toolchain_file,
                                 const EosPackage *pkg) {
    (void)self; (void)build_dir;
    /* NuttX uses its own tools/configure.sh <board>:<config> flow */
    const char *board_config = eos_pkg_option_get(pkg, "board_config");
    if (!board_config) board_config = "sim:nsh";

    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "cd ");
    eos_shell_cmd_arg(&cmd, src_dir);
    eos_shell_cmd_text(&cmd, " && ");
    if (toolchain_file && toolchain_file[0]) {
        eos_shell_cmd_text(&cmd, "CROSS_COMPILE=");
        eos_shell_cmd_word(&cmd, toolchain_file);
        eos_shell_cmd_text(&cmd, "- ");
    }
    eos_shell_cmd_text(&cmd, "./tools/configure.sh ");
    eos_shell_cmd_word(&cmd, board_config);
    return eos_shell_cmd_run(&cmd, "NuttX configure");
}

static EosResult nuttx_build(EosBackend *self, const char *build_dir, int jobs) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "make -C ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " -j");
    eos_shell_cmd_int(&cmd, jobs > 0 ? jobs : 4);
    return eos_shell_cmd_run(&cmd, "NuttX build");
}

static EosResult nuttx_clean(EosBackend *self, const char *build_dir) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "make -C ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " distclean");
    return eos_shell_cmd_run(&cmd, "NuttX clean");
}

static EosResult nuttx_install(EosBackend *self, const char *build_dir,
                               const char *install_dir) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
#ifdef _WIN32
    eos_shell_cmd_text(&cmd, "if not exist ");
    eos_shell_cmd_arg(&cmd, install_dir);
    eos_shell_cmd_text(&cmd, " mkdir ");
    eos_shell_cmd_arg(&cmd, install_dir);
    eos_shell_cmd_text(&cmd, " && copy /Y ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, "\\nuttx.bin ");
    eos_shell_cmd_arg(&cmd, install_dir);
    eos_shell_cmd_text(&cmd, "\\firmware.bin 2>nul");
#else
    eos_shell_cmd_text(&cmd, "mkdir -p ");
    eos_shell_cmd_arg(&cmd, install_dir);
    eos_shell_cmd_text(&cmd, " && if [ -f ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, "/nuttx.bin ]; then cp -f ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, "/nuttx.bin ");
    eos_shell_cmd_arg(&cmd, install_dir);
    eos_shell_cmd_text(&cmd, "/firmware.bin; elif [ -f ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, "/nuttx ]; then cp -f ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, "/nuttx ");
    eos_shell_cmd_arg(&cmd, install_dir);
    eos_shell_cmd_text(&cmd, "/firmware.elf; else echo \"nuttx_install: no nuttx.bin or nuttx image in\" ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " >&2; exit 1; fi");
#endif
    return eos_shell_cmd_run(&cmd, "NuttX install");
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
