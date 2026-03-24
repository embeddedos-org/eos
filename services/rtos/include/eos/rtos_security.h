// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file rtos_security.h
 * @brief EoS RTOS Security Services
 *
 * RTOS-specific security features: MPU-based task isolation,
 * static access-control policy for tasks/peripherals, and
 * lightweight audit/logging for resource-constrained targets.
 */

#ifndef EOS_RTOS_SECURITY_H
#define EOS_RTOS_SECURITY_H

#include <stdint.h>
#include <stddef.h>

/* ---- MPU-based Task Isolation ---- */

#define EOS_MPU_MAX_REGIONS   16
#define EOS_MPU_MAX_TASKS     32

typedef enum {
    EOS_MPU_NONE        = 0,
    EOS_MPU_READ        = (1 << 0),
    EOS_MPU_WRITE       = (1 << 1),
    EOS_MPU_EXEC        = (1 << 2),
    EOS_MPU_PRIV_ONLY   = (1 << 3),
    EOS_MPU_RO          = EOS_MPU_READ,
    EOS_MPU_RW          = EOS_MPU_READ | EOS_MPU_WRITE,
    EOS_MPU_RX          = EOS_MPU_READ | EOS_MPU_EXEC,
    EOS_MPU_RWX         = EOS_MPU_READ | EOS_MPU_WRITE | EOS_MPU_EXEC
} EosMpuPermission;

typedef enum {
    EOS_MPU_MEM_FLASH,
    EOS_MPU_MEM_SRAM,
    EOS_MPU_MEM_PERIPHERAL,
    EOS_MPU_MEM_EXTERNAL,
    EOS_MPU_MEM_STACK,
    EOS_MPU_MEM_SHARED
} EosMpuMemType;

typedef struct {
    char label[64];
    uint32_t base_addr;
    uint32_t size;
    EosMpuPermission permissions;
    EosMpuMemType mem_type;
    int enabled;
    int cacheable;
    int bufferable;
} EosMpuRegion;

typedef struct {
    char task_name[64];
    int task_id;
    EosMpuRegion regions[EOS_MPU_MAX_REGIONS];
    int region_count;
    int privileged;
} EosMpuTask;

typedef struct {
    EosMpuTask tasks[EOS_MPU_MAX_TASKS];
    int task_count;
    int mpu_regions_hw;
    int enabled;
} EosMpuConfig;

void eos_mpu_init(EosMpuConfig *mpu, int hw_regions);
int  eos_mpu_add_task(EosMpuConfig *mpu, const char *name, int task_id,
                      int privileged);
int  eos_mpu_add_region(EosMpuConfig *mpu, int task_idx, const char *label,
                        uint32_t base, uint32_t size, EosMpuPermission perm,
                        EosMpuMemType mem_type);
int  eos_mpu_add_stack_region(EosMpuConfig *mpu, int task_idx,
                              uint32_t stack_base, uint32_t stack_size);
int  eos_mpu_add_shared_region(EosMpuConfig *mpu, int task_idx,
                               uint32_t base, uint32_t size);
int  eos_mpu_validate(const EosMpuConfig *mpu);
int  eos_mpu_generate_config(const EosMpuConfig *mpu, const char *output_path);
void eos_mpu_dump(const EosMpuConfig *mpu);

/* ---- RTOS Static Access-Control Policy ---- */

#define EOS_RTOS_ACL_MAX_RULES 64

typedef enum {
    EOS_RTOS_RES_PERIPHERAL,
    EOS_RTOS_RES_MEMORY,
    EOS_RTOS_RES_IRQ,
    EOS_RTOS_RES_GPIO,
    EOS_RTOS_RES_DMA,
    EOS_RTOS_RES_TIMER,
    EOS_RTOS_RES_QUEUE,
    EOS_RTOS_RES_SEMAPHORE,
    EOS_RTOS_RES_MUTEX
} EosRtosResourceType;

typedef struct {
    char task[64];
    EosRtosResourceType resource_type;
    char resource_id[128];
    EosMpuPermission permissions;
    int allowed;
} EosRtosAclRule;

typedef struct {
    EosRtosAclRule rules[EOS_RTOS_ACL_MAX_RULES];
    int count;
    int default_deny;
} EosRtosAcl;

void eos_rtos_acl_init(EosRtosAcl *acl, int default_deny);
int  eos_rtos_acl_add(EosRtosAcl *acl, const char *task,
                      EosRtosResourceType res_type, const char *res_id,
                      EosMpuPermission perm, int allowed);
int  eos_rtos_acl_check(const EosRtosAcl *acl, const char *task,
                        EosRtosResourceType res_type, const char *res_id,
                        EosMpuPermission perm);
int  eos_rtos_acl_generate_header(const EosRtosAcl *acl, const char *output_path);
void eos_rtos_acl_dump(const EosRtosAcl *acl);

/* ---- RTOS Lightweight Audit/Logging ---- */

#define EOS_RTOS_LOG_MAX_ENTRIES  128
#define EOS_RTOS_LOG_MSG_LEN      64

typedef enum {
    EOS_RTOS_LOG_TRACE,
    EOS_RTOS_LOG_DEBUG,
    EOS_RTOS_LOG_INFO,
    EOS_RTOS_LOG_WARN,
    EOS_RTOS_LOG_ERROR,
    EOS_RTOS_LOG_FAULT
} EosRtosLogLevel;

typedef struct {
    uint32_t tick;
    uint8_t  task_id;
    uint8_t  level;
    uint16_t code;
    char     message[EOS_RTOS_LOG_MSG_LEN];
} EosRtosLogEntry;

typedef struct {
    EosRtosLogEntry entries[EOS_RTOS_LOG_MAX_ENTRIES];
    int head;
    int count;
    int capacity;
    EosRtosLogLevel min_level;
    int wrap;
} EosRtosLog;

void eos_rtos_log_init(EosRtosLog *log, EosRtosLogLevel min_level);
int  eos_rtos_log_record(EosRtosLog *log, uint32_t tick, uint8_t task_id,
                         EosRtosLogLevel level, uint16_t code, const char *msg);
int  eos_rtos_log_fault(EosRtosLog *log, uint32_t tick, uint8_t task_id,
                        uint16_t fault_code, uint32_t fault_addr);
void eos_rtos_log_dump(const EosRtosLog *log, int last_n);
int  eos_rtos_log_export(const EosRtosLog *log, const char *output_path);

#endif /* EOS_RTOS_SECURITY_H */
