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

static EosResult meson_configure(EosBackend *self, const char *src_dir,
                                 const char *build_dir, const char *toolchain_file,
                                 const EosPackage *pkg) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "meson setup ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " ");
    eos_shell_cmd_arg(&cmd, src_dir);
    if (toolchain_file && toolchain_file[0]) {
        eos_shell_cmd_text(&cmd, " --cross-file ");
        eos_shell_cmd_arg(&cmd, toolchain_file);
    }
    append_defines(&cmd, pkg, NULL);
    return eos_shell_cmd_run(&cmd, "Meson configure");
}

static EosResult meson_build(EosBackend *self, const char *build_dir, int jobs) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "ninja -C ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " -j ");
    eos_shell_cmd_int(&cmd, jobs > 0 ? jobs : 4);
    return eos_shell_cmd_run(&cmd, "Meson build");
}

static EosResult meson_install(EosBackend *self, const char *build_dir,
                               const char *install_dir) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "DESTDIR=");
    eos_shell_cmd_arg(&cmd, install_dir);
    eos_shell_cmd_text(&cmd, " ninja -C ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " install");
    return eos_shell_cmd_run(&cmd, "Meson install");
}

static EosResult meson_clean(EosBackend *self, const char *build_dir) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "ninja -C ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " -t clean");
    return eos_shell_cmd_run(&cmd, "Meson clean");
}

void eos_backend_meson_init(EosBackend *b) {
    memset(b, 0, sizeof(*b));
    strncpy(b->name, "meson", EOS_MAX_NAME - 1);
    b->type = EOS_BUILD_MAKE;
    b->configure = meson_configure;
    b->build = meson_build;
    b->install = meson_install;
    b->clean = meson_clean;
}
