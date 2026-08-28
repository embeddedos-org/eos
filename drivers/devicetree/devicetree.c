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

/* ------------------------------------------------------------------
 * Bounds-checked primitives
 *
 * Everything below treats the blob as untrusted. A flattened device tree
 * usually arrives from a prior boot stage or a flash region, so every offset
 * and length it contains has to be validated against the buffer we were
 * actually handed before it is used to index into it.
 * ------------------------------------------------------------------ */

/* Round `v` up to a 4-byte boundary, refusing the cases where that wraps. */
static int dt_align4(uint32_t v, uint32_t *out)
{
    if (v > UINT32_MAX - 3u) return -1;
    *out = (v + 3u) & ~3u;
    return 0;
}

/* Read a big-endian u32 at `off`, provided all four bytes are inside `len`. */
static int dt_read_be32(const uint8_t *buf, uint32_t len, uint32_t off,
                        uint32_t *out)
{
    if (off > len || len - off < 4u) return -1;
    *out = be32(buf + off);
    return 0;
}

/* Length of the NUL-terminated string at `off`, without reading past `len`.
 * Returns -1 when the buffer ends before a terminator is found. */
static int dt_strlen_at(const uint8_t *buf, uint32_t len, uint32_t off,
                        uint32_t *out)
{
    if (off >= len) return -1;
    for (uint32_t i = off; i < len; i++) {
        if (buf[i] == '\0') { *out = i - off; return 0; }
    }
    return -1;
}

/* Copy a bounded, possibly unterminated byte range into a NUL-terminated
 * destination buffer. */
static void dt_copy_str(char *dst, uint32_t dst_sz, const uint8_t *src,
                        uint32_t src_len)
{
    uint32_t n = (src_len < dst_sz - 1u) ? src_len : dst_sz - 1u;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

/* Cache a string-valued root property. The stored value is only `prop->len`
 * bytes and is not guaranteed to be terminated, so stop at whichever comes
 * first: an embedded NUL, the property length, or the destination size. */
static void dt_cache_string_prop(char *dst, uint32_t dst_sz,
                                 const EosDtProp *prop)
{
    uint32_t n = prop->len;
    uint32_t i = 0;
    while (i < n && prop->data[i] != '\0') i++;
    dt_copy_str(dst, dst_sz, prop->data, i);
}

int eos_dt_parse(EosDeviceTree *dt, const uint8_t *dtb, uint32_t size)
{
    if (!dt || !dtb || size < EOS_DT_HEADER_SIZE) return -1;
    memset(dt, 0, sizeof(*dt));

    if (be32(dtb) != EOS_DT_MAGIC) return -1;

    uint32_t totalsize   = be32(dtb + 4);
    uint32_t off_struct  = be32(dtb + 8);
    uint32_t off_string  = be32(dtb + 12);
    uint32_t size_string = be32(dtb + 32);
    uint32_t size_struct = be32(dtb + 36);

    dt->version    = be32(dtb + 20);
    dt->boot_cpuid = be32(dtb + 28);

    /* The blob may sit inside a larger buffer, but it must never claim to be
     * larger than the buffer we were given. */
    uint32_t blob_len = size;
    if (totalsize > blob_len) return -1;
    if (totalsize >= EOS_DT_HEADER_SIZE) blob_len = totalsize;

    /* Both blocks have to begin inside the blob. Without this the subtraction
     * below wraps and the walk runs off the end of the buffer. */
    if (off_struct >= blob_len || off_string >= blob_len) return -1;

    const uint8_t *structs = dtb + off_struct;
    const uint8_t *strings = dtb + off_string;

    /* Honour the declared block sizes when they fit, otherwise clamp to the
     * rest of the blob. Either way the walk stays inside the buffer. */
    uint32_t struct_len = blob_len - off_struct;
    uint32_t string_len = blob_len - off_string;
    if (size_struct != 0 && size_struct < struct_len) struct_len = size_struct;
    if (size_string != 0 && size_string < string_len) string_len = size_string;

    EosDtNode *stack[EOS_DT_MAX_DEPTH];
    uint32_t depth = 0;
    uint32_t pos   = 0;

    /* `pos` advances by at least 4 on every iteration and every read is bounds
     * checked against `struct_len`, so this terminates on any input. */
    for (;;) {
        uint32_t token;
        if (dt_read_be32(structs, struct_len, pos, &token) != 0) return -1;
        pos += 4;

        switch (token) {
        case EOS_DT_BEGIN_NODE: {
            uint32_t name_len;
            if (dt_strlen_at(structs, struct_len, pos, &name_len) != 0)
                return -1;

            uint32_t adv;
            if (dt_align4(name_len + 1u, &adv) != 0) return -1;
            if (adv > struct_len - pos) return -1;

            if (dt->node_count >= EOS_DT_MAX_NODES) return -1;
            if (depth >= EOS_DT_MAX_DEPTH) return -1;

            EosDtNode *node = &dt->nodes[dt->node_count++];
            memset(node, 0, sizeof(*node));
            dt_copy_str(node->name, sizeof(node->name), structs + pos, name_len);

            if (depth == 0) {
                dt->root = node;
                node->parent = NULL;
            } else {
                EosDtNode *parent = stack[depth - 1];
                node->parent = parent;
                if (parent->child_count < EOS_DT_MAX_CHILDREN)
                    parent->children[parent->child_count++] = node;
            }
            stack[depth++] = node;

            pos += adv;
            break;
        }

        case EOS_DT_END_NODE:
            /* A close with nothing open means the blob is malformed. */
            if (depth == 0) return -1;
            depth--;
            break;

        case EOS_DT_PROP: {
            uint32_t len, nameoff;
            if (dt_read_be32(structs, struct_len, pos, &len) != 0) return -1;
            pos += 4;
            if (dt_read_be32(structs, struct_len, pos, &nameoff) != 0) return -1;
            pos += 4;

            /* The value, and its 4-byte-aligned padding, must lie inside the
             * struct block. */
            uint32_t adv;
            if (len > struct_len - pos) return -1;
            if (dt_align4(len, &adv) != 0) return -1;
            if (adv > struct_len - pos) return -1;

            if (depth > 0) {
                /* The name lives in the strings block at an offset the blob
                 * chose, so it needs validating before it is dereferenced. */
                uint32_t pname_len;
                if (dt_strlen_at(strings, string_len, nameoff, &pname_len) != 0)
                    return -1;
                const char *pname = (const char *)(strings + nameoff);

                EosDtNode *node = stack[depth - 1];
                if (node->prop_count < EOS_DT_MAX_PROPS) {
                    EosDtProp *prop = &node->props[node->prop_count++];
                    dt_copy_str(prop->name, sizeof(prop->name),
                                (const uint8_t *)pname, pname_len);

                    uint32_t clen = (len > EOS_DT_PROP_MAX) ? EOS_DT_PROP_MAX : len;
                    memcpy(prop->data, structs + pos, clen);
                    prop->len = clen;

                    /* Cache model and compatible at root level */
                    if (depth == 1) {
                        if (strcmp(pname, "model") == 0) {
                            dt_cache_string_prop(dt->model, sizeof(dt->model), prop);
                        } else if (strcmp(pname, "compatible") == 0) {
                            dt_cache_string_prop(dt->compatible,
                                                 sizeof(dt->compatible), prop);
                        }
                    }
                    if (strcmp(pname, "phandle") == 0 && prop->len >= 4)
                        node->phandle = be32(prop->data);
                }
            }

            pos += adv;
            break;
        }

        case EOS_DT_NOP:
            break;

        case EOS_DT_END:
            /* Every node that was opened must have been closed. */
            return (depth == 0) ? 0 : -1;

        default:
            return -1;
        }
    }
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

/* Substring search restricted to `prop->len`.
 *
 * A property value is raw bytes, and a value that filled the property buffer
 * has no terminator, so strstr() would run past the end of prop->data. */
static int dt_prop_contains(const EosDtProp *prop, const char *needle)
{
    size_t nlen = strlen(needle);
    if (nlen == 0) return 1;
    if (prop->len < nlen) return 0;
    for (uint32_t i = 0; i + nlen <= prop->len; i++) {
        if (memcmp(prop->data + i, needle, nlen) == 0) return 1;
    }
    return 0;
}

EosDtNode *eos_dt_find_compatible(EosDeviceTree *dt, const char *compat) {
    if (!dt || !compat) return NULL;
    for (int i = 0; i < dt->node_count; i++) {
        for (int j = 0; j < dt->nodes[i].prop_count; j++) {
            if (strcmp(dt->nodes[i].props[j].name, "compatible") == 0) {
                if (dt_prop_contains(&dt->nodes[i].props[j], compat))
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
    /* A negative index, or one large enough that index * 4 overflows, must be
     * rejected before it is turned into an offset. */
    if (!p || index < 0 || index > (int)(UINT32_MAX / 4u)) return -1;
    uint32_t offset = (uint32_t)index * 4u;
    if (offset > p->len || p->len - offset < 4u) return -1;
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