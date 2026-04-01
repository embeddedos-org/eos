// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/rtos_security.h"
#include <stdio.h>
#include <string.h>

/* ==== MPU-based Task Isolation ==== */

void eos_mpu_init(EosMpuConfig *mpu, int hw_regions) {
    memset(mpu, 0, sizeof(*mpu));
    mpu->mpu_regions_hw = hw_regions > 0 ? hw_regions : 8;
    mpu->enabled = 1;
}

int eos_mpu_add_task(EosMpuConfig *mpu, const char *name, int task_id,
                     int privileged) {
    if (mpu->task_count >= EOS_MPU_MAX_TASKS) return -1;
    EosMpuTask *t = &mpu->tasks[mpu->task_count];
    memset(t, 0, sizeof(*t));
    strncpy(t->task_name, name, sizeof(t->task_name) - 1);
    t->task_id = task_id;
    t->privileged = privileged;
    mpu->task_count++;
    return mpu->task_count - 1;
}

int eos_mpu_add_region(EosMpuConfig *mpu, int task_idx, const char *label,
                       uint32_t base, uint32_t size, EosMpuPermission perm,
                       EosMpuMemType mem_type) {
    if (task_idx < 0 || task_idx >= mpu->task_count) return -1;
    EosMpuTask *t = &mpu->tasks[task_idx];
    if (t->region_count >= EOS_MPU_MAX_REGIONS) return -1;
    if (t->region_count >= mpu->mpu_regions_hw) return -1;

    EosMpuRegion *r = &t->regions[t->region_count];
    strncpy(r->label, label, sizeof(r->label) - 1);
    r->base_addr = base;
    r->size = size;
    r->permissions = perm;
    r->mem_type = mem_type;
    r->enabled = 1;
    r->cacheable = (mem_type == EOS_MPU_MEM_FLASH || mem_type == EOS_MPU_MEM_SRAM);
    r->bufferable = (mem_type == EOS_MPU_MEM_SRAM);
    t->region_count++;
    return 0;
}

int eos_mpu_add_stack_region(EosMpuConfig *mpu, int task_idx,
                             uint32_t stack_base, uint32_t stack_size) {
    return eos_mpu_add_region(mpu, task_idx, "stack",
                              stack_base, stack_size,
                              EOS_MPU_RW, EOS_MPU_MEM_STACK);
}

int eos_mpu_add_shared_region(EosMpuConfig *mpu, int task_idx,
                              uint32_t base, uint32_t size) {
    return eos_mpu_add_region(mpu, task_idx, "shared",
                              base, size,
                              EOS_MPU_RW, EOS_MPU_MEM_SHARED);
}

int eos_mpu_validate(const EosMpuConfig *mpu) {
    int errors = 0;
    for (int t = 0; t < mpu->task_count; t++) {
        const EosMpuTask *task = &mpu->tasks[t];
        if (task->region_count > mpu->mpu_regions_hw) {
            printf("  ERROR: Task '%s' uses %d regions but HW has %d\n",
                   task->task_name, task->region_count, mpu->mpu_regions_hw);
            errors++;
        }
        for (int r = 0; r < task->region_count; r++) {
            const EosMpuRegion *reg = &task->regions[r];
            /* Check power-of-2 alignment (ARM MPU requirement) */
            if (reg->size > 0 && (reg->size & (reg->size - 1)) != 0) {
                printf("  WARN: Task '%s' region '%s' size 0x%x not power-of-2\n",
                       task->task_name, reg->label, reg->size);
            }
            if (reg->base_addr % reg->size != 0 && reg->size > 0) {
                printf("  WARN: Task '%s' region '%s' base 0x%x not aligned to size 0x%x\n",
                       task->task_name, reg->label, reg->base_addr, reg->size);
            }
        }
    }
    return errors;
}

int eos_mpu_generate_config(const EosMpuConfig *mpu, const char *output_path) {
    FILE *fp = fopen(output_path, "w");
    if (!fp) return -1;

    fprintf(fp, "/* EoS auto-generated MPU configuration */\n");
    fprintf(fp, "#ifndef EOS_MPU_CONFIG_H\n#define EOS_MPU_CONFIG_H\n\n");
    fprintf(fp, "#include <stdint.h>\n\n");
    fprintf(fp, "#define EOS_MPU_HW_REGIONS %d\n\n", mpu->mpu_regions_hw);

    const char *perm_str[] = {"NONE","RO","WO","RW","XO","RX","WX","RWX"};

    for (int t = 0; t < mpu->task_count; t++) {
        const EosMpuTask *task = &mpu->tasks[t];
        fprintf(fp, "/* Task: %s (id=%d, %s) */\n",
                task->task_name, task->task_id,
                task->privileged ? "privileged" : "unprivileged");
        fprintf(fp, "#define TASK_%s_REGIONS %d\n", task->task_name, task->region_count);

        for (int r = 0; r < task->region_count; r++) {
            const EosMpuRegion *reg = &task->regions[r];
            int perm_idx = (int)reg->permissions & 0x7;
            fprintf(fp, "  /* [%d] %s: 0x%08x size=0x%x perm=%s */\n",
                    r, reg->label, reg->base_addr, reg->size, perm_str[perm_idx]);
        }
        fprintf(fp, "\n");
    }

    fprintf(fp, "#endif /* EOS_MPU_CONFIG_H */\n");
    fclose(fp);
    return 0;
}

void eos_mpu_dump(const EosMpuConfig *mpu) {
    printf("MPU Configuration (%d tasks, %d HW regions):\n",
           mpu->task_count, mpu->mpu_regions_hw);
    for (int t = 0; t < mpu->task_count; t++) {
        const EosMpuTask *task = &mpu->tasks[t];
        printf("  Task '%s' (id=%d, %s, %d regions):\n",
               task->task_name, task->task_id,
               task->privileged ? "priv" : "unpriv",
               task->region_count);
        for (int r = 0; r < task->region_count; r++) {
            const EosMpuRegion *reg = &task->regions[r];
            printf("    [%d] %-12s 0x%08x +0x%x perm=0x%x %s%s\n",
                   r, reg->label, reg->base_addr, reg->size,
                   reg->permissions,
                   reg->cacheable ? "C" : "",
                   reg->bufferable ? "B" : "");
        }
    }
}

/* ==== RTOS Static Access-Control ==== */

void eos_rtos_acl_init(EosRtosAcl *acl, int default_deny) {
    memset(acl, 0, sizeof(*acl));
    acl->default_deny = default_deny;
}

int eos_rtos_acl_add(EosRtosAcl *acl, const char *task,
                     EosRtosResourceType res_type, const char *res_id,
                     EosMpuPermission perm, int allowed) {
    if (acl->count >= EOS_RTOS_ACL_MAX_RULES) return -1;
    EosRtosAclRule *r = &acl->rules[acl->count];
    strncpy(r->task, task, sizeof(r->task) - 1);
    r->resource_type = res_type;
    strncpy(r->resource_id, res_id, sizeof(r->resource_id) - 1);
    r->permissions = perm;
    r->allowed = allowed;
    acl->count++;
    return 0;
}

int eos_rtos_acl_check(const EosRtosAcl *acl, const char *task,
                       EosRtosResourceType res_type, const char *res_id,
                       EosMpuPermission perm) {
    for (int i = acl->count - 1; i >= 0; i--) {
        const EosRtosAclRule *r = &acl->rules[i];
        int task_match = (strcmp(r->task, "*") == 0 || strcmp(r->task, task) == 0);
        int type_match = (r->resource_type == res_type);
        int id_match = (strcmp(r->resource_id, "*") == 0 || strcmp(r->resource_id, res_id) == 0);
        int perm_match = ((r->permissions & perm) == perm);
        if (task_match && type_match && id_match && perm_match)
            return r->allowed;
    }
    return acl->default_deny ? 0 : 1;
}

int eos_rtos_acl_generate_header(const EosRtosAcl *acl, const char *output_path) {
    FILE *fp = fopen(output_path, "w");
    if (!fp) return -1;

    const char *res_types[] = {
        "PERIPHERAL","MEMORY","IRQ","GPIO","DMA","TIMER","QUEUE","SEMAPHORE","MUTEX"
    };

    fprintf(fp, "/* EoS auto-generated RTOS access-control policy */\n");
    fprintf(fp, "#ifndef EOS_RTOS_ACL_POLICY_H\n#define EOS_RTOS_ACL_POLICY_H\n\n");
    fprintf(fp, "#define EOS_RTOS_ACL_DEFAULT_%s\n\n",
            acl->default_deny ? "DENY" : "ALLOW");
    fprintf(fp, "/* %d rules */\n", acl->count);

    for (int i = 0; i < acl->count; i++) {
        const EosRtosAclRule *r = &acl->rules[i];
        fprintf(fp, "/* Rule %d: %s task=%s res=%s:%s perm=0x%x */\n",
                i, r->allowed ? "ALLOW" : "DENY",
                r->task, res_types[r->resource_type],
                r->resource_id, r->permissions);
    }

    fprintf(fp, "\n#endif /* EOS_RTOS_ACL_POLICY_H */\n");
    fclose(fp);
    return 0;
}

void eos_rtos_acl_dump(const EosRtosAcl *acl) {
    const char *res_types[] = {
        "PERIPHERAL","MEMORY","IRQ","GPIO","DMA","TIMER","QUEUE","SEMAPHORE","MUTEX"
    };
    printf("RTOS Access Control (%d rules, default=%s):\n",
           acl->count, acl->default_deny ? "DENY" : "ALLOW");
    for (int i = 0; i < acl->count; i++) {
        const EosRtosAclRule *r = &acl->rules[i];
        printf("  [%s] task=%-16s %s:%-12s perm=0x%x\n",
               r->allowed ? "ALLOW" : "DENY ",
               r->task, res_types[r->resource_type],
               r->resource_id, r->permissions);
    }
}

/* ==== RTOS Lightweight Audit/Logging ==== */

void eos_rtos_log_init(EosRtosLog *log, EosRtosLogLevel min_level) {
    memset(log, 0, sizeof(*log));
    log->capacity = EOS_RTOS_LOG_MAX_ENTRIES;
    log->min_level = min_level;
    log->wrap = 1;
}

int eos_rtos_log_record(EosRtosLog *log, uint32_t tick, uint8_t task_id,
                        EosRtosLogLevel level, uint16_t code, const char *msg) {
    if (level < log->min_level) return 0;

    int idx;
    if (log->count < log->capacity) {
        idx = log->count++;
    } else if (log->wrap) {
        idx = log->head;
        log->head = (log->head + 1) % log->capacity;
    } else {
        return -1;
    }

    EosRtosLogEntry *e = &log->entries[idx];
    e->tick = tick;
    e->task_id = task_id;
    e->level = (uint8_t)level;
    e->code = code;
    if (msg) strncpy(e->message, msg, EOS_RTOS_LOG_MSG_LEN - 1);
    return 0;
}

int eos_rtos_log_fault(EosRtosLog *log, uint32_t tick, uint8_t task_id,
                       uint16_t fault_code, uint32_t fault_addr) {
    char msg[EOS_RTOS_LOG_MSG_LEN];
    snprintf(msg, sizeof(msg), "FAULT 0x%04x at 0x%08x", fault_code, fault_addr);
    return eos_rtos_log_record(log, tick, task_id, EOS_RTOS_LOG_FAULT, fault_code, msg);
}

void eos_rtos_log_dump(const EosRtosLog *log, int last_n) {
    const char *lvl_str[] = {"TRACE","DEBUG","INFO","WARN","ERROR","FAULT"};
    int start = (last_n > 0 && last_n < log->count) ? log->count - last_n : 0;
    printf("RTOS Log (%d entries, min=%s):\n", log->count, lvl_str[log->min_level]);
    for (int i = start; i < log->count; i++) {
        const EosRtosLogEntry *e = &log->entries[i % log->capacity];
        printf("  [%u] T%d %-5s 0x%04x %s\n",
               e->tick, e->task_id, lvl_str[e->level], e->code, e->message);
    }
}

int eos_rtos_log_export(const EosRtosLog *log, const char *output_path) {
    FILE *fp = fopen(output_path, "w");
    if (!fp) return -1;
    const char *lvl_str[] = {"TRACE","DEBUG","INFO","WARN","ERROR","FAULT"};
    fprintf(fp, "# EoS RTOS audit log\n");
    for (int i = 0; i < log->count; i++) {
        const EosRtosLogEntry *e = &log->entries[i % log->capacity];
        fprintf(fp, "%u,%d,%s,0x%04x,%s\n",
                e->tick, e->task_id, lvl_str[e->level], e->code, e->message);
    }
    fclose(fp);
    return 0;
}
