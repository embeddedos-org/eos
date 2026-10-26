// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/package.h"
#include "eos/log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ---- #148 Step 2b: the EosPackageSet string table ----
 * Package options are interned here as key\0value\0 pairs instead of
 * living in a 20,480-byte inline array per package. The table is a
 * heap arena owned by EosPackageSet, lazily allocated on first
 * intern; a NULL table means "empty". Interning returns the byte
 * offset of the pair. Allocation failure is reported to the caller
 * and fails the whole conversion \u2014 never a silent truncate.
 * Growing the table may move it, so packages hold offsets, and
 * eos_package_set_from_config() refreshes their borrowed pointers
 * at the end. */

static int set_table_intern(EosPackageSet *set, const char *key,
                            const char *val, uint32_t *off_out) {
    size_t klen = strlen(key) + 1;
    size_t vlen = strlen(val) + 1;
    if (set->options_table_len + klen + vlen > set->options_table_cap) {
        size_t new_cap = set->options_table_cap ? set->options_table_cap : 4096;
        while (new_cap < set->options_table_len + klen + vlen) new_cap *= 2;
        char *grown = realloc(set->options_table, new_cap);
        if (!grown) return -1;
        set->options_table = grown;
        set->options_table_cap = new_cap;
    }
    *off_out = (uint32_t)set->options_table_len;
    memcpy(set->options_table + set->options_table_len, key, klen);
    memcpy(set->options_table + set->options_table_len + klen, val, vlen);
    set->options_table_len += klen + vlen;
    return 0;
}

void eos_package_set_dispose(EosPackageSet *set) {
    if (!set) return;
    free(set->options_table);
    set->options_table = NULL;
    set->options_table_len = 0;
    set->options_table_cap = 0;
    for (int i = 0; i < set->count && i < EOS_MAX_PACKAGES; i++) {
        EosPackage *pkg = &set->packages[i];
        if (!pkg->options_owned) {
            pkg->options_table = NULL;
            pkg->options_off = 0;
            pkg->option_count = 0;
        }
    }
}

EosResult eos_package_set_from_config(EosPackageSet *set, const EosConfig *cfg) {
    /* A live set may already own a string table; release it before
     * the memset below would orphan it (#148 Step 2b lifetime
     * contract, mirroring eos_config_load()). */
    if (set->options_table) eos_package_set_dispose(set);
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

        /* #148 Step 2b: the copy still reads through the Step 0a
         * accessors, but the pairs are now interned into the set's
         * own string table instead of an inline array. Exhaustion
         * (allocation failure) fails the whole conversion \u2014 never
         * a silently truncated option list. */
        int opt_count = eos_package_option_count(pc);
        for (int o = 0; o < opt_count && o < EOS_MAX_OPTIONS; o++) {
            const EosKeyValue *kv = eos_package_option_at(pc, o);
            uint32_t off = 0;
            if (!kv || set_table_intern(set, kv->key, kv->value, &off) != 0) {
                EOS_ERROR("Out of memory interning options for package '%s'", pkg->name);
                eos_package_set_dispose(set);
                memset(set, 0, sizeof(*set));
                return EOS_ERR_NOMEM;
            }
            if (o == 0) pkg->options_off = off;
            pkg->option_count++;
        }

        /* Build the two derived paths through local copies of the name and
         * version. Passing pkg->name straight in made snprintf's sources alias
         * its own destination object — both live inside *set — which violates
         * the restrict qualifier on argument 1 and is undefined behaviour:
         *   warning: 'snprintf' arguments 5, 6 overlap destination object 'set'
         *
         * The return value is also checked now. EOS_MAX_PATH is 512 and
         * EOS_MAX_NAME is 128, so build_dir + "/src/" + name + "-" + version
         * reaches 647 bytes and snprintf would silently truncate:
         *   warning: 'snprintf' output between 8 and 646 bytes into a
         *            destination of size 512
         * Two packages whose names differ only past the cut would then share a
         * source or build directory and overwrite each other. Failing loudly is
         * the only safe outcome. */
        char name_copy[EOS_MAX_NAME];
        char version_copy[EOS_MAX_NAME];
        snprintf(name_copy, sizeof(name_copy), "%s", pkg->name);
        snprintf(version_copy, sizeof(version_copy), "%s", pkg->version);

        int n = snprintf(pkg->src_dir, EOS_MAX_PATH, "%s/src/%s-%s",
                         cfg->workspace.build_dir, name_copy, version_copy);
        if (n < 0 || n >= EOS_MAX_PATH) {
            EOS_ERROR("Source path for package '%s' exceeds %d bytes",
                      name_copy, EOS_MAX_PATH);
            return EOS_ERR_OVERFLOW;
        }

        n = snprintf(pkg->build_dir, EOS_MAX_PATH, "%s/build/%s",
                     cfg->workspace.build_dir, name_copy);
        if (n < 0 || n >= EOS_MAX_PATH) {
            EOS_ERROR("Build path for package '%s' exceeds %d bytes",
                      name_copy, EOS_MAX_PATH);
            return EOS_ERR_OVERFLOW;
        }

        pkg->resolved = 0;
        set->count++;
    }

    /* Interning may have moved the table; refresh every package's
     * borrowed pointer now that it is final (#148 Step 2b). */
    for (int i = 0; i < set->count; i++) {
        if (set->packages[i].option_count > 0)
            set->packages[i].options_table = set->options_table;
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

/* ---- #148 Step 0b accessors, re-implemented in Step 2b over the
 * string table (the twin of the EosPackageConfig accessors in
 * core/src/config.c). Call sites did not change. */

static const char *pkg_option_entry(const EosPackage *pkg, int i) {
    if (!pkg || !pkg->options_table || i < 0 || i >= pkg->option_count)
        return NULL;
    const char *p = pkg->options_table + pkg->options_off;
    for (int j = 0; j < i; j++) {
        p += strlen(p) + 1; /* key */
        p += strlen(p) + 1; /* value */
    }
    return p;
}

int eos_pkg_option_count(const EosPackage *pkg) {
    if (!pkg) return 0;
    return pkg->option_count;
}

const EosKeyValue *eos_pkg_option_at(const EosPackage *pkg, int i) {
    static EosKeyValue view;
    const char *entry = pkg_option_entry(pkg, i);
    if (!entry) return NULL;
    memset(&view, 0, sizeof(view));
    strncpy(view.key, entry, EOS_MAX_NAME - 1);
    strncpy(view.value, entry + strlen(entry) + 1, EOS_MAX_PATH - 1);
    return &view;
}

const char *eos_pkg_option_get(const EosPackage *pkg, const char *key) {
    if (!pkg || !key) return NULL;
    for (int i = 0; i < pkg->option_count; i++) {
        const char *entry = pkg_option_entry(pkg, i);
        if (entry && strcmp(entry, key) == 0)
            return entry + strlen(entry) + 1;
    }
    return NULL;
}

/* ---- #148 Step 2b: the standalone-package options writer ----
 * A package that is not a set member (firmware's configure package,
 * test fixtures) owns its table: eos_pkg_option_add() interns into a
 * lazily allocated heap table owned by the package itself, flagged
 * options_owned so dispose and the set code can tell borrowed from
 * owned. Appends only \u2014 order is preserved, duplicates are kept,
 * exactly as the inline array behaved. */

EosResult eos_pkg_option_add(EosPackage *pkg, const char *key, const char *value) {
    if (!pkg || !key || !value) return EOS_ERR_INVALID;
    if (pkg->options_table && !pkg->options_owned) return EOS_ERR_INVALID;
    if (pkg->option_count >= EOS_MAX_OPTIONS) return EOS_ERR_OVERFLOW;

    size_t klen = strlen(key) + 1;
    size_t vlen = strlen(value) + 1;
    if (klen > EOS_MAX_NAME || vlen > EOS_MAX_PATH)
        return EOS_ERR_OVERFLOW;

    /* Current table length: an owned table holds exactly this
     * package's pairs, starting at offset 0. */
    size_t len = 0;
    const char *p = pkg->options_table;
    for (int j = 0; j < pkg->option_count; j++) {
        len += strlen(p + len) + 1;
        len += strlen(p + len) + 1;
    }

    size_t new_cap = len ? len * 2 : 1024;
    while (new_cap < len + klen + vlen) new_cap *= 2;
    char *grown = realloc(pkg->options_table, new_cap);
    if (!grown) return EOS_ERR_NOMEM;
    pkg->options_table = grown;
    pkg->options_owned = true;
    memcpy(grown + len, key, klen);
    memcpy(grown + len + klen, value, vlen);
    pkg->option_count++;
    return EOS_OK;
}

void eos_pkg_options_dispose(EosPackage *pkg) {
    if (!pkg) return;
    if (pkg->options_owned) free((void *)pkg->options_table);
    pkg->options_table = NULL;
    pkg->options_off = 0;
    pkg->option_count = 0;
    pkg->options_owned = false;
}
