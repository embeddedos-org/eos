// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file multicore.c
 * @brief EoS Multicore Framework — Implementation
 *
 * Provides core management, spinlocks, IPI, shared memory,
 * affinity, remote processor control, and atomic operations.
 */

#include <eos/multicore.h>
#include <string.h>

#if EOS_ENABLE_MULTICORE

/* ---- Internal state ---- */

static bool mc_initialized = false;
static eos_mp_mode_t mc_mode = EOS_MP_SMP;

static eos_core_info_t cores[EOS_MAX_CORES];
static uint8_t num_cores = 1;

typedef struct {
    eos_ipi_handler_t handler;
    void *ctx;
} ipi_slot_t;

static ipi_slot_t ipi_handlers[4]; /* one per eos_ipi_type_t */

#define MAX_SHMEM 8
static eos_shmem_region_t shmem_regions[MAX_SHMEM];
static eos_shmem_config_t shmem_configs[MAX_SHMEM];
static int shmem_count = 0;

#define MAX_RPROC 4
typedef struct {
    eos_rproc_config_t cfg;
    eos_rproc_state_t  state;
    eos_rproc_rx_callback_t rx_cb;
    void *rx_ctx;
    bool configured;
} rproc_slot_t;

static rproc_slot_t rprocs[MAX_RPROC];

/* ---- Core Management ---- */

int eos_multicore_init(eos_mp_mode_t mode)
{
    mc_mode = mode;
    memset(cores, 0, sizeof(cores));
    memset(ipi_handlers, 0, sizeof(ipi_handlers));
    memset(shmem_regions, 0, sizeof(shmem_regions));
    memset(rprocs, 0, sizeof(rprocs));
    shmem_count = 0;

    /* Core 0 is always online (boot core) */
    cores[0].core_id = 0;
    cores[0].state = EOS_CORE_ONLINE;
    cores[0].mode = mode;
    num_cores = 1;

    mc_initialized = true;
    return 0;
}

void eos_multicore_deinit(void)
{
    mc_initialized = false;
}

uint8_t eos_core_count(void)
{
    return num_cores;
}

uint8_t eos_core_id(void)
{
    /* Stub: real implementation reads CPU ID register */
    return 0;
}

int eos_core_start(uint8_t core_id, eos_core_entry_fn entry, void *arg)
{
    if (!mc_initialized || core_id >= EOS_MAX_CORES) return -1;
    if (!entry) return -1;

    cores[core_id].core_id = core_id;
    cores[core_id].state = EOS_CORE_ONLINE;
    cores[core_id].mode = mc_mode;
    if (core_id >= num_cores) num_cores = core_id + 1;

    /* Stub: real implementation powers on core and jumps to entry(arg) */
    (void)entry; (void)arg;
    return 0;
}

int eos_core_stop(uint8_t core_id)
{
    if (!mc_initialized || core_id >= EOS_MAX_CORES) return -1;
    if (core_id == 0) return -1; /* can't stop boot core */
    cores[core_id].state = EOS_CORE_HALTED;
    return 0;
}

int eos_core_get_info(uint8_t core_id, eos_core_info_t *info)
{
    if (!mc_initialized || core_id >= EOS_MAX_CORES || !info) return -1;
    memcpy(info, &cores[core_id], sizeof(*info));
    return 0;
}

/* ---- Spinlocks ---- */

void eos_spin_init(eos_spinlock_t *lock)
{
    if (lock) *lock = 0;
}

void eos_spin_lock(eos_spinlock_t *lock)
{
    if (!lock) return;
    /* Stub: real implementation uses atomic test-and-set */
    while (*lock) { /* spin */ }
    *lock = 1;
}

bool eos_spin_trylock(eos_spinlock_t *lock)
{
    if (!lock) return false;
    if (*lock) return false;
    *lock = 1;
    return true;
}

void eos_spin_unlock(eos_spinlock_t *lock)
{
    if (lock) *lock = 0;
}

void eos_spin_lock_irqsave(eos_spinlock_t *lock, uint32_t *flags)
{
    if (flags) *flags = 0; /* stub: save interrupt state */
    eos_spin_lock(lock);
}

void eos_spin_unlock_irqrestore(eos_spinlock_t *lock, uint32_t flags)
{
    eos_spin_unlock(lock);
    (void)flags; /* stub: restore interrupt state */
}

/* ---- IPI ---- */

int eos_ipi_send(uint8_t target_core, eos_ipi_type_t type, uint32_t data)
{
    if (!mc_initialized || target_core >= EOS_MAX_CORES) return -1;
    if (type > EOS_IPI_USER) return -1;

    ipi_slot_t *slot = &ipi_handlers[type];
    if (slot->handler) {
        slot->handler(eos_core_id(), data, slot->ctx);
    }
    return 0;
}

int eos_ipi_broadcast(eos_ipi_type_t type, uint32_t data)
{
    if (!mc_initialized) return -1;
    for (uint8_t i = 0; i < num_cores; i++) {
        if (i != eos_core_id() && cores[i].state == EOS_CORE_ONLINE) {
            eos_ipi_send(i, type, data);
        }
    }
    return 0;
}

int eos_ipi_register_handler(eos_ipi_type_t type, eos_ipi_handler_t handler,
                              void *ctx)
{
    if (type > EOS_IPI_USER) return -1;
    ipi_handlers[type].handler = handler;
    ipi_handlers[type].ctx = ctx;
    return 0;
}

/* ---- Shared Memory ---- */

int eos_shmem_create(const eos_shmem_config_t *cfg, eos_shmem_region_t *region)
{
    if (!mc_initialized || !cfg || !region) return -1;
    if (shmem_count >= MAX_SHMEM) return -1;

    int id = shmem_count;
    memcpy(&shmem_configs[id], cfg, sizeof(*cfg));
    region->addr = cfg->base;
    region->size = cfg->size;
    region->id = id;
    memcpy(&shmem_regions[id], region, sizeof(*region));
    shmem_count++;
    return 0;
}

int eos_shmem_open(const char *name, eos_shmem_region_t *region)
{
    if (!mc_initialized || !name || !region) return -1;
    for (int i = 0; i < shmem_count; i++) {
        if (shmem_configs[i].name &&
            strcmp(shmem_configs[i].name, name) == 0) {
            memcpy(region, &shmem_regions[i], sizeof(*region));
            return 0;
        }
    }
    return -1;
}

int eos_shmem_close(eos_shmem_region_t *region)
{
    if (!region) return -1;
    region->addr = NULL;
    region->size = 0;
    return 0;
}

void eos_shmem_flush(const eos_shmem_region_t *region)
{
    (void)region; /* stub: real impl flushes cache lines */
}

void eos_shmem_invalidate(const eos_shmem_region_t *region)
{
    (void)region; /* stub: real impl invalidates cache lines */
}

/* ---- Task Core Affinity ---- */

int eos_task_set_affinity(uint32_t task_id, eos_core_mask_t mask)
{
    (void)task_id; (void)mask;
    return 0; /* stub */
}

int eos_task_get_affinity(uint32_t task_id, eos_core_mask_t *mask)
{
    if (!mask) return -1;
    (void)task_id;
    *mask = EOS_CORE_MASK_ALL;
    return 0;
}

int eos_task_migrate(uint32_t task_id, uint8_t target_core)
{
    if (target_core >= EOS_MAX_CORES) return -1;
    (void)task_id;
    return 0;
}

/* ---- Remote Processor ---- */

int eos_rproc_init(const eos_rproc_config_t *cfg)
{
    if (!cfg || cfg->id >= MAX_RPROC) return -1;
    rproc_slot_t *r = &rprocs[cfg->id];
    memcpy(&r->cfg, cfg, sizeof(*cfg));
    r->state = EOS_RPROC_OFFLINE;
    r->rx_cb = NULL;
    r->rx_ctx = NULL;
    r->configured = true;
    return 0;
}

int eos_rproc_start(uint8_t rproc_id)
{
    if (rproc_id >= MAX_RPROC || !rprocs[rproc_id].configured) return -1;
    rprocs[rproc_id].state = EOS_RPROC_RUNNING;
    return 0;
}

int eos_rproc_stop(uint8_t rproc_id)
{
    if (rproc_id >= MAX_RPROC || !rprocs[rproc_id].configured) return -1;
    rprocs[rproc_id].state = EOS_RPROC_OFFLINE;
    return 0;
}

eos_rproc_state_t eos_rproc_get_state(uint8_t rproc_id)
{
    if (rproc_id >= MAX_RPROC) return EOS_RPROC_OFFLINE;
    return rprocs[rproc_id].state;
}

int eos_rproc_send(uint8_t rproc_id, const void *data, size_t len)
{
    (void)rproc_id; (void)data; (void)len;
    return -1; /* stub */
}

int eos_rproc_set_rx_callback(uint8_t rproc_id,
                               eos_rproc_rx_callback_t cb, void *ctx)
{
    if (rproc_id >= MAX_RPROC) return -1;
    rprocs[rproc_id].rx_cb = cb;
    rprocs[rproc_id].rx_ctx = ctx;
    return 0;
}

/* ---- Memory Barriers ---- */

void eos_dmb(void)
{
#if defined(__GNUC__) || defined(__clang__)
    __asm__ volatile("" ::: "memory");
#elif defined(_MSC_VER)
    _ReadWriteBarrier();
#endif
}

void eos_dsb(void) { eos_dmb(); }
void eos_isb(void) { eos_dmb(); }

/* ---- Atomics ---- */

int32_t eos_atomic_add(volatile int32_t *ptr, int32_t val)
{
    if (!ptr) return 0;
    int32_t old = *ptr;
    *ptr += val;
    return old;
}

int32_t eos_atomic_sub(volatile int32_t *ptr, int32_t val)
{
    if (!ptr) return 0;
    int32_t old = *ptr;
    *ptr -= val;
    return old;
}

int32_t eos_atomic_cas(volatile int32_t *ptr, int32_t expected, int32_t desired)
{
    if (!ptr) return 0;
    int32_t old = *ptr;
    if (old == expected) *ptr = desired;
    return old;
}

int32_t eos_atomic_load(volatile const int32_t *ptr)
{
    return ptr ? *ptr : 0;
}

void eos_atomic_store(volatile int32_t *ptr, int32_t val)
{
    if (ptr) *ptr = val;
}

#endif /* EOS_ENABLE_MULTICORE */
