// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EOS_GRAPH_H
#define EOS_GRAPH_H

#include "eos/types.h"
#include "eos/error.h"

typedef enum {
    EOS_NODE_PACKAGE,
    EOS_NODE_KERNEL,
    EOS_NODE_ROOTFS,
    EOS_NODE_IMAGE,
    EOS_NODE_FIRMWARE,
    EOS_NODE_DOCS
} EosNodeType;

typedef enum {
    EOS_NODE_PENDING,
    EOS_NODE_BUILDING,
    EOS_NODE_DONE,
    EOS_NODE_FAILED
} EosNodeStatus;

typedef struct {
    char name[EOS_MAX_NAME];
    EosNodeType type;
    EosNodeStatus status;
    EosBuildType build_type;
    int id;
    void *user_data;
} EosNode;

typedef struct {
    int from;
    int to;
} EosEdge;

typedef struct {
    EosNode nodes[EOS_MAX_NODES];
    int node_count;
    EosEdge edges[EOS_MAX_EDGES];
    int edge_count;
    int sorted_order[EOS_MAX_NODES];
    int sorted_count;
} EosGraph;

void eos_graph_init(EosGraph *g);
EosResult eos_graph_add_node(EosGraph *g, const char *name, EosNodeType type,
                                 EosBuildType build_type, int *out_id);
EosResult eos_graph_add_edge(EosGraph *g, int from, int to);
EosResult eos_graph_topological_sort(EosGraph *g);
int eos_graph_find_node(const EosGraph *g, const char *name);
void eos_graph_dump(const EosGraph *g);

#endif /* EOS_GRAPH_H */
