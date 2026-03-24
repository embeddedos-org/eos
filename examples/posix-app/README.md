# POSIX App — POSIX Compatibility Demo

Demonstrates that standard POSIX APIs work on EoS. Creates pthreads, uses semaphores for synchronization, and passes messages through POSIX message queues — all running on the EoS kernel.

## What it demonstrates

- Thread creation with `eos_pthread_create()` / `eos_pthread_join()`
- Named semaphores with `eos_sem_init()` / `eos_sem_wait()` / `eos_sem_post()`
- POSIX message queues with `eos_mq_open()` / `eos_mq_send()` / `eos_mq_receive()`
- Sleep with `eos_sleep()`
- Writing portable code that works on both EoS and standard POSIX systems

## Modules used

| Module | Header | Functions |
|--------|--------|-----------|
| POSIX Threads | `eos/posix_threads.h` | `eos_pthread_create`, `eos_pthread_join` |
| POSIX Sync | `eos/posix_sync.h` | `eos_sem_init`, `eos_sem_wait`, `eos_sem_post`, `eos_sem_destroy` |
| POSIX IPC | `eos/posix_ipc.h` | `eos_mq_open`, `eos_mq_send`, `eos_mq_receive`, `eos_mq_close` |
| POSIX Time | `eos/posix_time.h` | `eos_sleep` |

## How to build

```bash
cmake -B build -DEOS_PRODUCT=iot
cmake --build build
./build/posix-app
```

## Expected output

```
=== EoS POSIX Compatibility Demo ===

[main] Threads created, waiting for completion...

[reader] Started
[writer-1] Started
[writer-2] Started
[writer-1] Sent: msg from thread 1, seq 0
[reader] Received (27 bytes): msg from thread 1, seq 0
[writer-2] Sent: msg from thread 2, seq 0
[reader] Received (27 bytes): msg from thread 2, seq 0
...
[writer-1] Done
[writer-2] Done
[reader] Done

=== POSIX demo complete ===
```

## Why POSIX compatibility?

Many embedded developers come from Linux backgrounds. EoS provides a POSIX compatibility layer so you can:
- Port existing Linux applications to EoS with minimal changes
- Use familiar APIs (pthreads, semaphores, message queues)
- Write code that compiles on both Linux and EoS targets
