// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/backend.h"
#include "eos/shell_cmd.h"
#include <stdio.h>
#include <string.h>

static EosResult autotools_configure(EosBackend *self, const char *src_dir,
                                     const char *build_dir, const char *toolchain_file,
                                     const EosPackage *pkg) {
    (void)self; (void)build_dir;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "cd ");
    eos_shell_cmd_arg(&cmd, src_dir);
    eos_shell_cmd_text(&cmd, " && ./configure");
    if (toolchain_file && toolchain_file[0]) {
        eos_shell_cmd_text(&cmd, " --host=");
        eos_shell_cmd_arg(&cmd, toolchain_file);
    }
    /* --<key>=<value>: the key is an option name, one word; the value quoted. */
    int option_count = eos_pkg_option_count(pkg);
    for (int i = 0; i < option_count; i++) {
        const EosKeyValue *opt = eos_pkg_option_at(pkg, i);
        eos_shell_cmd_text(&cmd, " --");
        eos_shell_cmd_word(&cmd, opt->key);
        eos_shell_cmd_text(&cmd, "=");
        eos_shell_cmd_arg(&cmd, opt->value);
    }
    return eos_shell_cmd_run(&cmd, "Autotools configure");
}

static EosResult autotools_build(EosBackend *self, const char *build_dir, int jobs) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "make -C ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " -j");
    eos_shell_cmd_int(&cmd, jobs > 0 ? jobs : 4);
    return eos_shell_cmd_run(&cmd, "Autotools build");
}

static EosResult autotools_clean(EosBackend *self, const char *build_dir) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "make -C ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " distclean");
    return eos_shell_cmd_run(&cmd, "Autotools clean");
}

static EosResult autotools_install(EosBackend *self, const char *build_dir,
                                   const char *install_dir) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "make -C ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " install DESTDIR=");
    eos_shell_cmd_arg(&cmd, install_dir);
    return eos_shell_cmd_run(&cmd, "Autotools install");
}

void eos_backend_autotools_init(EosBackend *b) {
    memset(b, 0, sizeof(*b));
    strncpy(b->name, "autotools", EOS_MAX_NAME - 1);
    b->type = EOS_BUILD_AUTOTOOLS;
    b->configure = autotools_configure;
    b->build = autotools_build;
    b->install = autotools_install;
    b->clean = autotools_clean;
}
