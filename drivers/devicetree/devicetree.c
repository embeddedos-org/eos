// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "devicetree.h"
#include <string.h>
#include <stdio.h>

static uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

static uint32_t align4(uint32_t v) { return (v + 3) & ~3u; }

int eos_dt_parse(EosDeviceTree *dt, const uint8_t *dtb, uint32_t size) {
    if (!dt || !dtb || size < 40) return -1;
    memset(dt, 0, sizeof(*dt));

    uint32_t magic = be32(dtb);
    if (magic != EOS_DT_MAGIC) return -1;

    uint32_t totalsize  = be32(dtb + 4);
    uint32_t off_struct = be32(dtb + 8);
    uint32_t off_string = be32(dtb + 12);
    dt->version   = be32(dtb + 20);
    dt->boot_cpuid = be32(dtb + 28);
    (void)totalsize;

    const uint8_t *structs = dtb + off_struct;
    const uint8_t *strings = dtb + off_string;
    uint32_t pos = 0;

    EosDtNode *stack[32];
    int depth = 0;

    while (pos < (size - off_struct)) {
        uint32_t token = be32(structs + pos);
        pos += 4;

        switch (token) {
        case EOS_DT_BEGIN_NODE: {
            if (dt->node_count >= EOS_DT_MAX_NODES) return -1;
            EosDtNode *node = &dt->nodes[dt->node_count++];
            memset(node, 0, sizeof(*node));

            const char *name = (const char *)(structs + pos);
            strncpy(node->name, name, EOS_DT_NAME_MAX - 1);
            pos += align4((uint32_t)strlen(name) + 1);

            if (depth == 0) {
                dt->root = node;
                node->parent = NULL;
            } else {
                node->parent = stack[depth - 1];
                EosDtNode *parent = stack[depth - 1];
                if (parent->child_count < 16)
                    parent->children[parent->child_count++] = node;
            }
            if (depth < 32) stack[depth] = node;
            depth++;
            break;
        }
        case EOS_DT_END_NODE:
            if (depth > 0) depth--;
            break;

        case EOS_DT_PROP: {
            uint32_t len     = be32(structs + pos); pos += 4;
            uint32_t nameoff = be32(structs + pos); pos += 4;
            if (depth == 0) { pos += align4(len); break; }

            EosDtNode *node = stack[depth - 1];
            if (node->prop_count < EOS_DT_MAX_PROPS) {
                EosDtProp *prop = &node->props[node->prop_count++];
                const char *pname = (const char *)(strings + nameoff);
                strncpy(prop->name, pname, EOS_DT_NAME_MAX - 1);
                uint32_t clen = (len > EOS_DT_PROP_MAX) ? EOS_DT_PROP_MAX : len;
                memcpy(prop->data, structs + pos, clen);
                prop->len = clen;

                /* Cache model and compatible at root level */
                if (depth == 1) {
                    if (strcmp(pname, "model") == 0)
                        strncpy(dt->model, (const char *)prop->data, 63);
                    else if (strcmp(pname, "compatible") == 0)
                        strncpy(dt->compatible, (const char *)prop->data, 127);
                }
                if (strcmp(pname, "phandle") == 0 && len >= 4)
                    node->phandle = be32(prop->data);
            }
            pos += align4(len);
            break;
        }
        case EOS_DT_NOP:
            break;
        case EOS_DT_END:
            return 0;
        default:
            return -1;
        }
    }
    return 0;
}

EosDtNode *eos_dt_find(EosDeviceTree *dt, const char *path) {
    if (!dt || !path || !dt->root) return NULL;
    if (strcmp(path, "/") == 0) return dt->root;

    /* Walk the path */
    const char *p = (*path == '/') ? path + 1 : path;
    EosDtNode *current = dt->root;

    while (*p && current) {
        char component[EOS_DT_NAME_MAX];
        const char *slash = strchr(p, '/');
        int len = slash ? (int)(slash - p) : (int)strlen(p);
        if (len >= EOS_DT_NAME_MAX) len = EOS_DT_NAME_MAX - 1;
        memcpy(component, p, (size_t)len);
        component[len] = '\0';

        EosDtNode *found = NULL;
        for (int i = 0; i < current->child_count; i++) {
            if (strcmp(current->children[i]->name, component) == 0) {
                found = current->children[i];
                break;
            }
        }
        current = found;
        p += len;
        if (*p == '/') p++;
    }
    return current;
}

EosDtNode *eos_dt_find_compatible(EosDeviceTree *dt, const char *compat) {
    if (!dt || !compat) return NULL;
    for (int i = 0; i < dt->node_count; i++) {
        for (int j = 0; j < dt->nodes[i].prop_count; j++) {
            if (strcmp(dt->nodes[i].props[j].name, "compatible") == 0) {
                if (strstr((const char *)dt->nodes[i].props[j].data, compat))
                    return &dt->nodes[i];
            }
        }
    }
    return NULL;
}

EosDtNode *eos_dt_find_by_phandle(EosDeviceTree *dt, uint32_t phandle) {
    if (!dt) return NULL;
    for (int i = 0; i < dt->node_count; i++) {
        if (dt->nodes[i].phandle == phandle)
            return &dt->nodes[i];
    }
    return NULL;
}

const EosDtProp *eos_dt_get_prop(EosDtNode *node, const char *name) {
    if (!node || !name) return NULL;
    for (int i = 0; i < node->prop_count; i++) {
        if (strcmp(node->props[i].name, name) == 0)
            return &node->props[i];
    }
    return NULL;
}

int eos_dt_get_u32(EosDtNode *node, const char *name, uint32_t *val) {
    const EosDtProp *p = eos_dt_get_prop(node, name);
    if (!p || p->len < 4 || !val) return -1;
    *val = be32(p->data);
    return 0;
}

int eos_dt_get_string(EosDtNode *node, const char *name, char *buf, int maxlen) {
    const EosDtProp *p = eos_dt_get_prop(node, name);
    if (!p || !buf || maxlen <= 0) return -1;
    int len = (int)p->len;
    if (len >= maxlen) len = maxlen - 1;
    memcpy(buf, p->data, (size_t)len);
    buf[len] = '\0';
    return 0;
}

int eos_dt_get_reg(EosDtNode *node, uint32_t *addr, uint32_t *size) {
    const EosDtProp *p = eos_dt_get_prop(node, "reg");
    if (!p || p->len < 8) return -1;
    if (addr) *addr = be32(p->data);
    if (size) *size = be32(p->data + 4);
    return 0;
}

int eos_dt_get_irq(EosDtNode *node, int index) {
    const EosDtProp *p = eos_dt_get_prop(node, "interrupts");
    if (!p) return -1;
    uint32_t offset = (uint32_t)(index * 4);
    if (offset + 4 > p->len) return -1;
    return (int)be32(p->data + offset);
}

static void walk_node(EosDtNode *node, int depth, eos_dt_walker fn, void *ctx) {
    if (!node || !fn) return;
    fn(node, depth, ctx);
    for (int i = 0; i < node->child_count; i++)
        walk_node(node->children[i], depth + 1, fn, ctx);
}

void eos_dt_walk(EosDeviceTree *dt, eos_dt_walker fn, void *ctx) {
    if (dt && dt->root) walk_node(dt->root, 0, fn, ctx);
}

static void dump_walker(EosDtNode *node, int depth, void *ctx) {
    (void)ctx;
    char indent[64] = {0};
    for (int i = 0; i < depth && i < 30; i++) { indent[i*2] = ' '; indent[i*2+1] = ' '; }
    fprintf(stderr, "%s%s {\n", indent, node->name[0] ? node->name : "/");
    for (int i = 0; i < node->prop_count; i++) {
        EosDtProp *p = &node->props[i];
        fprintf(stderr, "%s  %s = ", indent, p->name);
        if (p->len > 0 && p->data[p->len - 1] == '\0' && p->len < 128) {
            fprintf(stderr, "\"%s\"", (const char *)p->data);
        } else if (p->len == 4) {
            fprintf(stderr, "<0x%08x>", be32(p->data));
        } else {
            fprintf(stderr, "[%u bytes]", p->len);
        }
        fprintf(stderr, ";\n");
    }
}

void eos_dt_dump(EosDeviceTree *dt) {
    if (!dt) return;
    fprintf(stderr, "=== Device Tree (v%u, %d nodes) ===\n", dt->version, dt->node_count);
    if (dt->model[0]) fprintf(stderr, "Model: %s\n", dt->model);
    if (dt->compatible[0]) fprintf(stderr, "Compatible: %s\n", dt->compatible);
    eos_dt_walk(dt, dump_walker, NULL);
}

int eos_dt_node_count(EosDeviceTree *dt) {
    return dt ? dt->node_count : 0;
}