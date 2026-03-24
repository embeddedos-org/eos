# Multitask RTOS — Kernel Usage Demo

Demonstrates the EoS RTOS kernel with 3 cooperating tasks, a mutex-protected shared counter, and a message queue for producer/consumer communication.

## What it demonstrates

- Creating multiple tasks with `eos_task_create()` at different priorities
- Message queue communication: `eos_queue_send()` / `eos_queue_receive()`
- Mutex-protected shared resource: `eos_mutex_lock()` / `eos_mutex_unlock()`
- Task delay with `eos_task_delay_ms()`
- Queue depth monitoring with `eos_queue_count()`

## Task architecture

```
┌──────────┐   message_t    ┌──────────┐
│ Producer ├───────────────►│ Consumer │
│ (pri=2)  │   via queue    │ (pri=3)  │
└────┬─────┘                └────┬─────┘
     │                           │
     │    ┌───────────┐          │
     └───►│  Mutex    │◄─────────┘
          │ g_counter │
          └─────┬─────┘
                │
          ┌─────▼─────┐
          │  Monitor  │
          │  (pri=1)  │
          └───────────┘
```

## Modules used

| Module | Header | Functions |
|--------|--------|-----------|
| Kernel | `eos/kernel.h` | `eos_kernel_init`, `eos_kernel_start`, `eos_task_create`, `eos_task_delay_ms` |
| Mutex | `eos/kernel.h` | `eos_mutex_create`, `eos_mutex_lock`, `eos_mutex_unlock` |
| Queue | `eos/kernel.h` | `eos_queue_create`, `eos_queue_send`, `eos_queue_receive`, `eos_queue_count` |
| HAL | `eos/hal.h` | `eos_hal_init`, `eos_get_tick_ms` |

## How to build

```bash
cmake -B build -DEOS_PRODUCT=iot
cmake --build build
./build/multitask-rtos
```

## Expected output

```
[main] Creating tasks...
[main] Starting kernel...
[producer] Started
[consumer] Started
[monitor] Started
[producer] Sent msg id=0 value=52
[consumer] Received msg id=0 value=52 ts=1 ms
[producer] Sent msg id=1 value=62
[consumer] Received msg id=1 value=62 ts=501 ms
[monitor] counter=4 queue_depth=0 uptime=2001 ms
...
```
