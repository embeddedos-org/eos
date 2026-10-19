// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/backend.h"
#include "eos/shell_cmd.h"
#include "eos/log.h"
#include <stdio.h>
#include <string.h>

static EosResult cargo_configure(EosBackend *self, const char *src_dir,
                                 const char *build_dir, const char *toolchain_file,
                                 const EosPackage *pkg) {
    (void)self; (void)build_dir; (void)pkg;

    if (toolchain_file && toolchain_file[0]) {
        EOS_DEBUG("Cargo: toolchain file %s is not used; set a target in Cargo config",
                  toolchain_file);
    }

    /* Cargo doesn't have a separate configure step; verify Cargo.toml exists */
    char toml_path[EOS_MAX_PATH];
    snprintf(toml_path, sizeof(toml_path), "%s/Cargo.toml", src_dir);
    FILE *fp = fopen(toml_path, "r");
    if (!fp) {
        EOS_ERROR("Cargo.toml not found at %s", toml_path);
        return EOS_ERR_NOT_FOUND;
    }
    fclose(fp);
    return EOS_OK;
}

static EosResult cargo_build(EosBackend *self, const char *build_dir, int jobs) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "cargo build --manifest-path ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, "/Cargo.toml --release -j ");
    eos_shell_cmd_int(&cmd, jobs > 0 ? jobs : 4);
    return eos_shell_cmd_run(&cmd, "Cargo build");
}

static EosResult cargo_install(EosBackend *self, const char *build_dir,
                               const char *install_dir) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
#ifdef _WIN32
    eos_shell_cmd_text(&cmd, "if not exist ");
    eos_shell_cmd_arg(&cmd, install_dir);
    eos_shell_cmd_text(&cmd, "\\bin mkdir ");
    eos_shell_cmd_arg(&cmd, install_dir);
    eos_shell_cmd_text(&cmd, "\\bin && copy /Y ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, "\\target\\release\\*.exe ");
    eos_shell_cmd_arg(&cmd, install_dir);
    eos_shell_cmd_text(&cmd, "\\bin\\ 2>nul");
#else
    eos_shell_cmd_text(&cmd, "mkdir -p ");
    eos_shell_cmd_arg(&cmd, install_dir);
    eos_shell_cmd_text(&cmd, "/bin && find ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, "/target/release -maxdepth 1 -type f \\( -perm -0100 -o -perm -0010 -o -perm -0001 \\) -exec cp {} ");
    eos_shell_cmd_arg(&cmd, install_dir);
    eos_shell_cmd_text(&cmd, "/bin/ \\;");
#endif
    return eos_shell_cmd_run(&cmd, "Cargo install");
}

static EosResult cargo_clean(EosBackend *self, const char *build_dir) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "cargo clean --manifest-path ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, "/Cargo.toml");
    return eos_shell_cmd_run(&cmd, "Cargo clean");
}

void eos_backend_cargo_init(EosBackend *b) {
    memset(b, 0, sizeof(*b));
    strncpy(b->name, "cargo", EOS_MAX_NAME - 1);
    b->type = EOS_BUILD_CUSTOM;
    b->configure = cargo_configure;
    b->build = cargo_build;
    b->install = cargo_install;
    b->clean = cargo_clean;
}
