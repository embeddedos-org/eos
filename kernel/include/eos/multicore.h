// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file multicore.h
 * @brief EoS Multicore / Multiprocessor Framework
 *
 * Supports SMP (Symmetric Multi-Processing), AMP (Asymmetric Multi-Processing),
 * inter-processor communication, spinlocks, shared memory, core affinity,
 * and multi-controller coordination.
 */

#ifndef EOS_MULTICORE_H
#define EOS_MULTICORE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <eos/eos_config.h>

#if EOS_ENABLE_MULTICORE

#ifdef __cplusplus
extern "C" {
#endif

#define EOS_MAX_CORES       16
#define EOS_MAX_IPC_CHANNELS 8

/* ============================================================
 * Core Management
 * ============================================================ */

typedef enum {
    EOS_CORE_OFFLINE  = 0,
    EOS_CORE_ONLINE   = 1,
    EOS_CORE_SLEEPING = 2,
    EOS_CORE_HALTED   = 3,
} eos_core_state_t;

typedef enum {
    EOS_MP_SMP = 0,   /* Symmetric — same OS image on all cores */
    EOS_MP_AMP = 1,   /* Asymmetric — different firmware per core */
} eos_mp_mode_t;

typedef struct {
    uint8_t          core_id;
    eos_core_state_t state;
    uint32_t         freq_hz;
    uint32_t         load_pct;     /* 0–100 */
    eos_mp_mode_t    mode;
} eos_core_info_t;

typedef void (*eos_core_entry_fn)(void *arg);

int  eos_multicore_init(eos_mp_mode_t mode);
void eos_multicore_deinit(void);

uint8_t eos_core_count(void);
uint8_t eos_core_id(void);

int  eos_core_start(uint8_t core_id, eos_core_entry_fn entry, void *arg);
int  eos_core_stop(uint8_t core_id);
int  eos_core_get_info(uint8_t core_id, eos_core_info_t *info);

/* ============================================================
 * Spinlocks — Multi-core synchronization primitives
 * ============================================================ */

typedef volatile uint32_t eos_spinlock_t;

#define EOS_SPINLOCK_INIT 0

void eos_spin_init(eos_spinlock_t *lock);
void eos_spin_lock(eos_spinlock_t *lock);
bool eos_spin_trylock(eos_spinlock_t *lock);
void eos_spin_unlock(eos_spinlock_t *lock);

/* Spinlock with IRQ save/restore */
void eos_spin_lock_irqsave(eos_spinlock_t *lock, uint32_t *flags);
void eos_spin_unlock_irqrestore(eos_spinlock_t *lock, uint32_t flags);

/* ============================================================
 * Inter-Processor Interrupts (IPI)
 * ============================================================ */

typedef enum {
    EOS_IPI_RESCHEDULE  = 0,
    EOS_IPI_CALL_FUNC   = 1,
    EOS_IPI_STOP        = 2,
    EOS_IPI_USER        = 3,
} eos_ipi_type_t;

typedef void (*eos_ipi_handler_t)(uint8_t from_core, uint32_t data, void *ctx);

int  eos_ipi_send(uint8_t target_core, eos_ipi_type_t type, uint32_t data);
int  eos_ipi_broadcast(eos_ipi_type_t type, uint32_t data);
int  eos_ipi_register_handler(eos_ipi_type_t type, eos_ipi_handler_t handler,
                               void *ctx);

/* ============================================================
 * Shared Memory Regions
 * ============================================================ */

typedef struct {
    const char *name;
    void       *base;
    size_t      size;
    bool        cached;
} eos_shmem_config_t;

typedef struct {
    void   *addr;
    size_t  size;
    int     id;
} eos_shmem_region_t;

int  eos_shmem_create(const eos_shmem_config_t *cfg, eos_shmem_region_t *region);
int  eos_shmem_open(const char *name, eos_shmem_region_t *region);
int  eos_shmem_close(eos_shmem_region_t *region);
void eos_shmem_flush(const eos_shmem_region_t *region);
void eos_shmem_invalidate(const eos_shmem_region_t *region);

/* ============================================================
 * Task Core Affinity
 * ============================================================ */

typedef uint32_t eos_core_mask_t;

#define EOS_CORE_MASK(c)     (1U << (c))
#define EOS_CORE_MASK_ALL    0xFFFFFFFF
#define EOS_CORE_MASK_NONE   0x00000000

int  eos_task_set_affinity(uint32_t task_id, eos_core_mask_t mask);
int  eos_task_get_affinity(uint32_t task_id, eos_core_mask_t *mask);
int  eos_task_migrate(uint32_t task_id, uint8_t target_core);

/* ============================================================
 * Multi-Controller / Remote Processor Communication
 * ============================================================ */

typedef enum {
    EOS_RPROC_OFFLINE = 0,
    EOS_RPROC_RUNNING = 1,
    EOS_RPROC_CRASHED = 2,
} eos_rproc_state_t;

typedef struct {
    uint8_t  id;
    const char *name;
    uint32_t firmware_addr;
    uint32_t firmware_size;
} eos_rproc_config_t;

typedef void (*eos_rproc_rx_callback_t)(uint8_t rproc_id,
                                         const void *data, size_t len,
                                         void *ctx);

int  eos_rproc_init(const eos_rproc_config_t *cfg);
int  eos_rproc_start(uint8_t rproc_id);
int  eos_rproc_stop(uint8_t rproc_id);
eos_rproc_state_t eos_rproc_get_state(uint8_t rproc_id);

int  eos_rproc_send(uint8_t rproc_id, const void *data, size_t len);
int  eos_rproc_set_rx_callback(uint8_t rproc_id,
                                eos_rproc_rx_callback_t cb, void *ctx);

/* ============================================================
 * Memory Barriers & Atomic Operations
 * ============================================================ */

void eos_dmb(void);  /* Data Memory Barrier */
void eos_dsb(void);  /* Data Synchronization Barrier */
void eos_isb(void);  /* Instruction Synchronization Barrier */

int32_t eos_atomic_add(volatile int32_t *ptr, int32_t val);
int32_t eos_atomic_sub(volatile int32_t *ptr, int32_t val);
int32_t eos_atomic_cas(volatile int32_t *ptr, int32_t expected, int32_t desired);
int32_t eos_atomic_load(volatile const int32_t *ptr);
void    eos_atomic_store(volatile int32_t *ptr, int32_t val);

#ifdef __cplusplus
}
#endif

#endif /* EOS_ENABLE_MULTICORE */
#endif /* EOS_MULTICORE_H */
