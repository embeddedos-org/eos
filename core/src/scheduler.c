// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/scheduler.h"
#include "eos/backend.h"
#include "eos/package.h"
#include "eos/log.h"
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(d) _mkdir(d)
#else
#include <sys/types.h>
#define MKDIR(d) mkdir(d, 0755)
#endif

void eos_scheduler_init(EosScheduler *sched, EosGraph *graph) {
    memset(sched, 0, sizeof(*sched));
    sched->graph = graph;
    sched->build_dir = ".eos/build";
    sched->parallel_jobs = 4;
}

static const EosPackage *find_package_by_name(const EosPackageSet *pkgs,
                                              const char *name) {
    if (!pkgs) return NULL;
    for (int i = 0; i < pkgs->count; i++) {
        if (strcmp(pkgs->packages[i].name, name) == 0)
            return &pkgs->packages[i];
    }
    return NULL;
}

static void compute_cache_key(const EosPackage *pkg, const char *toolchain,
                              char *key, size_t key_sz) {
    char input[2048];
    int len = snprintf(input, sizeof(input), "%s:%s:%s:%s",
                       pkg->name, pkg->version,
                       eos_build_type_str(pkg->build_type),
                       toolchain ? toolchain : "host");
    for (int i = 0; i < pkg->option_count && len < (int)sizeof(input) - 128; i++) {
        len += snprintf(input + len, sizeof(input) - (size_t)len,
                        ":%s=%s", pkg->options[i].key, pkg->options[i].value);
    }
    if (pkg->hash[0]) {
        snprintf(input + len, sizeof(input) - (size_t)len, ":%s", pkg->hash);
    }

    char hash[EOS_HASH_LEN];
    eos_cache_compute_hash(input, (size_t)strlen(input), hash, sizeof(hash));
    snprintf(key, key_sz, "%s-%s", pkg->name, hash);
}

EosResult eos_scheduler_execute(EosScheduler *sched) {
    EosGraph *g = sched->graph;

    EOS_CHECK(eos_graph_topological_sort(g));

    /* Initialize cache */
    char cache_dir[EOS_MAX_PATH];
    snprintf(cache_dir, sizeof(cache_dir), "%s/../cache",
             sched->build_dir);
    eos_cache_init(&sched->cache, cache_dir);

    EOS_INFO("=== Build Execution ===");
    EOS_INFO("  Targets:   %d", g->sorted_count);
    EOS_INFO("  Build dir: %s", sched->build_dir);
    EOS_INFO("  Jobs:      %d", sched->parallel_jobs);
    if (sched->toolchain_target && sched->toolchain_target[0])
        EOS_INFO("  Toolchain: %s", sched->toolchain_target);

    sched->built_count = 0;
    sched->cached_count = 0;
    sched->failed_count = 0;

    for (int i = 0; i < g->sorted_count; i++) {
        int idx = g->sorted_order[i];
        EosNode *node = &g->nodes[idx];

        EOS_INFO("[%d/%d] %s (%s)", i + 1, g->sorted_count,
                 node->name, eos_build_type_str(node->build_type));

        /* Dry-run mode */
        if (sched->dry_run) {
            const EosPackage *pkg = find_package_by_name(sched->packages, node->name);
            EOS_INFO("  (dry-run) backend=%s src=%s build=%s",
                     eos_build_type_str(node->build_type),
                     pkg ? pkg->src_dir : "(unknown)",
                     pkg ? pkg->build_dir : "(unknown)");
            node->status = EOS_NODE_DONE;
            sched->built_count++;
            continue;
        }

        /* Find package metadata */
        const EosPackage *pkg = find_package_by_name(sched->packages, node->name);

        /* Check cache */
        if (pkg) {
            char cache_key[EOS_MAX_PATH];
            compute_cache_key(pkg, sched->toolchain_target, cache_key, sizeof(cache_key));

            int hit = 0;
            eos_cache_check(&sched->cache, cache_key, &hit);
            if (hit) {
                EOS_INFO("  Cache hit: %s — skipping build", cache_key);
                node->status = EOS_NODE_DONE;
                sched->cached_count++;
                continue;
            }
        }

        /* Look up backend */
        EosBackend *be = eos_backend_find_by_type(node->build_type);
        if (!be) {
            EOS_ERROR("  No backend registered for %s",
                      eos_build_type_str(node->build_type));
            node->status = EOS_NODE_FAILED;
            sched->failed_count++;
            return EOS_ERR_BUILD;
        }

        node->status = EOS_NODE_BUILDING;

        /* Resolve directories */
        char src_dir[EOS_MAX_PATH];
        char build_dir[EOS_MAX_PATH];

        if (pkg) {
            strncpy(src_dir, pkg->src_dir, EOS_MAX_PATH - 1);
            strncpy(build_dir, pkg->build_dir, EOS_MAX_PATH - 1);
        } else {
            snprintf(src_dir, sizeof(src_dir), "%s/src/%s",
                     sched->build_dir, node->name);
            snprintf(build_dir, sizeof(build_dir), "%s/build/%s",
                     sched->build_dir, node->name);
        }

        MKDIR(build_dir);

        /* Configure */
        EOS_INFO("  Configure: %s -> %s", src_dir, build_dir);
        EosResult res = be->configure(be, src_dir, build_dir,
                                      sched->toolchain_target,
                                      pkg ? pkg->options : NULL,
                                      pkg ? pkg->option_count : 0);
        if (res != EOS_OK) {
            EOS_ERROR("  Configure failed: %s (%s)", node->name, eos_error_str(res));
            node->status = EOS_NODE_FAILED;
            sched->failed_count++;
            return res;
        }

        /* Build */
        EOS_INFO("  Build: %s (jobs=%d)", build_dir, sched->parallel_jobs);
        res = be->build(be, build_dir, sched->parallel_jobs);
        if (res != EOS_OK) {
            EOS_ERROR("  Build failed: %s (%s)", node->name, eos_error_str(res));
            node->status = EOS_NODE_FAILED;
            sched->failed_count++;
            return res;
        }

        /* Install to staging */
        char install_dir[EOS_MAX_PATH];
        snprintf(install_dir, sizeof(install_dir), "%s/staging", sched->build_dir);
        MKDIR(install_dir);

        EOS_INFO("  Install: %s -> %s", build_dir, install_dir);
        res = be->install(be, build_dir, install_dir);
        if (res != EOS_OK) {
            EOS_WARN("  Install returned non-zero (non-fatal): %s", node->name);
        }

        node->status = EOS_NODE_DONE;
        sched->built_count++;

        /* Store in cache */
        if (pkg) {
            char cache_key[EOS_MAX_PATH];
            compute_cache_key(pkg, sched->toolchain_target, cache_key, sizeof(cache_key));

            char hash[EOS_HASH_LEN];
            char hash_input[512];
            snprintf(hash_input, sizeof(hash_input), "%s:built", cache_key);
            eos_cache_compute_hash(hash_input, strlen(hash_input), hash, sizeof(hash));
            eos_cache_store(&sched->cache, cache_key, hash);
            EOS_DEBUG("  Cached: %s", cache_key);
        }

        EOS_INFO("  Done: %s", node->name);
    }

    EOS_INFO("=== Build Summary ===");
    EOS_INFO("  Built:  %d", sched->built_count);
    EOS_INFO("  Cached: %d", sched->cached_count);
    EOS_INFO("  Failed: %d", sched->failed_count);
    EOS_INFO("  Total:  %d", g->sorted_count);

    return (sched->failed_count == 0) ? EOS_OK : EOS_ERR_BUILD;
}
