#include <stdio.h>
#include <string.h>
#include "eos/config.h"
#include "eos/package.h"
#include "eos/graph.h"
#include "eos/log.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(expr, msg)                                         \
    do {                                                          \
        tests_run++;                                              \
        if (!(expr)) {                                            \
            printf("  FAIL: %s (line %d)\n", msg, __LINE__);     \
        } else {                                                  \
            tests_passed++;                                       \
            printf("  PASS: %s\n", msg);                         \
        }                                                         \
    } while (0)

static void test_package_from_config(void) {
    printf("test_package_from_config:\n");
    static EosConfig cfg;
    static EosPackageSet set;

    /* Create a test config */
    FILE *fp = fopen("test_pkg.yaml", "w");
    if (!fp) { printf("  SKIP: cannot create test file\n"); return; }

    fprintf(fp, "project:\n");
    fprintf(fp, "  name: pkg-test\n");
    fprintf(fp, "  version: 1.0.0\n");
    fprintf(fp, "\n");
    fprintf(fp, "packages:\n");
    fprintf(fp, "  - name: libfoo\n");
    fprintf(fp, "    version: 2.0.0\n");
    fprintf(fp, "    build:\n");
    fprintf(fp, "      type: cmake\n");
    fprintf(fp, "\n");
    fprintf(fp, "  - name: libbar\n");
    fprintf(fp, "    version: 3.1.0\n");
    fprintf(fp, "    build:\n");
    fprintf(fp, "      type: make\n");
    fclose(fp);

    eos_config_load(&cfg, "test_pkg.yaml");

    EosResult res = eos_package_set_from_config(&set, &cfg);

    ASSERT(res == EOS_OK, "package set created from config");
    ASSERT(set.count == 2, "two packages loaded");
    ASSERT(strcmp(set.packages[0].name, "libfoo") == 0, "first package name");
    ASSERT(strcmp(set.packages[0].version, "2.0.0") == 0, "first package version");
    ASSERT(set.packages[0].build_type == EOS_BUILD_CMAKE, "first package build type");
    ASSERT(strcmp(set.packages[1].name, "libbar") == 0, "second package name");
    ASSERT(set.packages[1].build_type == EOS_BUILD_MAKE, "second package build type");

    remove("test_pkg.yaml");
}

static void test_package_resolve(void) {
    printf("test_package_resolve:\n");

    static EosPackageSet set;
    memset(&set, 0, sizeof(set));

    /* Add packages with deps */
    strncpy(set.packages[0].name, "base", EOS_MAX_NAME - 1);
    set.packages[0].build_type = EOS_BUILD_CMAKE;
    set.packages[0].dep_count = 0;

    strncpy(set.packages[1].name, "app", EOS_MAX_NAME - 1);
    set.packages[1].build_type = EOS_BUILD_CMAKE;
    strncpy(set.packages[1].deps[0], "base", EOS_MAX_NAME - 1);
    set.packages[1].dep_count = 1;

    set.count = 2;

    EosResult res = eos_package_resolve(&set);
    ASSERT(res == EOS_OK, "packages resolve successfully");
    ASSERT(set.packages[0].resolved == 1, "base package resolved");
    ASSERT(set.packages[1].resolved == 1, "app package resolved");
}

static void test_package_unresolved_dep(void) {
    printf("test_package_unresolved_dep:\n");

    static EosPackageSet set;
    memset(&set, 0, sizeof(set));

    strncpy(set.packages[0].name, "broken", EOS_MAX_NAME - 1);
    strncpy(set.packages[0].deps[0], "nonexistent", EOS_MAX_NAME - 1);
    set.packages[0].dep_count = 1;
    set.count = 1;

    EosResult res = eos_package_resolve(&set);
    ASSERT(res == EOS_ERR_NOT_FOUND, "unresolved dep detected");
}

static void test_package_build_graph(void) {
    printf("test_package_build_graph:\n");

    static EosPackageSet set;
    memset(&set, 0, sizeof(set));

    strncpy(set.packages[0].name, "zlib", EOS_MAX_NAME - 1);
    set.packages[0].build_type = EOS_BUILD_CMAKE;

    strncpy(set.packages[1].name, "busybox", EOS_MAX_NAME - 1);
    set.packages[1].build_type = EOS_BUILD_KBUILD;
    strncpy(set.packages[1].deps[0], "zlib", EOS_MAX_NAME - 1);
    set.packages[1].dep_count = 1;

    set.count = 2;

    EosGraph graph;
    EosResult res = eos_package_build_graph(&set, &graph);
    ASSERT(res == EOS_OK, "build graph created");
    ASSERT(graph.node_count == 2, "graph has 2 nodes");
    ASSERT(graph.edge_count == 1, "graph has 1 edge (zlib -> busybox)");

    /* Verify topological order */
    res = eos_graph_topological_sort(&graph);
    ASSERT(res == EOS_OK, "topological sort succeeds");

    int pos_zlib = -1, pos_busybox = -1;
    for (int i = 0; i < graph.sorted_count; i++) {
        if (strcmp(graph.nodes[graph.sorted_order[i]].name, "zlib") == 0) pos_zlib = i;
        if (strcmp(graph.nodes[graph.sorted_order[i]].name, "busybox") == 0) pos_busybox = i;
    }
    ASSERT(pos_zlib < pos_busybox, "zlib builds before busybox");
}

int main(void) {
    eos_log_set_level(EOS_LOG_ERROR);

    printf("=== EoS Package Tests ===\n\n");

    test_package_from_config();
    test_package_resolve();
    test_package_unresolved_dep();
    test_package_build_graph();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
