// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EOS_BACKEND_H
#define EOS_BACKEND_H

#include "eos/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EosBackend EosBackend;

typedef int (*eos_backend_configure_fn)(EosBackend *be, const char *src_dir,
                                        const char *build_dir,
                                        const char *toolchain,
                                        const EosKeyValue *options,
                                        int option_count);
typedef int (*eos_backend_build_fn)(EosBackend *be, const char *build_dir, int jobs);
typedef int (*eos_backend_install_fn)(EosBackend *be, const char *build_dir,
                                      const char *install_dir);

struct EosBackend {
    const char              *name;
    EosBuildType             type;
    eos_backend_configure_fn configure;
    eos_backend_build_fn     build;
    eos_backend_install_fn   install;
    void                    *user_data;
};

int         eos_backend_register(EosBackend *be);
EosBackend *eos_backend_find_by_type(EosBuildType type);
EosBackend *eos_backend_find(const char *name);
int         eos_backend_init_all(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_BACKEND_H */
