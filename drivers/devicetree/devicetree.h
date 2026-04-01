// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EOS_DEVICETREE_H
#define EOS_DEVICETREE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EOS_DT_MAX_NODES       128
#define EOS_DT_MAX_PROPS       16
#define EOS_DT_NAME_MAX        64
#define EOS_DT_PROP_MAX        256

#define EOS_DT_MAGIC           0xD00DFEED
#define EOS_DT_BEGIN_NODE      0x00000001
#define EOS_DT_END_NODE        0x00000002
#define EOS_DT_PROP            0x00000003
#define EOS_DT_NOP             0x00000004
#define EOS_DT_END             0x00000009

typedef struct {
    char     name[EOS_DT_NAME_MAX];
    uint8_t  data[EOS_DT_PROP_MAX];
    uint32_t len;
} EosDtProp;

typedef struct eos_dt_node {
    char                  name[EOS_DT_NAME_MAX];
    EosDtProp             props[EOS_DT_MAX_PROPS];
    int                   prop_count;
    struct eos_dt_node   *parent;
    struct eos_dt_node   *children[16];
    int                   child_count;
    uint32_t              phandle;
} EosDtNode;

typedef struct {
    EosDtNode  nodes[EOS_DT_MAX_NODES];
    int        node_count;
    EosDtNode *root;
    uint32_t   version;
    uint32_t   boot_cpuid;
    char       model[64];
    char       compatible[128];
} EosDeviceTree;

/* Parse a flattened device tree blob (.dtb) */
int  eos_dt_parse(EosDeviceTree *dt, const uint8_t *dtb, uint32_t size);

/* Node lookup */
EosDtNode *eos_dt_find(EosDeviceTree *dt, const char *path);
EosDtNode *eos_dt_find_compatible(EosDeviceTree *dt, const char *compat);
EosDtNode *eos_dt_find_by_phandle(EosDeviceTree *dt, uint32_t phandle);

/* Property access */
const EosDtProp *eos_dt_get_prop(EosDtNode *node, const char *name);
int       eos_dt_get_u32(EosDtNode *node, const char *name, uint32_t *val);
int       eos_dt_get_string(EosDtNode *node, const char *name, char *buf, int maxlen);
int       eos_dt_get_reg(EosDtNode *node, uint32_t *addr, uint32_t *size);
int       eos_dt_get_irq(EosDtNode *node, int index);

/* Iteration */
typedef void (*eos_dt_walker)(EosDtNode *node, int depth, void *ctx);
void eos_dt_walk(EosDeviceTree *dt, eos_dt_walker fn, void *ctx);

/* Introspection */
void eos_dt_dump(EosDeviceTree *dt);
int  eos_dt_node_count(EosDeviceTree *dt);

#ifdef __cplusplus
}
#endif

#endif /* EOS_DEVICETREE_H */