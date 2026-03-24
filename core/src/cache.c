// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/cache.h"
#include "eos/log.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(d) _mkdir(d)
#define PATH_SEP '\\'
#else
#include <sys/types.h>
#define MKDIR(d) mkdir(d, 0755)
#define PATH_SEP '/'
#endif

void eos_cache_init(EosCache *cache, const char *dir) {
    memset(cache, 0, sizeof(*cache));
    strncpy(cache->cache_dir, dir, EOS_MAX_PATH - 1);
}

/* Simple DJB2-based hash for cache keys (not cryptographic — for build cache only) */
void eos_cache_compute_hash(const char *input, size_t len, char *out, size_t out_size) {
    unsigned long h1 = 5381;
    unsigned long h2 = 0x9e3779b9;

    for (size_t i = 0; i < len; i++) {
        h1 = ((h1 << 5) + h1) ^ (unsigned char)input[i];
        h2 = ((h2 << 7) + h2) ^ (unsigned char)input[i];
    }

    snprintf(out, out_size, "%08lx%08lx%08lx%08lx%08lx%08lx%08lx%08lx",
             h1 & 0xFFFFFFFF, h2 & 0xFFFFFFFF,
             (h1 >> 4) & 0xFFFFFFFF, (h2 >> 4) & 0xFFFFFFFF,
             (h1 ^ h2) & 0xFFFFFFFF, (h2 ^ h1 >> 8) & 0xFFFFFFFF,
             (h1 + h2) & 0xFFFFFFFF, (h1 * h2) & 0xFFFFFFFF);
}

static void cache_path(const EosCache *cache, const char *key, char *path, size_t sz) {
    snprintf(path, sz, "%s%c%s.hash", cache->cache_dir, PATH_SEP, key);
}

EosResult eos_cache_check(const EosCache *cache, const char *key, int *hit) {
    char path[EOS_MAX_PATH];
    cache_path(cache, key, path, sizeof(path));

    FILE *fp = fopen(path, "r");
    if (!fp) {
        *hit = 0;
        return EOS_OK;
    }

    char stored[EOS_HASH_LEN] = {0};
    if (fgets(stored, sizeof(stored), fp)) {
        *hit = 1;
        EOS_DEBUG("Cache hit: %s", key);
    } else {
        *hit = 0;
    }

    fclose(fp);
    return EOS_OK;
}

EosResult eos_cache_store(EosCache *cache, const char *key, const char *hash) {
    MKDIR(cache->cache_dir);

    char path[EOS_MAX_PATH];
    cache_path(cache, key, path, sizeof(path));

    FILE *fp = fopen(path, "w");
    if (!fp) {
        EOS_WARN("Cannot write cache: %s", path);
        return EOS_ERR_IO;
    }

    fprintf(fp, "%s", hash);
    fclose(fp);
    EOS_DEBUG("Cache stored: %s -> %s", key, hash);
    return EOS_OK;
}

EosResult eos_cache_invalidate(EosCache *cache, const char *key) {
    char path[EOS_MAX_PATH];
    cache_path(cache, key, path, sizeof(path));
    remove(path);
    EOS_DEBUG("Cache invalidated: %s", key);
    return EOS_OK;
}
