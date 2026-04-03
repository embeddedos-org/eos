// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/toolchain.h"
#include "eos/log.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static char *tc_trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

static EosArch arch_from_target(const char *target) {
    if (strstr(target, "aarch64") || strstr(target, "arm64"))
        return EOS_ARCH_ARM64;
    if (strstr(target, "x86_64") || strstr(target, "x86-64"))
        return EOS_ARCH_X86_64;
    if (strstr(target, "riscv64"))
        return EOS_ARCH_RISCV64;
    if (strstr(target, "arm-none-eabi"))
        return EOS_ARCH_ARM_CORTEX_M;
    return EOS_ARCH_HOST;
}

EosResult eos_toolchain_load(EosToolchain *tc, const char *yaml_path) {
    FILE *fp = fopen(yaml_path, "r");
    if (!fp) {
        EOS_ERROR("Cannot open toolchain: %s", yaml_path);
        return EOS_ERR_IO;
    }

    memset(tc, 0, sizeof(*tc));
    char line[1024];

    while (fgets(line, sizeof(line), fp)) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        nl = strchr(line, '\r');
        if (nl) *nl = '\0';

        char *trimmed = tc_trim(line);
        if (trimmed[0] == '\0' || trimmed[0] == '#') continue;

        char *colon = strchr(trimmed, ':');
        if (!colon) continue;

        *colon = '\0';
        char *key = tc_trim(trimmed);
        char *val = tc_trim(colon + 1);

        if (strcmp(key, "name") == 0)           strncpy(tc->name, val, EOS_MAX_NAME - 1);
        else if (strcmp(key, "target") == 0)    strncpy(tc->target_triple, val, EOS_MAX_NAME - 1);
        else if (strcmp(key, "cc") == 0)        strncpy(tc->cc, val, EOS_MAX_PATH - 1);
        else if (strcmp(key, "cxx") == 0)       strncpy(tc->cxx, val, EOS_MAX_PATH - 1);
        else if (strcmp(key, "ar") == 0)        strncpy(tc->ar, val, EOS_MAX_PATH - 1);
        else if (strcmp(key, "ld") == 0)        strncpy(tc->ld, val, EOS_MAX_PATH - 1);
        else if (strcmp(key, "sysroot") == 0)   strncpy(tc->sysroot, val, EOS_MAX_PATH - 1);
        else if (strcmp(key, "cflags") == 0)    strncpy(tc->cflags, val, EOS_MAX_PATH - 1);
        else if (strcmp(key, "ldflags") == 0)   strncpy(tc->ldflags, val, EOS_MAX_PATH - 1);
    }

    fclose(fp);

    tc->arch = arch_from_target(tc->target_triple);

    EOS_INFO("Toolchain loaded: %s (%s, arch=%s)",
               tc->name, tc->target_triple, eos_arch_str(tc->arch));
    return EOS_OK;
}

EosResult eos_toolchain_detect_host(EosToolchain *tc) {
    memset(tc, 0, sizeof(*tc));
    strncpy(tc->name, "host", EOS_MAX_NAME - 1);
    tc->arch = EOS_ARCH_HOST;

#ifdef _WIN32
    strncpy(tc->target_triple, "x86_64-pc-windows-msvc", EOS_MAX_NAME - 1);
    strncpy(tc->cc, "cl.exe", EOS_MAX_PATH - 1);
    strncpy(tc->cxx, "cl.exe", EOS_MAX_PATH - 1);
    strncpy(tc->ar, "lib.exe", EOS_MAX_PATH - 1);
    strncpy(tc->ld, "link.exe", EOS_MAX_PATH - 1);
#elif defined(__APPLE__)
    strncpy(tc->target_triple, "x86_64-apple-darwin", EOS_MAX_NAME - 1);
    strncpy(tc->cc, "cc", EOS_MAX_PATH - 1);
    strncpy(tc->cxx, "c++", EOS_MAX_PATH - 1);
    strncpy(tc->ar, "ar", EOS_MAX_PATH - 1);
    strncpy(tc->ld, "ld", EOS_MAX_PATH - 1);
#else
    strncpy(tc->target_triple, "x86_64-linux-gnu", EOS_MAX_NAME - 1);
    strncpy(tc->cc, "gcc", EOS_MAX_PATH - 1);
    strncpy(tc->cxx, "g++", EOS_MAX_PATH - 1);
    strncpy(tc->ar, "ar", EOS_MAX_PATH - 1);
    strncpy(tc->ld, "ld", EOS_MAX_PATH - 1);
#endif

    EOS_INFO("Detected host toolchain: %s", tc->target_triple);
    return EOS_OK;
}

EosResult eos_toolchain_load_by_target(EosToolchain *tc, const char *target,
                                            const char *search_dir) {
    char path[EOS_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s.yaml", search_dir, target);

    FILE *fp = fopen(path, "r");
    if (fp) {
        fclose(fp);
        return eos_toolchain_load(tc, path);
    }

    EOS_WARN("No toolchain YAML for '%s', detecting host", target);
    return eos_toolchain_detect_host(tc);
}

void eos_toolchain_dump(const EosToolchain *tc) {
    printf("Toolchain: %s\n", tc->name);
    printf("  Target: %s (arch: %s)\n", tc->target_triple, eos_arch_str(tc->arch));
    printf("  CC:     %s\n", tc->cc);
    printf("  CXX:    %s\n", tc->cxx);
    printf("  AR:     %s\n", tc->ar);
    printf("  LD:     %s\n", tc->ld);
    if (tc->sysroot[0]) printf("  Sysroot: %s\n", tc->sysroot);
    if (tc->cflags[0])  printf("  CFLAGS:  %s\n", tc->cflags);
    if (tc->ldflags[0]) printf("  LDFLAGS: %s\n", tc->ldflags);
}
