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
    /* #148 Step 2a: options no longer live inline. They are interned as
     * key\0value\0 pairs in the owning EosConfig's heap string table
     * (EosConfig.options_table below); this struct carries only a
     * borrowed pointer to that table, the byte offset of this package's
     * first option within it, and the count (capped at EOS_MAX_OPTIONS).
     * The borrowed pointer is refreshed by eos_config_load() after any
     * table growth; a zeroed package has a NULL table and no options. */
    const char *options_table;
    uint32_t options_off;
    uint16_t option_count;
} EosPackageConfig;

/* Options accessors (#148 Step 0a, storage moved in Step 2a): the only
 * sanctioned way to read a package's options. NULL-safe throughout.
 * eos_package_option_at() materialises the pair into a static view:
 * the returned pointer stays valid until the next option_at() call. */
int eos_package_option_count(const EosPackageConfig *pkg);
const EosKeyValue *eos_package_option_at(const EosPackageConfig *pkg, int i);
const char *eos_package_option_get(const EosPackageConfig *pkg, const char *key);

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
    /* #148: the former options[EOS_MAX_OPTIONS] array (20,480 B per
     * entry, 81,920 B across rtos[EOS_MAX_RTOS]) and its option_count
     * were dead storage — no writer, reader, or accessor anywhere in
     * the tree (parser sets provider/board/core/entry/output/format
     * only). Removed, not migrated to the string table: there was
     * nothing to migrate. */
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

    /* #148 Step 2a: heap string table owning every package's options
     * (interned key\0value\0 pairs; each package's options_off indexes
     * into it). NULL in a zeroed or eos_config_init()'d config means
     * "empty table". Owned by this struct: release it with
     * eos_config_dispose() before re-initialising a live config;
     * eos_config_load() disposes any previous table itself. */
    char *options_table;
    size_t options_table_len;
    size_t options_table_cap;
} EosConfig;

EosResult eos_config_load(EosConfig *cfg, const char *path);
void eos_config_init(EosConfig *cfg);
void eos_config_dispose(EosConfig *cfg);
void eos_config_dump(const EosConfig *cfg);

#endif /* EOS_CONFIG_H */
