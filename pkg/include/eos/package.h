// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EOS_PACKAGE_H
#define EOS_PACKAGE_H

#include "eos/types.h"
#include "eos/error.h"
#include "eos/config.h"
#include "eos/graph.h"

typedef struct {
    char name[EOS_MAX_NAME];
    char version[EOS_MAX_NAME];
    char source[EOS_MAX_URL];
    char hash[EOS_HASH_LEN];
    EosBuildType build_type;

    char deps[EOS_MAX_DEPS][EOS_MAX_NAME];
    int dep_count;

    /* #148 Step 2b: options no longer live inline. They are interned
     * as key\0value\0 pairs in a heap string table; this struct
     * carries only a borrowed pointer to that table, the byte offset
     * of this package's first option within it, and the count
     * (capped at EOS_MAX_OPTIONS). The table is owned by the
     * EosPackageSet when the package came from
     * eos_package_set_from_config(), or by the package itself when
     * built with eos_pkg_option_add() (options_owned). A zeroed
     * package has a NULL table and no options. */
    const char *options_table;
    uint32_t options_off;
    uint16_t option_count;
    bool options_owned;

    char src_dir[EOS_MAX_PATH];
    char build_dir[EOS_MAX_PATH];
    int resolved;
} EosPackage;

typedef struct {
    EosPackage packages[EOS_MAX_PACKAGES];
    int count;

    /* #148 Step 2b: heap string table owning the options of every
     * package in the set (interned key\0value\0 pairs; each
     * package's options_off indexes into it). NULL in a zeroed set
     * means "empty table". Owned by this struct: release it with
     * eos_package_set_dispose() before reusing a live set;
     * eos_package_set_from_config() disposes any previous table
     * itself. */
    char *options_table;
    size_t options_table_len;
    size_t options_table_cap;
} EosPackageSet;

EosResult eos_package_set_from_config(EosPackageSet *set, const EosConfig *cfg);
void eos_package_set_dispose(EosPackageSet *set);
EosResult eos_pkg_option_add(EosPackage *pkg, const char *key, const char *value);
void eos_pkg_options_dispose(EosPackage *pkg);
EosResult eos_package_build_graph(const EosPackageSet *set, EosGraph *graph);
EosResult eos_package_resolve(EosPackageSet *set);
void eos_package_dump(const EosPackageSet *set);

EosResult eos_fetch_source(const char *url, const char *dest_dir,
                               const char *expected_hash);

#endif /* EOS_PACKAGE_H */
