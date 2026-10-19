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

static EosResult cmake_configure(EosBackend *self, const char *src_dir,
                                 const char *build_dir, const char *toolchain_file,
                                 const EosPackage *pkg) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "cmake -S ");
    eos_shell_cmd_arg(&cmd, src_dir);
    eos_shell_cmd_text(&cmd, " -B ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " -G Ninja");
    if (toolchain_file && toolchain_file[0]) {
        eos_shell_cmd_text(&cmd, " -DCMAKE_TOOLCHAIN_FILE=");
        eos_shell_cmd_arg(&cmd, toolchain_file);
    }
    append_defines(&cmd, pkg, NULL);
    return eos_shell_cmd_run(&cmd, "CMake configure");
}

static EosResult cmake_build(EosBackend *self, const char *build_dir, int jobs) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "cmake --build ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " -j ");
    eos_shell_cmd_int(&cmd, jobs > 0 ? jobs : 4);
    return eos_shell_cmd_run(&cmd, "CMake build");
}

static EosResult cmake_install(EosBackend *self, const char *build_dir,
                               const char *install_dir) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "cmake --install ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " --prefix ");
    eos_shell_cmd_arg(&cmd, install_dir);
    return eos_shell_cmd_run(&cmd, "CMake install");
}

static EosResult cmake_clean(EosBackend *self, const char *build_dir) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "cmake --build ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " --target clean");
    return eos_shell_cmd_run(&cmd, "CMake clean");
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
