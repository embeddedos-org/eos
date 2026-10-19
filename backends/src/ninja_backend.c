// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/backend.h"
#include "eos/shell_cmd.h"
#include <stdio.h>
#include <string.h>

static EosResult ninja_configure(EosBackend *self, const char *src_dir,
                                 const char *build_dir, const char *toolchain_file,
                                 const EosPackage *pkg) {
    (void)self; (void)src_dir; (void)build_dir; (void)toolchain_file;
    (void)pkg;
    /* Ninja consumes a build.ninja that something else generated. */
    return EOS_OK;
}

static EosResult ninja_build(EosBackend *self, const char *build_dir, int jobs) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "ninja -C ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " -j ");
    eos_shell_cmd_int(&cmd, jobs > 0 ? jobs : 4);
    return eos_shell_cmd_run(&cmd, "Ninja build");
}

static EosResult ninja_install(EosBackend *self, const char *build_dir,
                               const char *install_dir) {
    (void)self; (void)install_dir;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "ninja -C ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " install");
    return eos_shell_cmd_run(&cmd, "Ninja install");
}

static EosResult ninja_clean(EosBackend *self, const char *build_dir) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "ninja -C ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " -t clean");
    return eos_shell_cmd_run(&cmd, "Ninja clean");
}

void eos_backend_ninja_init(EosBackend *b) {
    memset(b, 0, sizeof(*b));
    strncpy(b->name, "ninja", EOS_MAX_NAME - 1);
    b->type = EOS_BUILD_NINJA;
    b->configure = ninja_configure;
    b->build = ninja_build;
    b->install = ninja_install;
    b->clean = ninja_clean;
}
