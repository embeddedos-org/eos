# Chapter 7: Multicore

*Srikanth Patchava & EmbeddedOS Contributors*

---

## 7.1 Overview

Modern embedded SoCs frequently include multiple processor cores. EoS provides a
comprehensive multicore framework supporting both **SMP** (Symmetric Multi-Processing)
and **AMP** (Asymmetric Multi-Processing) topologies.

The multicore API (`multicore.h`) provides:

- **Core management** — start, stop, and query cores
- **Spinlocks** — low-level multi-core synchronization
- **Inter-Processor Interrupts (IPI)** — cross-core signaling
- **Shared memory** — named memory regions with cache management
- **Task core affinity** — bind tasks to specific cores
- **Remote processor control** — manage heterogeneous co-processors
- **Memory barriers and atomics** — architecture-portable primitives

All multicore functionality is conditionally compiled under `EOS_ENABLE_MULTICORE`.

### Key Constants

| Macro | Value | Description |
|-------|-------|-------------|
| `EOS_MAX_CORES` | 16 | Maximum supported cores |
| `EOS_MAX_IPC_CHANNELS` | 8 | Maximum IPC channels |

## 7.2 SMP vs. AMP

| Feature | SMP | AMP |
|---------|-----|-----|
| OS image | Same on all cores | Different per core |
| Memory | Shared address space | May be separate |
| Scheduling | Global scheduler | Per-core schedulers |
| Use case | Linux-class SoCs | Heterogeneous (A-core + M-core) |
| EoS mode | `EOS_MP_SMP` | `EOS_MP_AMP` |

```c
// Initialize for SMP
eos_multicore_init(EOS_MP_SMP);

// Initialize for AMP (e.g., STM32MP1: Cortex-A7 + Cortex-M4)
eos_multicore_init(EOS_MP_AMP);
```

## 7.3 Core Management

### Core States

```
    ┌──────────┐    start()    ┌──────────┐
    │ OFFLINE  │──────────────→│  ONLINE  │
    └──────────┘               └────┬─────┘
         ▲                          │
         │ stop()                   │ sleep
         │                     ┌────▼─────┐
         └─────────────────────│ SLEEPING │
                               └──────────┘
```

| State | Value | Description |
|-------|-------|-------------|
| `EOS_CORE_OFFLINE` | 0 | Core powered down or not started |
| `EOS_CORE_ONLINE` | 1 | Core executing code |
| `EOS_CORE_SLEEPING` | 2 | Core in low-power sleep |
| `EOS_CORE_HALTED` | 3 | Core stopped due to error |

### Core API

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_multicore_init` | `int eos_multicore_init(eos_mp_mode_t mode)` | Initialize multicore subsystem |
| `eos_multicore_deinit` | `void eos_multicore_deinit(void)` | Shutdown multicore |
| `eos_core_count` | `uint8_t eos_core_count(void)` | Number of active cores |
| `eos_core_id` | `uint8_t eos_core_id(void)` | Current core ID |
| `eos_core_start` | `int eos_core_start(uint8_t id, eos_core_entry_fn entry, void *arg)` | Boot a secondary core |
| `eos_core_stop` | `int eos_core_stop(uint8_t id)` | Stop a core (cannot stop core 0) |
| `eos_core_get_info` | `int eos_core_get_info(uint8_t id, eos_core_info_t *info)` | Query core state |

### Example: Booting a Secondary Core

```c
void core1_main(void *arg)
{
    printf("Core %d is alive! arg=%p\n", eos_core_id(), arg);

    while (1) {
        // Core 1 workload
        eos_delay_ms(1000);
    }
}

int main(void)
{
    eos_hal_init();
    eos_multicore_init(EOS_MP_SMP);

    printf("Core 0 starting core 1...\n");
    eos_core_start(1, core1_main, NULL);

    printf("Cores online: %d\n", eos_core_count());

    while (1) {
        eos_delay_ms(1000);
    }
}
```

### Architecture-Specific Core ID Detection

The `eos_core_id()` function uses inline assembly for each architecture:

| Architecture | Instruction | Register |
|-------------|------------|----------|
| **AArch64** | `mrs x0, mpidr_el1` | MPIDR_EL1[7:0] |
| **ARMv7** | `mrc p15, 0, r0, c0, c0, 5` | MPIDR[1:0] |
| **x86_64** | `cpuid` (leaf 1) | EBX[31:24] (APIC ID) |
| **RISC-V** | `csrr a0, mhartid` | mhartid |

## 7.4 Spinlocks

Spinlocks are the lowest-level synchronization primitive for multi-core systems.
They busy-wait and should only be held for very short durations.

### Spinlock API

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_spin_init` | `void eos_spin_init(eos_spinlock_t *lock)` | Initialize spinlock |
| `eos_spin_lock` | `void eos_spin_lock(eos_spinlock_t *lock)` | Acquire (busy-wait) |
| `eos_spin_trylock` | `bool eos_spin_trylock(eos_spinlock_t *lock)` | Try acquire (non-blocking) |
| `eos_spin_unlock` | `void eos_spin_unlock(eos_spinlock_t *lock)` | Release |
| `eos_spin_lock_irqsave` | `void eos_spin_lock_irqsave(eos_spinlock_t *lock, uint32_t *flags)` | Acquire + disable IRQs |
| `eos_spin_unlock_irqrestore` | `void eos_spin_unlock_irqrestore(eos_spinlock_t *lock, uint32_t flags)` | Release + restore IRQs |

### Architecture-Specific Spin Hints

The spinlock implementation uses architecture-specific wait hints to reduce power
consumption during contention:

| Architecture | Lock Wait Hint | Unlock Wake Hint |
|-------------|---------------|-----------------|
| **ARM/AArch64** | `wfe` (Wait For Event) | `sev` (Send Event) |
| **x86** | `pause` | (none needed) |
| **RISC-V** | (spin only) | (none) |

### Implementation Detail

```c
void eos_spin_lock(eos_spinlock_t *lock)
{
    while (__atomic_exchange_n(lock, 1, __ATOMIC_ACQUIRE)) {
#if defined(__aarch64__) || defined(__arm__)
        __asm__ volatile("wfe");    // Power-efficient wait
#elif defined(__x86_64__)
        __asm__ volatile("pause");  // Reduce pipeline stall
#endif
    }
}

void eos_spin_unlock(eos_spinlock_t *lock)
{
    __atomic_store_n(lock, 0, __ATOMIC_RELEASE);
#if defined(__aarch64__) || defined(__arm__)
    __asm__ volatile("sev");  // Wake waiting cores
#endif
}
```

### IRQ-Safe Spinlocks

When a spinlock protects data accessed from both task and interrupt context, use
the IRQ-safe variants:

```c
eos_spinlock_t data_lock = EOS_SPINLOCK_INIT;
uint32_t flags;

eos_spin_lock_irqsave(&data_lock, &flags);
// Critical section — interrupts disabled, lock held
shared_data++;
eos_spin_unlock_irqrestore(&data_lock, flags);
// Interrupts restored to previous state
```

The IRQ save/restore is also architecture-specific:

| Architecture | Save | Restore |
|-------------|------|---------|
| AArch64 | `mrs x0, daif; msr daifset, #0xF` | `msr daif, x0` |
| ARMv7 | `mrs r0, cpsr; cpsid if` | `msr cpsr_c, r0` |
| x86 | `pushf; pop; cli` | `sti` (if IF was set) |
| RISC-V | `csrr; csrc mstatus, 0x8` | `csrs mstatus, 0x8` |

## 7.5 Inter-Processor Interrupts (IPI)

IPIs allow one core to signal another for scheduling, function calls, or user-defined
events.

### IPI Types

| Type | Value | Purpose |
|------|-------|---------|
| `EOS_IPI_RESCHEDULE` | 0 | Trigger scheduler on target core |
| `EOS_IPI_CALL_FUNC` | 1 | Execute a function on target core |
| `EOS_IPI_STOP` | 2 | Request core shutdown |
| `EOS_IPI_USER` | 3 | User-defined event |

### IPI API

| Function | Description |
|----------|-------------|
| `eos_ipi_send(target, type, data)` | Send IPI to a specific core |
| `eos_ipi_broadcast(type, data)` | Send IPI to all other online cores |
| `eos_ipi_register_handler(type, handler, ctx)` | Register IPI handler |

### Example: Cross-Core Function Call

```c
void ipi_handler(uint8_t from_core, uint32_t data, void *ctx)
{
    printf("Core %d received IPI from core %d, data=%u\n",
           eos_core_id(), from_core, data);
}

int main(void)
{
    eos_multicore_init(EOS_MP_SMP);

    eos_ipi_register_handler(EOS_IPI_USER, ipi_handler, NULL);
    eos_core_start(1, core1_main, NULL);

    // Send user IPI to core 1
    eos_ipi_send(1, EOS_IPI_USER, 42);

    // Broadcast to all other cores
    eos_ipi_broadcast(EOS_IPI_RESCHEDULE, 0);
}
```

## 7.6 Shared Memory

Named shared memory regions enable data exchange between cores with explicit cache
management.

### Shared Memory API

| Function | Description |
|----------|-------------|
| `eos_shmem_create(cfg, region)` | Create a named shared region |
| `eos_shmem_open(name, region)` | Open an existing region by name |
| `eos_shmem_close(region)` | Close a region |
| `eos_shmem_flush(region)` | Clean + invalidate cache lines |
| `eos_shmem_invalidate(region)` | Invalidate cache lines |

### Example: Inter-Core Data Sharing

```c
// Core 0: Create shared buffer
uint8_t shared_buf[256] __attribute__((aligned(64)));

eos_shmem_config_t cfg = {
    .name   = "sensor_data",
    .base   = shared_buf,
    .size   = sizeof(shared_buf),
    .cached = true,
};
eos_shmem_region_t region;
eos_shmem_create(&cfg, &region);

// Write data and flush
memcpy(shared_buf, sensor_data, sizeof(sensor_data));
eos_shmem_flush(&region);  // Push to main memory

// Core 1: Open and read
eos_shmem_region_t remote;
eos_shmem_open("sensor_data", &remote);
eos_shmem_invalidate(&remote);  // Pull from main memory
memcpy(local_buf, remote.addr, remote.size);
```

### Cache Operations by Architecture

| Architecture | Flush (Clean+Invalidate) | Invalidate |
|-------------|------------------------|------------|
| AArch64 | `dc civac` per cache line + `dsb sy` | `dc ivac` + `dsb sy` |
| ARMv7 | `mcr p15 DCCIMVAC` + `dsb` | `mcr p15 DCIMVAC` + `dsb` |
| x86 | `clflush` + `mfence` | `clflush` + `mfence` |

## 7.7 Task Core Affinity

Bind tasks to specific cores using affinity masks:

```c
// Pin task to core 0 only
eos_task_set_affinity(task_id, EOS_CORE_MASK(0));

// Allow task on cores 0 and 1
eos_task_set_affinity(task_id, EOS_CORE_MASK(0) | EOS_CORE_MASK(1));

// Allow any core
eos_task_set_affinity(task_id, EOS_CORE_MASK_ALL);

// Migrate task to core 2
eos_task_migrate(task_id, 2);
```

### Affinity Macros

| Macro | Value | Meaning |
|-------|-------|---------|
| `EOS_CORE_MASK(c)` | `1U << c` | Single core mask |
| `EOS_CORE_MASK_ALL` | `0xFFFFFFFF` | Run on any core |
| `EOS_CORE_MASK_NONE` | `0x00000000` | No core (invalid) |

## 7.8 Remote Processor Control

For AMP systems with heterogeneous co-processors (e.g., Cortex-A + Cortex-M):

### Remote Processor API

| Function | Description |
|----------|-------------|
| `eos_rproc_init(cfg)` | Configure remote processor |
| `eos_rproc_start(id)` | Boot remote processor |
| `eos_rproc_stop(id)` | Stop remote processor |
| `eos_rproc_get_state(id)` | Query state (OFFLINE/RUNNING/CRASHED) |
| `eos_rproc_send(id, data, len)` | Send message via shared-memory mailbox |
| `eos_rproc_set_rx_callback(id, cb, ctx)` | Register receive callback |

### Example: AMP Communication (Cortex-A ↔ Cortex-M)

```c
// Main core (Cortex-A): Send command to M-core
eos_rproc_config_t m4_cfg = {
    .id            = 0,
    .name          = "cortex-m4",
    .firmware_addr = 0x10000000,
    .firmware_size = 64 * 1024,
};
eos_rproc_init(&m4_cfg);
eos_rproc_start(0);

// Send a command
uint8_t cmd[] = { 0x01, 0x42, 0x00, 0x10 };
eos_rproc_send(0, cmd, sizeof(cmd));

// Receive responses
void rx_handler(uint8_t rproc_id, const void *data, size_t len, void *ctx)
{
    printf("Received %zu bytes from rproc %d\n", len, rproc_id);
}
eos_rproc_set_rx_callback(0, rx_handler, NULL);
```

## 7.9 Memory Barriers and Atomics

### Memory Barriers

| Function | Instruction | Purpose |
|----------|-----------|---------|
| `eos_dmb()` | `dmb` / `mfence` / `fence rw,rw` | Data Memory Barrier |
| `eos_dsb()` | `dsb` / `mfence` / `fence iorw,iorw` | Data Synchronization Barrier |
| `eos_isb()` | `isb` / (nop) / `fence.i` | Instruction Synchronization Barrier |

### Barrier Implementation by Architecture

| Barrier | AArch64 | ARMv7 | x86 | RISC-V |
|---------|---------|-------|-----|--------|
| DMB | `dmb sy` | `dmb` | `mfence` | `fence rw, rw` |
| DSB | `dsb sy` | `dsb` | `mfence` | `fence iorw, iorw` |
| ISB | `isb` | `isb` | (compiler barrier) | `fence.i` |

### Atomic Operations

| Function | Description |
|----------|-------------|
| `eos_atomic_add(ptr, val)` | Atomic fetch-and-add |
| `eos_atomic_sub(ptr, val)` | Atomic fetch-and-subtract |
| `eos_atomic_cas(ptr, expected, desired)` | Compare-and-swap |
| `eos_atomic_load(ptr)` | Atomic load |
| `eos_atomic_store(ptr, val)` | Atomic store |

All atomics use `__ATOMIC_SEQ_CST` ordering for maximum safety.

### Example: Lock-Free Counter

```c
volatile int32_t request_count = 0;

void handle_request(void)
{
    int32_t old = eos_atomic_add(&request_count, 1);
    printf("Request #%d (total: %d)\n", old + 1,
           eos_atomic_load(&request_count));
}
```

## 7.10 Summary

| Feature | API | Use Case |
|---------|-----|----------|
| Core mgmt | `eos_core_start/stop` | Boot secondary cores |
| Spinlocks | `eos_spin_lock/unlock` | Short critical sections |
| IPI | `eos_ipi_send/broadcast` | Cross-core signaling |
| Shared mem | `eos_shmem_create/open` | Inter-core data exchange |
| Affinity | `eos_task_set_affinity` | Pin tasks to cores |
| Remote proc | `eos_rproc_init/start` | AMP co-processor control |
| Barriers | `eos_dmb/dsb/isb` | Memory ordering |
| Atomics | `eos_atomic_add/cas` | Lock-free data structures |

---

*Next: [Chapter 8 — Driver Framework](ch08-drivers.md)*
