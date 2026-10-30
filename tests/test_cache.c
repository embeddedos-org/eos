// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

#include "eos/cache.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define REMOVE_DIR(path) _rmdir(path)
#else
#include <unistd.h>
#define REMOVE_DIR(path) rmdir(path)
#endif

int main(void) {
    EosCache cache;
    const char *cache_dir = "test_cache_tmp";
    const char *key = "package-key";
    const char *bad_marker = "corrupt";
    char input[128];
    char marker[EOS_HASH_LEN];
    char marker_path[EOS_MAX_PATH];
    int hit = 1;

    eos_cache_init(&cache, cache_dir);
    assert(eos_cache_store(&cache, key, bad_marker) == EOS_OK);
    assert(eos_cache_check(&cache, key, &hit) == EOS_OK);
    assert(!hit);

    snprintf(input, sizeof(input), "%s:built", key);
    eos_cache_compute_hash(input, strlen(input), marker, sizeof(marker));
    assert(eos_cache_store(&cache, key, marker) == EOS_OK);
    assert(eos_cache_check(&cache, key, &hit) == EOS_OK);
    assert(hit);

    snprintf(marker_path, sizeof(marker_path), "%s/%s.hash", cache_dir, key);
    FILE *fp = fopen(marker_path, "a");
    assert(fp != NULL);
    assert(fputs("trailing", fp) >= 0);
    assert(fclose(fp) == 0);
    assert(eos_cache_check(&cache, key, &hit) == EOS_OK);
    assert(!hit);

    assert(eos_cache_invalidate(&cache, key) == EOS_OK);
    remove(marker_path);
    REMOVE_DIR(cache_dir);
    return 0;
}