// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EOS_CONFIG_H
#define EOS_CONFIG_H

#include "eos/types.h"
#include "eos/error.h"

typedef struct {
    char name[EOS_MAX_NAME];
    char version[EOS_MAX_NAME];
} EosProjectConfig;

typedef struct {
    char backend[EOS_MAX_NAME];
    char build_dir[EOS_MAX_PATH];
    char cache_dir[EOS_MAX_PATH];
} EosWorkspaceConfig;

typedef struct {
    char target[EOS_MAX_NAME];
    char rtos_target[EOS_MAX_NAME];
} EosToolchainConfig;

typedef struct {
    char name[EOS_MAX_NAME];
    char version[EOS_MAX_NAME];
    char source[EOS_MAX_URL];
    char hash[EOS_HASH_LEN];
    EosBuildType build_type;
    char deps[EOS_MAX_DEPS][EOS_MAX_NAME];
    int dep_count;
    EosKeyValue options[EOS_MAX_OPTIONS];
    int option_count;
} EosPackageConfig;

typedef struct {
    char provider[EOS_MAX_NAME];
    char defconfig[EOS_MAX_PATH];
} EosKernelConfig;

typedef struct {
    char provider[EOS_MAX_NAME];
    EosInitSystem init;
    char hostname[EOS_MAX_NAME];
} EosRootfsConfig;

typedef struct {
    EosRtosProvider provider;
    char board[EOS_MAX_NAME];
    char core[EOS_MAX_NAME];
    char entry[EOS_MAX_PATH];
    char output[EOS_MAX_PATH];
    EosFirmwareFormat format;
    EosKeyValue options[EOS_MAX_OPTIONS];
    int option_count;
} EosRtosConfig;

typedef struct {
    int enabled;
    int api_docs;
    int user_docs;
    char generator[EOS_MAX_NAME];
    char output_dir[EOS_MAX_PATH];
    char api_tool[EOS_MAX_NAME];
    char api_config[EOS_MAX_PATH];
    char site_tool[EOS_MAX_NAME];
    char site_config[EOS_MAX_PATH];
    char logo[EOS_MAX_PATH];
    char title[EOS_MAX_NAME];
} EosDocsConfig;

typedef struct {
    EosSystemKind kind;

    EosKernelConfig kernel;
    EosRootfsConfig rootfs;
    EosImageFormat image_format;
    char output[EOS_MAX_PATH];

    EosRtosConfig rtos[EOS_MAX_RTOS];
    int rtos_count;
} EosSystemConfig;

typedef struct {
    EosProjectConfig project;
    EosWorkspaceConfig workspace;
    EosToolchainConfig toolchain;
    EosSystemConfig system;
    EosDocsConfig docs;

    char layers[EOS_MAX_LAYERS][EOS_MAX_PATH];
    int layer_count;

    EosPackageConfig packages[EOS_MAX_PACKAGES];
    int package_count;
} EosConfig;

EosResult eos_config_load(EosConfig *cfg, const char *path);
void eos_config_init(EosConfig *cfg);
void eos_config_dump(const EosConfig *cfg);

#endif /* EOS_CONFIG_H */
