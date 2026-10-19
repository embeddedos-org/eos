// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/backend.h"
#include "eos/shell_cmd.h"
#include "eos/log.h"
#include <stdio.h>
#include <string.h>

static EosResult make_configure(EosBackend *self, const char *src_dir,
                                const char *build_dir, const char *toolchain_file,
                                const EosPackage *pkg) {
    (void)self; (void)build_dir; (void)pkg;

    /* Check for configure script (autotools-style) */
    char configure_path[EOS_MAX_PATH];
    snprintf(configure_path, sizeof(configure_path), "%s/configure", src_dir);

    FILE *fp = fopen(configure_path, "r");
    if (!fp) {
        EOS_DEBUG("Make: no configure script found, skipping configure");
        return EOS_OK;
    }
    fclose(fp);

    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "cd ");
    eos_shell_cmd_arg(&cmd, src_dir);
    eos_shell_cmd_text(&cmd, " && ./configure");
    if (toolchain_file && toolchain_file[0]) {
        eos_shell_cmd_text(&cmd, " --host=");
        eos_shell_cmd_arg(&cmd, toolchain_file);
    }
    return eos_shell_cmd_run(&cmd, "Make configure");
}

static EosResult make_build(EosBackend *self, const char *build_dir, int jobs) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "make -C ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " -j");
    eos_shell_cmd_int(&cmd, jobs > 0 ? jobs : 4);
    return eos_shell_cmd_run(&cmd, "Make build");
}

static EosResult make_clean(EosBackend *self, const char *build_dir) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "make -C ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " clean");
    return eos_shell_cmd_run(&cmd, "Make clean");
}

static EosResult make_install(EosBackend *self, const char *build_dir,
                              const char *install_dir) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "make -C ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " install DESTDIR=");
    eos_shell_cmd_arg(&cmd, install_dir);
    return eos_shell_cmd_run(&cmd, "Make install");
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
