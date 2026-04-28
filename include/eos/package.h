// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EOS_PACKAGE_H
#define EOS_PACKAGE_H

#include "eos/types.h"
#include "eos/error.h"
#include "eos/config.h"
#include "eos/graph.h"

#ifdef __cplusplus
extern "C" {
#endif

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

    char src_dir[EOS_MAX_PATH];
    char build_dir[EOS_MAX_PATH];
    int resolved;

    bool installed;
    int (*init_fn)(void);
} EosPackage;

typedef struct EosPackageSet {
    EosPackage packages[EOS_MAX_PACKAGES];
    int count;
} EosPackageSet;

/* Build-system API */
EosResult eos_package_set_from_config(EosPackageSet *set, const EosConfig *cfg);
EosResult eos_package_build_graph(const EosPackageSet *set, EosGraph *graph);
EosResult eos_package_resolve(EosPackageSet *set);
void eos_package_dump(const EosPackageSet *set);

/* Registry API */
int              eos_package_register(EosPackageSet *set, const EosPackage *pkg);
const EosPackage *eos_package_find(const EosPackageSet *set, const char *name);
int              eos_package_init_all(EosPackageSet *set);

/* Source fetching */
EosResult eos_fetch_source(const char *url, const char *dest_dir,
                               const char *expected_hash);

#ifdef __cplusplus
}
#endif

#endif /* EOS_PACKAGE_H */
