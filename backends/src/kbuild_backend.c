// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/backend.h"
#include "eos/shell_cmd.h"
#include <stdio.h>
#include <string.h>

static EosResult kbuild_configure(EosBackend *self, const char *src_dir,
                                  const char *build_dir, const char *toolchain_file,
                                  const EosPackage *pkg) {
    (void)self;
    /* Find defconfig from options */
    const char *defconfig = eos_pkg_option_get(pkg, "defconfig");
    if (!defconfig) defconfig = "defconfig";

    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "make -C ");
    eos_shell_cmd_arg(&cmd, src_dir);
    eos_shell_cmd_text(&cmd, " O=");
    eos_shell_cmd_arg(&cmd, build_dir);
    if (toolchain_file && toolchain_file[0]) {
        /* make reads ARCH=, CROSS_COMPILE= and the target as single words. */
        eos_shell_cmd_text(&cmd, " ARCH=arm64 CROSS_COMPILE=");
        eos_shell_cmd_word(&cmd, toolchain_file);
        eos_shell_cmd_text(&cmd, "-");
    }
    eos_shell_cmd_text(&cmd, " ");
    eos_shell_cmd_word(&cmd, defconfig);
    return eos_shell_cmd_run(&cmd, "Kbuild configure");
}

static EosResult kbuild_build(EosBackend *self, const char *build_dir, int jobs) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "make -C ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " -j");
    eos_shell_cmd_int(&cmd, jobs > 0 ? jobs : 4);
    return eos_shell_cmd_run(&cmd, "Kbuild build");
}

static EosResult kbuild_clean(EosBackend *self, const char *build_dir) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "make -C ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " mrproper");
    return eos_shell_cmd_run(&cmd, "Kbuild clean");
}

static EosResult kbuild_install(EosBackend *self, const char *build_dir,
                                const char *install_dir) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "make -C ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " install INSTALL_PATH=");
    eos_shell_cmd_arg(&cmd, install_dir);
    return eos_shell_cmd_run(&cmd, "Kbuild install");
}

void eos_backend_kbuild_init(EosBackend *b) {
    memset(b, 0, sizeof(*b));
    strncpy(b->name, "kbuild", EOS_MAX_NAME - 1);
    b->type = EOS_BUILD_KBUILD;
    b->configure = kbuild_configure;
    b->build = kbuild_build;
    b->install = kbuild_install;
    b->clean = kbuild_clean;
}
