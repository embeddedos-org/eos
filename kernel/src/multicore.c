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

/* ---- Shared-memory mailbox layout for rproc_send ---- */
#define RPROC_MBOX_SIZE 256
typedef struct {
    volatile uint32_t len;
    volatile uint8_t  data[RPROC_MBOX_SIZE];
} rproc_mbox_t;

static rproc_mbox_t rproc_mbox[MAX_RPROC];

/* ---- Core Management ---- */

int eos_multicore_init(eos_mp_mode_t mode)
{
    mc_mode = mode;
    memset(cores, 0, sizeof(cores));
    memset(ipi_handlers, 0, sizeof(ipi_handlers));
    memset(shmem_regions, 0, sizeof(shmem_regions));
    memset(rprocs, 0, sizeof(rprocs));
    memset(rproc_mbox, 0, sizeof(rproc_mbox));
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
#if defined(__aarch64__)
    uint64_t mpidr;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return (uint8_t)(mpidr & 0xFF);
#elif defined(__arm__)
    uint32_t mpidr;
    __asm__ volatile("mrc p15, 0, %0, c0, c0, 5" : "=r"(mpidr));
    return (uint8_t)(mpidr & 0x03);
#elif defined(__x86_64__) || defined(__i386__)
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(1));
    return (uint8_t)((ebx >> 24) & 0xFF); /* Initial APIC ID */
#elif defined(__riscv)
    unsigned long hartid;
    __asm__ volatile("csrr %0, mhartid" : "=r"(hartid));
    return (uint8_t)(hartid & 0xFF);
#elif defined(_MSC_VER) && defined(_M_X64)
    int cpuinfo[4];
    __cpuid(cpuinfo, 1);
    return (uint8_t)((cpuinfo[1] >> 24) & 0xFF);
#else
    return 0;
#endif
}

int eos_core_start(uint8_t core_id, eos_core_entry_fn entry, void *arg)
{
    if (!mc_initialized || core_id >= EOS_MAX_CORES) return -1;
    if (!entry) return -1;

    cores[core_id].core_id = core_id;
    cores[core_id].state = EOS_CORE_ONLINE;
    cores[core_id].mode = mc_mode;
    if (core_id >= num_cores) num_cores = core_id + 1;

    /* Platform-specific core bringup would go here (e.g., PSCI on ARM).
     * In a real system, the entry point and arg would be written to a
     * well-known address and a SEV/IPI issued to wake the core. */
    (void)entry; (void)arg;
    return 0;
}

int eos_core_stop(uint8_t core_id)
{
    if (!mc_initialized || core_id >= EOS_MAX_CORES) return -1;
    if (core_id == 0) return -1; /* can't stop boot core */
    cores[core_id].state = EOS_CORE_OFFLINE;
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
    if (lock) __atomic_store_n(lock, 0, __ATOMIC_RELEASE);
}

void eos_spin_lock(eos_spinlock_t *lock)
{
    if (!lock) return;
    while (__atomic_exchange_n(lock, 1, __ATOMIC_ACQUIRE)) {
        /* Spin — hint to the processor that this is a spin-wait loop */
#if defined(__aarch64__) || defined(__arm__)
        __asm__ volatile("wfe");
#elif defined(__x86_64__) || defined(__i386__)
        __asm__ volatile("pause");
#elif defined(__riscv)
        /* No standard spin hint on RISC-V; just spin */
#endif
    }
}

bool eos_spin_trylock(eos_spinlock_t *lock)
{
    if (!lock) return false;
    uint32_t expected = 0;
    return __atomic_compare_exchange_n(lock, &expected, 1, false,
                                       __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
}

void eos_spin_unlock(eos_spinlock_t *lock)
{
    if (!lock) return;
    __atomic_store_n(lock, 0, __ATOMIC_RELEASE);
#if defined(__aarch64__) || defined(__arm__)
    __asm__ volatile("sev"); /* Wake cores waiting in WFE */
#endif
}

void eos_spin_lock_irqsave(eos_spinlock_t *lock, uint32_t *flags)
{
    uint32_t saved = 0;
#if defined(__aarch64__)
    __asm__ volatile("mrs %0, daif" : "=r"(saved));
    __asm__ volatile("msr daifset, #0xF"); /* Mask all exceptions */
#elif defined(__arm__)
    __asm__ volatile("mrs %0, cpsr" : "=r"(saved));
    __asm__ volatile("cpsid if"); /* Disable IRQ + FIQ */
#elif defined(__x86_64__) || defined(__i386__)
    __asm__ volatile("pushf; pop %0" : "=r"(saved));
    __asm__ volatile("cli"); /* Clear interrupt flag */
#elif defined(__riscv)
    __asm__ volatile("csrr %0, mstatus" : "=r"(saved));
    __asm__ volatile("csrc mstatus, %0" :: "r"(0x8)); /* Clear MIE */
#else
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
    if (flags) *flags = saved;
    eos_spin_lock(lock);
}

void eos_spin_unlock_irqrestore(eos_spinlock_t *lock, uint32_t flags)
{
    eos_spin_unlock(lock);
#if defined(__aarch64__)
    __asm__ volatile("msr daif, %0" :: "r"((uint64_t)flags));
#elif defined(__arm__)
    __asm__ volatile("msr cpsr_c, %0" :: "r"(flags));
#elif defined(__x86_64__) || defined(__i386__)
    if (flags & 0x200) { /* IF was set — re-enable interrupts */
        __asm__ volatile("sti");
    }
#elif defined(__riscv)
    if (flags & 0x8) { /* MIE was set — re-enable */
        __asm__ volatile("csrs mstatus, %0" :: "r"(0x8));
    }
#else
    (void)flags;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
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
    if (!region || !region->addr || region->size == 0) return;

#if defined(__aarch64__)
    /* Clean + Invalidate by VA to PoC for each cache line (typically 64 bytes) */
    uintptr_t start = (uintptr_t)region->addr;
    uintptr_t end = start + region->size;
    for (uintptr_t addr = start & ~63UL; addr < end; addr += 64) {
        __asm__ volatile("dc civac, %0" :: "r"(addr) : "memory");
    }
    __asm__ volatile("dsb sy" ::: "memory");
#elif defined(__arm__)
    /* ARMv7 DCCIMVAC: Clean + Invalidate D-cache by MVA to PoC */
    uintptr_t start = (uintptr_t)region->addr;
    uintptr_t end = start + region->size;
    for (uintptr_t addr = start & ~31UL; addr < end; addr += 32) {
        __asm__ volatile("mcr p15, 0, %0, c7, c14, 1" :: "r"(addr) : "memory");
    }
    __asm__ volatile("dsb" ::: "memory");
#elif defined(__x86_64__) || defined(__i386__)
    uintptr_t start = (uintptr_t)region->addr;
    uintptr_t end = start + region->size;
    for (uintptr_t addr = start & ~63UL; addr < end; addr += 64) {
        __asm__ volatile("clflush (%0)" :: "r"(addr) : "memory");
    }
    __asm__ volatile("mfence" ::: "memory");
#else
    /* Fallback: compiler barrier only */
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    (void)region;
#endif
}

void eos_shmem_invalidate(const eos_shmem_region_t *region)
{
    if (!region || !region->addr || region->size == 0) return;

#if defined(__aarch64__)
    uintptr_t start = (uintptr_t)region->addr;
    uintptr_t end = start + region->size;
    for (uintptr_t addr = start & ~63UL; addr < end; addr += 64) {
        __asm__ volatile("dc ivac, %0" :: "r"(addr) : "memory");
    }
    __asm__ volatile("dsb sy" ::: "memory");
#elif defined(__arm__)
    uintptr_t start = (uintptr_t)region->addr;
    uintptr_t end = start + region->size;
    for (uintptr_t addr = start & ~31UL; addr < end; addr += 32) {
        __asm__ volatile("mcr p15, 0, %0, c7, c6, 1" :: "r"(addr) : "memory");
    }
    __asm__ volatile("dsb" ::: "memory");
#elif defined(__x86_64__) || defined(__i386__)
    /* x86 is cache-coherent; clflush serves as invalidate too */
    uintptr_t start = (uintptr_t)region->addr;
    uintptr_t end = start + region->size;
    for (uintptr_t addr = start & ~63UL; addr < end; addr += 64) {
        __asm__ volatile("clflush (%0)" :: "r"(addr) : "memory");
    }
    __asm__ volatile("mfence" ::: "memory");
#else
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    (void)region;
#endif
}

/* ---- Task Core Affinity ---- */

#define MAX_TASKS_AFFINITY 64
static eos_core_mask_t task_affinity[MAX_TASKS_AFFINITY];
static bool task_affinity_set[MAX_TASKS_AFFINITY];

int eos_task_set_affinity(uint32_t task_id, eos_core_mask_t mask)
{
    if (task_id >= MAX_TASKS_AFFINITY) return -1;
    task_affinity[task_id] = mask;
    task_affinity_set[task_id] = true;
    return 0;
}

int eos_task_get_affinity(uint32_t task_id, eos_core_mask_t *mask)
{
    if (!mask) return -1;
    if (task_id >= MAX_TASKS_AFFINITY) return -1;
    *mask = task_affinity_set[task_id] ? task_affinity[task_id] : EOS_CORE_MASK_ALL;
    return 0;
}

int eos_task_migrate(uint32_t task_id, uint8_t target_core)
{
    if (target_core >= EOS_MAX_CORES) return -1;
    if (cores[target_core].state != EOS_CORE_ONLINE) return -1;

    /* Update affinity to include target core */
    if (task_id < MAX_TASKS_AFFINITY) {
        task_affinity[task_id] = EOS_CORE_MASK(target_core);
        task_affinity_set[task_id] = true;
    }

    /* Send a reschedule IPI to the target core so its scheduler picks up the task */
    eos_ipi_send(target_core, EOS_IPI_RESCHEDULE, task_id);
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
    rproc_mbox[cfg->id].len = 0;
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
    if (rproc_id >= MAX_RPROC || !rprocs[rproc_id].configured) return -1;
    if (rprocs[rproc_id].state != EOS_RPROC_RUNNING) return -1;
    if (!data || len == 0 || len > RPROC_MBOX_SIZE) return -1;

    rproc_mbox_t *mbox = &rproc_mbox[rproc_id];

    /* Write data to shared memory mailbox */
    memcpy((void *)mbox->data, data, len);
    eos_dmb();
    __atomic_store_n(&mbox->len, (uint32_t)len, __ATOMIC_RELEASE);
    eos_dsb();

    /* Send IPI notification to the remote processor */
    eos_ipi_send(rproc_id, EOS_IPI_USER, (uint32_t)len);

    return 0;
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
#if defined(__aarch64__)
    __asm__ volatile("dmb sy" ::: "memory");
#elif defined(__arm__)
    __asm__ volatile("dmb" ::: "memory");
#elif defined(__x86_64__) || defined(__i386__)
    __asm__ volatile("mfence" ::: "memory");
#elif defined(__riscv)
    __asm__ volatile("fence rw, rw" ::: "memory");
#elif defined(_MSC_VER)
    _ReadWriteBarrier();
    MemoryBarrier();
#else
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
}

void eos_dsb(void)
{
#if defined(__aarch64__)
    __asm__ volatile("dsb sy" ::: "memory");
#elif defined(__arm__)
    __asm__ volatile("dsb" ::: "memory");
#elif defined(__x86_64__) || defined(__i386__)
    __asm__ volatile("mfence" ::: "memory");
#elif defined(__riscv)
    __asm__ volatile("fence iorw, iorw" ::: "memory");
#elif defined(_MSC_VER)
    _ReadWriteBarrier();
    MemoryBarrier();
#else
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
}

void eos_isb(void)
{
#if defined(__aarch64__)
    __asm__ volatile("isb" ::: "memory");
#elif defined(__arm__)
    __asm__ volatile("isb" ::: "memory");
#elif defined(__x86_64__) || defined(__i386__)
    /* x86 serializes instruction fetch on control-flow changes;
     * cpuid is the closest equivalent if needed. */
    __asm__ volatile("" ::: "memory");
#elif defined(__riscv)
    __asm__ volatile("fence.i" ::: "memory");
#elif defined(_MSC_VER)
    _ReadWriteBarrier();
#else
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
}

/* ---- Atomics ---- */

int32_t eos_atomic_add(volatile int32_t *ptr, int32_t val)
{
    if (!ptr) return 0;
    return __atomic_fetch_add(ptr, val, __ATOMIC_SEQ_CST);
}

int32_t eos_atomic_sub(volatile int32_t *ptr, int32_t val)
{
    if (!ptr) return 0;
    return __atomic_fetch_sub(ptr, val, __ATOMIC_SEQ_CST);
}

int32_t eos_atomic_cas(volatile int32_t *ptr, int32_t expected, int32_t desired)
{
    if (!ptr) return 0;
    int32_t old = expected;
    __atomic_compare_exchange_n(ptr, &old, desired, false,
                                __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old;
}

int32_t eos_atomic_load(volatile const int32_t *ptr)
{
    if (!ptr) return 0;
    return __atomic_load_n((const int32_t *)ptr, __ATOMIC_SEQ_CST);
}

void eos_atomic_store(volatile int32_t *ptr, int32_t val)
{
    if (!ptr) return;
    __atomic_store_n(ptr, val, __ATOMIC_SEQ_CST);
}

#endif /* EOS_ENABLE_MULTICORE */
