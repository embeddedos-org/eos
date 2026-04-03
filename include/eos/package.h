// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EOS_PACKAGE_H
#define EOS_PACKAGE_H

#include "eos/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char   *name;
    const char   *version;
    EosBuildType  build_type;
    const char   *src_dir;
    const char   *build_dir;
    EosKeyValue   options[EOS_MAX_OPTIONS];
    int           option_count;
    char          hash[EOS_HASH_LEN];
    bool          installed;
    int         (*init_fn)(void);
} EosPackage;

typedef struct EosPackageSet {
    EosPackage packages[EOS_MAX_PACKAGES];
    int count;
} EosPackageSet;

int              eos_package_register(EosPackageSet *set, const EosPackage *pkg);
const EosPackage *eos_package_find(const EosPackageSet *set, const char *name);
int              eos_package_init_all(EosPackageSet *set);

#ifdef __cplusplus
}
#endif

#endif /* EOS_PACKAGE_H */
