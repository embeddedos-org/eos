// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/backend.h"
#include "eos/shell_cmd.h"
#include <stdio.h>
#include <string.h>

/* -D<key>=<value> for every option; the key unquoted as one word, the value
 * quoted. A value with spaces used to split into two arguments. */
static void append_defines(EosShellCmd *cmd, const EosPackage *pkg,
                           const char *skip_key) {
    int option_count = eos_pkg_option_count(pkg);
    for (int i = 0; i < option_count; i++) {
        const EosKeyValue *opt = eos_pkg_option_at(pkg, i);
        if (skip_key && strcmp(opt->key, skip_key) == 0) continue;
        eos_shell_cmd_text(cmd, " -D");
        eos_shell_cmd_word(cmd, opt->key);
        eos_shell_cmd_text(cmd, "=");
        eos_shell_cmd_arg(cmd, opt->value);
    }
}

static EosResult zephyr_configure(EosBackend *self, const char *src_dir,
                                  const char *build_dir, const char *toolchain_file,
                                  const EosPackage *pkg) {
    (void)self;
    /* Zephyr uses west + CMake under the hood */
    const char *board = eos_pkg_option_get(pkg, "board");

    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    if (board) {
        eos_shell_cmd_text(&cmd, "west build -b ");
        eos_shell_cmd_word(&cmd, board);
        eos_shell_cmd_text(&cmd, " -d ");
        eos_shell_cmd_arg(&cmd, build_dir);
        eos_shell_cmd_text(&cmd, " ");
        eos_shell_cmd_arg(&cmd, src_dir);
    } else {
        eos_shell_cmd_text(&cmd, "cmake -S ");
        eos_shell_cmd_arg(&cmd, src_dir);
        eos_shell_cmd_text(&cmd, " -B ");
        eos_shell_cmd_arg(&cmd, build_dir);
        eos_shell_cmd_text(&cmd, " -G Ninja -DBOARD=native_posix");
    }
    if (toolchain_file && toolchain_file[0]) {
        eos_shell_cmd_text(&cmd, " -DCMAKE_TOOLCHAIN_FILE=");
        eos_shell_cmd_arg(&cmd, toolchain_file);
    }
    append_defines(&cmd, pkg, "board");
    return eos_shell_cmd_run(&cmd, "Zephyr configure");
}

static EosResult zephyr_build(EosBackend *self, const char *build_dir, int jobs) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "west build -d ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " -- -j");
    eos_shell_cmd_int(&cmd, jobs > 0 ? jobs : 4);
    EosResult res = eos_shell_cmd_run(&cmd, "Zephyr build");
    if (res != EOS_ERR_BUILD) return res;   /* ran and succeeded, or was refused */

    /* west is not there or failed: the build directory is a CMake tree too. */
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "cmake --build ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " -j ");
    eos_shell_cmd_int(&cmd, jobs > 0 ? jobs : 4);
    return eos_shell_cmd_run(&cmd, "Zephyr build (cmake)");
}

static EosResult zephyr_install(EosBackend *self, const char *build_dir,
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
    eos_shell_cmd_text(&cmd, "\\zephyr\\zephyr.bin ");
    eos_shell_cmd_arg(&cmd, install_dir);
    eos_shell_cmd_text(&cmd, "\\firmware.bin");
#else
    eos_shell_cmd_text(&cmd, "mkdir -p ");
    eos_shell_cmd_arg(&cmd, install_dir);
    eos_shell_cmd_text(&cmd, " && if [ -f ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, "/zephyr/zephyr.bin ]; then cp -f ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, "/zephyr/zephyr.bin ");
    eos_shell_cmd_arg(&cmd, install_dir);
    eos_shell_cmd_text(&cmd, "/firmware.bin; elif [ -f ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, "/zephyr/zephyr.elf ]; then cp -f ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, "/zephyr/zephyr.elf ");
    eos_shell_cmd_arg(&cmd, install_dir);
    eos_shell_cmd_text(&cmd, "/firmware.elf; else echo \"zephyr_install: no zephyr.bin or zephyr.elf in\" ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, "/zephyr >&2; exit 1; fi");
#endif
    return eos_shell_cmd_run(&cmd, "Zephyr install");
}

static EosResult zephyr_clean(EosBackend *self, const char *build_dir) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "cmake --build ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " --target clean");
    return eos_shell_cmd_run(&cmd, "Zephyr clean");
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
