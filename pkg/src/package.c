// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/package.h"
#include "eos/log.h"
#include <string.h>
#include <stdio.h>

EosResult eos_package_set_from_config(EosPackageSet *set, const EosConfig *cfg) {
    memset(set, 0, sizeof(*set));

    for (int i = 0; i < cfg->package_count; i++) {
        if (set->count >= EOS_MAX_PACKAGES) {
            EOS_WARN("Package limit reached (%d)", EOS_MAX_PACKAGES);
            break;
        }

        EosPackage *pkg = &set->packages[set->count];
        const EosPackageConfig *pc = &cfg->packages[i];

        strncpy(pkg->name, pc->name, EOS_MAX_NAME - 1);
        strncpy(pkg->version, pc->version, EOS_MAX_NAME - 1);
        strncpy(pkg->source, pc->source, EOS_MAX_URL - 1);
        strncpy(pkg->hash, pc->hash, EOS_HASH_LEN - 1);
        pkg->build_type = pc->build_type;

        for (int d = 0; d < pc->dep_count && d < EOS_MAX_DEPS; d++) {
            strncpy(pkg->deps[d], pc->deps[d], EOS_MAX_NAME - 1);
            pkg->dep_count++;
        }

        for (int o = 0; o < pc->option_count && o < EOS_MAX_OPTIONS; o++) {
            pkg->options[o] = pc->options[o];
            pkg->option_count++;
        }

        snprintf(pkg->src_dir, EOS_MAX_PATH, "%s/src/%s-%s",
                 cfg->workspace.build_dir, pkg->name, pkg->version);
        snprintf(pkg->build_dir, EOS_MAX_PATH, "%s/build/%s",
                 cfg->workspace.build_dir, pkg->name);

        pkg->resolved = 0;
        set->count++;
    }

    EOS_INFO("Loaded %d packages from config", set->count);
    return EOS_OK;
}

static int find_package(const EosPackageSet *set, const char *name) {
    for (int i = 0; i < set->count; i++) {
        if (strcmp(set->packages[i].name, name) == 0) return i;
    }
    return -1;
}

EosResult eos_package_resolve(EosPackageSet *set) {
    for (int i = 0; i < set->count; i++) {
        EosPackage *pkg = &set->packages[i];
        for (int d = 0; d < pkg->dep_count; d++) {
            int dep_idx = find_package(set, pkg->deps[d]);
            if (dep_idx < 0) {
                EOS_ERROR("Unresolved dependency: %s requires %s", pkg->name, pkg->deps[d]);
                return EOS_ERR_NOT_FOUND;
            }
        }
        pkg->resolved = 1;
        EOS_DEBUG("Resolved: %s v%s (%d deps)", pkg->name, pkg->version, pkg->dep_count);
    }

    EOS_INFO("All %d packages resolved", set->count);
    return EOS_OK;
}

EosResult eos_package_build_graph(const EosPackageSet *set, EosGraph *graph) {
    eos_graph_init(graph);

    /* Add all packages as nodes */
    for (int i = 0; i < set->count; i++) {
        int node_id;
        EOS_CHECK(eos_graph_add_node(graph, set->packages[i].name,
                                          EOS_NODE_PACKAGE,
                                          set->packages[i].build_type, &node_id));
    }

    /* Add dependency edges */
    for (int i = 0; i < set->count; i++) {
        const EosPackage *pkg = &set->packages[i];
        int to_id = eos_graph_find_node(graph, pkg->name);

        for (int d = 0; d < pkg->dep_count; d++) {
            int from_id = eos_graph_find_node(graph, pkg->deps[d]);
            if (from_id >= 0 && to_id >= 0) {
                EOS_CHECK(eos_graph_add_edge(graph, from_id, to_id));
            }
        }
    }

    EOS_INFO("Build graph: %d nodes, %d edges", graph->node_count, graph->edge_count);
    return EOS_OK;
}

void eos_package_dump(const EosPackageSet *set) {
    printf("Packages (%d):\n", set->count);
    for (int i = 0; i < set->count; i++) {
        const EosPackage *pkg = &set->packages[i];
        printf("  %s v%s [%s]", pkg->name, pkg->version,
               eos_build_type_str(pkg->build_type));
        if (pkg->dep_count > 0) {
            printf(" deps: ");
            for (int d = 0; d < pkg->dep_count; d++) {
                printf("%s%s", pkg->deps[d], d < pkg->dep_count - 1 ? ", " : "");
            }
        }
        printf("\n");
    }
}

/* ---- Registry API ---- */

int eos_package_register(EosPackageSet *set, const EosPackage *pkg) {
    if (!set || !pkg) return -1;
    if (set->count >= EOS_MAX_PACKAGES) return -1;
    memcpy(&set->packages[set->count], pkg, sizeof(EosPackage));
    set->count++;
    return 0;
}

const EosPackage *eos_package_find(const EosPackageSet *set, const char *name) {
    if (!set || !name) return NULL;
    for (int i = 0; i < set->count; i++) {
        if (strcmp(set->packages[i].name, name) == 0)
            return &set->packages[i];
    }
    return NULL;
}

int eos_package_init_all(EosPackageSet *set) {
    if (!set) return -1;
    for (int i = 0; i < set->count; i++) {
        if (set->packages[i].init_fn) {
            int r = set->packages[i].init_fn();
            if (r != 0) return r;
        }
        set->packages[i].installed = true;
    }
    return 0;
}
