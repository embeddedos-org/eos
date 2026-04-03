// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/graph.h"
#include "eos/log.h"
#include <string.h>
#include <stdio.h>

void eos_graph_init(EosGraph *g) {
    memset(g, 0, sizeof(*g));
}

EosResult eos_graph_add_node(EosGraph *g, const char *name, EosNodeType type,
                                 EosBuildType build_type, int *out_id) {
    if (g->node_count >= EOS_MAX_NODES) {
        EOS_ERROR("Graph node overflow (max %d)", EOS_MAX_NODES);
        return EOS_ERR_OVERFLOW;
    }

    int id = g->node_count;
    EosNode *n = &g->nodes[id];
    strncpy(n->name, name, EOS_MAX_NAME - 1);
    n->type = type;
    n->build_type = build_type;
    n->status = EOS_NODE_PENDING;
    n->id = id;
    n->user_data = NULL;
    g->node_count++;

    if (out_id) *out_id = id;
    EOS_DEBUG("Graph: added node [%d] %s", id, name);
    return EOS_OK;
}

EosResult eos_graph_add_edge(EosGraph *g, int from, int to) {
    if (from < 0 || from >= g->node_count || to < 0 || to >= g->node_count) {
        return EOS_ERR_INVALID;
    }
    if (g->edge_count >= EOS_MAX_EDGES) {
        EOS_ERROR("Graph edge overflow (max %d)", EOS_MAX_EDGES);
        return EOS_ERR_OVERFLOW;
    }

    g->edges[g->edge_count].from = from;
    g->edges[g->edge_count].to = to;
    g->edge_count++;

    EOS_DEBUG("Graph: edge %s -> %s", g->nodes[from].name, g->nodes[to].name);
    return EOS_OK;
}

int eos_graph_find_node(const EosGraph *g, const char *name) {
    for (int i = 0; i < g->node_count; i++) {
        if (strcmp(g->nodes[i].name, name) == 0) return i;
    }
    return -1;
}

/* Kahn's algorithm for topological sort with cycle detection */
EosResult eos_graph_topological_sort(EosGraph *g) {
    int in_degree[EOS_MAX_NODES] = {0};

    for (int i = 0; i < g->edge_count; i++) {
        in_degree[g->edges[i].to]++;
    }

    int queue[EOS_MAX_NODES];
    int front = 0, back = 0;

    for (int i = 0; i < g->node_count; i++) {
        if (in_degree[i] == 0) {
            queue[back++] = i;
        }
    }

    g->sorted_count = 0;

    while (front < back) {
        int node = queue[front++];
        g->sorted_order[g->sorted_count++] = node;

        for (int i = 0; i < g->edge_count; i++) {
            if (g->edges[i].from == node) {
                int neighbor = g->edges[i].to;
                in_degree[neighbor]--;
                if (in_degree[neighbor] == 0) {
                    queue[back++] = neighbor;
                }
            }
        }
    }

    if (g->sorted_count != g->node_count) {
        EOS_ERROR("Dependency cycle detected in build graph");
        return EOS_ERR_CYCLE;
    }

    EOS_DEBUG("Topological sort: %d nodes ordered", g->sorted_count);
    return EOS_OK;
}

void eos_graph_dump(const EosGraph *g) {
    printf("Build Graph: %d nodes, %d edges\n", g->node_count, g->edge_count);
    for (int i = 0; i < g->node_count; i++) {
        printf("  [%d] %s (%s)\n", i, g->nodes[i].name,
               eos_build_type_str(g->nodes[i].build_type));
    }
    if (g->sorted_count > 0) {
        printf("Build order: ");
        for (int i = 0; i < g->sorted_count; i++) {
            printf("%s", g->nodes[g->sorted_order[i]].name);
            if (i < g->sorted_count - 1) printf(" -> ");
        }
        printf("\n");
    }
}
