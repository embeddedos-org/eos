// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include <stdio.h>
#include <string.h>
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

static void test_graph_init(void) {
    printf("test_graph_init:\n");
    EosGraph g;
    eos_graph_init(&g);

    ASSERT(g.node_count == 0, "empty graph has no nodes");
    ASSERT(g.edge_count == 0, "empty graph has no edges");
    ASSERT(g.sorted_count == 0, "empty graph has no sorted order");
}

static void test_graph_add_nodes(void) {
    printf("test_graph_add_nodes:\n");
    EosGraph g;
    eos_graph_init(&g);

    int id1, id2, id3;
    EosResult r;

    r = eos_graph_add_node(&g, "zlib", EOS_NODE_PACKAGE, EOS_BUILD_CMAKE, &id1);
    ASSERT(r == EOS_OK, "add zlib succeeds");
    ASSERT(id1 == 0, "first node id is 0");

    r = eos_graph_add_node(&g, "busybox", EOS_NODE_PACKAGE, EOS_BUILD_KBUILD, &id2);
    ASSERT(r == EOS_OK, "add busybox succeeds");
    ASSERT(id2 == 1, "second node id is 1");

    r = eos_graph_add_node(&g, "kernel", EOS_NODE_KERNEL, EOS_BUILD_KBUILD, &id3);
    ASSERT(r == EOS_OK, "add kernel succeeds");

    ASSERT(g.node_count == 3, "graph has 3 nodes");
}

static void test_graph_find_node(void) {
    printf("test_graph_find_node:\n");
    EosGraph g;
    eos_graph_init(&g);

    eos_graph_add_node(&g, "alpha", EOS_NODE_PACKAGE, EOS_BUILD_CMAKE, NULL);
    eos_graph_add_node(&g, "beta", EOS_NODE_PACKAGE, EOS_BUILD_MAKE, NULL);

    ASSERT(eos_graph_find_node(&g, "alpha") == 0, "find alpha returns 0");
    ASSERT(eos_graph_find_node(&g, "beta") == 1, "find beta returns 1");
    ASSERT(eos_graph_find_node(&g, "gamma") == -1, "find nonexistent returns -1");
}

static void test_topological_sort_linear(void) {
    printf("test_topological_sort_linear:\n");
    EosGraph g;
    eos_graph_init(&g);

    /* A -> B -> C (linear chain) */
    int a, b, c;
    eos_graph_add_node(&g, "A", EOS_NODE_PACKAGE, EOS_BUILD_CMAKE, &a);
    eos_graph_add_node(&g, "B", EOS_NODE_PACKAGE, EOS_BUILD_CMAKE, &b);
    eos_graph_add_node(&g, "C", EOS_NODE_PACKAGE, EOS_BUILD_CMAKE, &c);

    eos_graph_add_edge(&g, a, b);
    eos_graph_add_edge(&g, b, c);

    EosResult r = eos_graph_topological_sort(&g);
    ASSERT(r == EOS_OK, "topological sort succeeds");
    ASSERT(g.sorted_count == 3, "all 3 nodes sorted");

    /* A must come before B, B before C */
    int pos_a = -1, pos_b = -1, pos_c = -1;
    for (int i = 0; i < g.sorted_count; i++) {
        if (g.sorted_order[i] == a) pos_a = i;
        if (g.sorted_order[i] == b) pos_b = i;
        if (g.sorted_order[i] == c) pos_c = i;
    }
    ASSERT(pos_a < pos_b, "A comes before B");
    ASSERT(pos_b < pos_c, "B comes before C");
}

static void test_topological_sort_diamond(void) {
    printf("test_topological_sort_diamond:\n");
    EosGraph g;
    eos_graph_init(&g);

    /*   A
        / \
       B   C
        \ /
         D   */
    int a, b, c, d;
    eos_graph_add_node(&g, "A", EOS_NODE_PACKAGE, EOS_BUILD_CMAKE, &a);
    eos_graph_add_node(&g, "B", EOS_NODE_PACKAGE, EOS_BUILD_CMAKE, &b);
    eos_graph_add_node(&g, "C", EOS_NODE_PACKAGE, EOS_BUILD_CMAKE, &c);
    eos_graph_add_node(&g, "D", EOS_NODE_PACKAGE, EOS_BUILD_CMAKE, &d);

    eos_graph_add_edge(&g, a, b);
    eos_graph_add_edge(&g, a, c);
    eos_graph_add_edge(&g, b, d);
    eos_graph_add_edge(&g, c, d);

    EosResult r = eos_graph_topological_sort(&g);
    ASSERT(r == EOS_OK, "diamond sort succeeds");

    int pos_a = -1, pos_d = -1;
    for (int i = 0; i < g.sorted_count; i++) {
        if (g.sorted_order[i] == a) pos_a = i;
        if (g.sorted_order[i] == d) pos_d = i;
    }
    ASSERT(pos_a < pos_d, "A comes before D in diamond");
}

static void test_cycle_detection(void) {
    printf("test_cycle_detection:\n");
    EosGraph g;
    eos_graph_init(&g);

    /* A -> B -> C -> A (cycle!) */
    int a, b, c;
    eos_graph_add_node(&g, "A", EOS_NODE_PACKAGE, EOS_BUILD_CMAKE, &a);
    eos_graph_add_node(&g, "B", EOS_NODE_PACKAGE, EOS_BUILD_CMAKE, &b);
    eos_graph_add_node(&g, "C", EOS_NODE_PACKAGE, EOS_BUILD_CMAKE, &c);

    eos_graph_add_edge(&g, a, b);
    eos_graph_add_edge(&g, b, c);
    eos_graph_add_edge(&g, c, a);

    EosResult r = eos_graph_topological_sort(&g);
    ASSERT(r == EOS_ERR_CYCLE, "cycle detected correctly");
}

static void test_independent_nodes(void) {
    printf("test_independent_nodes:\n");
    EosGraph g;
    eos_graph_init(&g);

    eos_graph_add_node(&g, "X", EOS_NODE_PACKAGE, EOS_BUILD_CMAKE, NULL);
    eos_graph_add_node(&g, "Y", EOS_NODE_PACKAGE, EOS_BUILD_CMAKE, NULL);
    eos_graph_add_node(&g, "Z", EOS_NODE_PACKAGE, EOS_BUILD_CMAKE, NULL);

    EosResult r = eos_graph_topological_sort(&g);
    ASSERT(r == EOS_OK, "independent nodes sort succeeds");
    ASSERT(g.sorted_count == 3, "all 3 independent nodes sorted");
}

int main(void) {
    eos_log_set_level(EOS_LOG_ERROR);

    printf("=== EoS Graph Tests ===\n\n");

    test_graph_init();
    test_graph_add_nodes();
    test_graph_find_node();
    test_topological_sort_linear();
    test_topological_sort_diamond();
    test_cycle_detection();
    test_independent_nodes();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
