# Multicore AMP — Dual-Core Application

Demonstrates asymmetric multiprocessing (AMP) with EoS. The primary core produces data into a spinlock-protected shared memory ring buffer, while the secondary core consumes it and sends IPI notifications back.

## What it demonstrates

- Multicore initialization in AMP mode with `eos_multicore_init(EOS_MP_AMP)`
- Starting a secondary core with `eos_core_start()`
- Shared memory creation with `eos_shmem_create()`
- Spinlock-protected ring buffer for cross-core data transfer
- Inter-Processor Interrupts with `eos_ipi_send()` / `eos_ipi_register_handler()`
- Memory barriers via spinlock internals

## Architecture

```
┌─────────────────┐            ┌─────────────────┐
│    Core 0       │            │    Core 1        │
│   (Primary)     │            │  (Secondary)     │
│                 │            │                  │
│  producer_task  │   shmem    │  secondary_main  │
│      │          │   ring     │       │          │
│      ▼          │   buffer   │       ▼          │
│  ringbuf_write ─┼───────────►│  ringbuf_read   │
│                 │  spinlock  │       │          │
│  ipi_handler ◄──┼────────────┼── eos_ipi_send  │
└─────────────────┘            └─────────────────┘
```

## Modules used

| Module | Header | Functions |
|--------|--------|-----------|
| Multicore | `eos/multicore.h` | `eos_multicore_init`, `eos_core_start`, `eos_core_count`, `eos_core_id` |
| Spinlock | `eos/multicore.h` | `eos_spin_init`, `eos_spin_lock`, `eos_spin_unlock` |
| IPI | `eos/multicore.h` | `eos_ipi_send`, `eos_ipi_register_handler` |
| Shared Memory | `eos/multicore.h` | `eos_shmem_create`, `eos_shmem_close` |
| Kernel | `eos/kernel.h` | `eos_kernel_init`, `eos_task_create` |

## How to build

```bash
cmake -B build -DEOS_PRODUCT=iot -DEOS_ENABLE_MULTICORE=1
cmake --build build
./build/multicore-amp
```

## Expected output

```
=== EoS Multicore AMP Demo ===
[core-0] Primary core running, 2 cores available
[core-0] Shared memory created: 4096 bytes at 0x...
[core-0] Starting core 1...
[core-0] Starting kernel...
[core-0] Producer task started
[core-1] Secondary core started
[core-0] Wrote value: 1
[core-1] Read value: 1 (processed=0)
[core-0] IPI received from core 1: data=0x00000001
[core-0] Wrote value: 2
[core-1] Read value: 2 (processed=1)
...
[core-0] Producer done
[core-1] Secondary core done (processed 20 messages)
=== Multicore demo complete ===
```
