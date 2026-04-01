// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EOS_TOOLCHAIN_H
#define EOS_TOOLCHAIN_H

#include "eos/types.h"
#include "eos/error.h"

typedef struct {
    char name[EOS_MAX_NAME];
    char target_triple[EOS_MAX_NAME];
    EosArch arch;
    char cc[EOS_MAX_PATH];
    char cxx[EOS_MAX_PATH];
    char ar[EOS_MAX_PATH];
    char ld[EOS_MAX_PATH];
    char sysroot[EOS_MAX_PATH];
    char cflags[EOS_MAX_PATH];
    char ldflags[EOS_MAX_PATH];
} EosToolchain;

EosResult eos_toolchain_load(EosToolchain *tc, const char *yaml_path);
EosResult eos_toolchain_detect_host(EosToolchain *tc);
EosResult eos_toolchain_load_by_target(EosToolchain *tc, const char *target,
                                            const char *search_dir);
void eos_toolchain_dump(const EosToolchain *tc);

#endif /* EOS_TOOLCHAIN_H */
