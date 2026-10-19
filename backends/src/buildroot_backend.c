// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/backend.h"
#include "eos/shell_cmd.h"
#include <stdio.h>
#include <string.h>

static EosResult buildroot_configure(EosBackend *self, const char *src_dir,
                                     const char *build_dir, const char *toolchain_file,
                                     const EosPackage *pkg) {
    (void)self;
    const char *defconfig = eos_pkg_option_get(pkg, "defconfig");
    if (!defconfig) defconfig = "qemu_aarch64_virt_defconfig";

    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "make -C ");
    eos_shell_cmd_arg(&cmd, src_dir);
    eos_shell_cmd_text(&cmd, " O=");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " ");
    eos_shell_cmd_word(&cmd, defconfig);
    if (toolchain_file && toolchain_file[0]) {
        eos_shell_cmd_text(&cmd, " BR2_TOOLCHAIN_EXTERNAL_PATH=");
        eos_shell_cmd_arg(&cmd, toolchain_file);
    }
    return eos_shell_cmd_run(&cmd, "Buildroot configure");
}

static EosResult buildroot_build(EosBackend *self, const char *build_dir, int jobs) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "make -C ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " -j");
    eos_shell_cmd_int(&cmd, jobs > 0 ? jobs : 4);
    return eos_shell_cmd_run(&cmd, "Buildroot build");
}

static EosResult buildroot_clean(EosBackend *self, const char *build_dir) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "make -C ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " clean");
    return eos_shell_cmd_run(&cmd, "Buildroot clean");
}

static EosResult buildroot_install(EosBackend *self, const char *build_dir,
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
    eos_shell_cmd_arg(&cmd, build_dir);   /* the quoted path, then the glob */
    eos_shell_cmd_text(&cmd, "\\images\\* ");
    eos_shell_cmd_arg(&cmd, install_dir);
    eos_shell_cmd_text(&cmd, "\\");
#else
    eos_shell_cmd_text(&cmd, "mkdir -p ");
    eos_shell_cmd_arg(&cmd, install_dir);
    eos_shell_cmd_text(&cmd, " && cp -r ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, "/images/* ");
    eos_shell_cmd_arg(&cmd, install_dir);
    eos_shell_cmd_text(&cmd, "/");
#endif
    return eos_shell_cmd_run(&cmd, "Buildroot install");
}

void eos_backend_buildroot_init(EosBackend *b) {
    memset(b, 0, sizeof(*b));
    strncpy(b->name, "buildroot", EOS_MAX_NAME - 1);
    b->type = EOS_BUILD_KBUILD;
    b->configure = buildroot_configure;
    b->build = buildroot_build;
    b->install = buildroot_install;
    b->clean = buildroot_clean;
}
