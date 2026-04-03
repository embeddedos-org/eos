// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EOS_CACHE_H
#define EOS_CACHE_H

#include "eos/types.h"
#include "eos/error.h"

typedef struct {
    char cache_dir[EOS_MAX_PATH];
} EosCache;

void eos_cache_init(EosCache *cache, const char *dir);
EosResult eos_cache_check(const EosCache *cache, const char *key, int *hit);
EosResult eos_cache_store(EosCache *cache, const char *key, const char *hash);
EosResult eos_cache_invalidate(EosCache *cache, const char *key);
void eos_cache_compute_hash(const char *input, size_t len, char *out, size_t out_size);

#endif /* EOS_CACHE_H */
