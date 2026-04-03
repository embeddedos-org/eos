// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/backend.h"
#include "eos/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EosResult cargo_configure(EosBackend *self, const char *src_dir,
                                 const char *build_dir, const char *toolchain_file,
                                 const EosKeyValue *options, int option_count) {
    (void)self;
    (void)build_dir;
    (void)options;
    (void)option_count;

    if (toolchain_file && toolchain_file[0]) {
        EOS_INFO("Cargo: target=%s (set via --target)", toolchain_file);
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
    EOS_DEBUG("Cargo: found Cargo.toml at %s", toml_path);
    return EOS_OK;
}

static EosResult cargo_build(EosBackend *self, const char *build_dir, int jobs) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cargo build --manifest-path \"%s/Cargo.toml\" "
             "--release -j %d", build_dir, jobs > 0 ? jobs : 4);
    EOS_INFO("Cargo build: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult cargo_install(EosBackend *self, const char *build_dir,
                               const char *install_dir) {
    (void)self;
    char cmd[1024];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd),
             "if not exist \"%s\\bin\" mkdir \"%s\\bin\" && "
             "copy /Y \"%s\\target\\release\\*.exe\" \"%s\\bin\\\" 2>nul",
             install_dir, install_dir, build_dir, install_dir);
#else
    snprintf(cmd, sizeof(cmd),
             "mkdir -p \"%s/bin\" && "
             "find \"%s/target/release\" -maxdepth 1 -type f -executable "
             "-exec cp {} \"%s/bin/\" \\; 2>/dev/null || true",
             install_dir, build_dir, install_dir);
#endif
    EOS_INFO("Cargo install: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
}

static EosResult cargo_clean(EosBackend *self, const char *build_dir) {
    (void)self;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cargo clean --manifest-path \"%s/Cargo.toml\"",
             build_dir);
    EOS_INFO("Cargo clean: %s", cmd);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_BUILD;
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
