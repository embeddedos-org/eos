<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 EoS Project -->

# ISO 26262 — Software Architecture for Safety

## 1. Purpose

This document describes the EoS software architecture from a functional safety perspective, focusing on freedom from interference, memory partitioning, error containment, and watchdog integration as required by ISO 26262-6, Clause 7.

> **Standard Reference:** ISO 26262:2018, Part 6, Clause 7 — Software architectural design

## 2. Architectural Overview

```
┌──────────────────────────────────────────────────────────┐
│                   Application Tasks                      │
│  ┌──────────┐ ┌──────────┐ ┌────────┐ ┌──────────────┐  │
│  │ Safety A │ │ Safety B │ │ QM C   │ │ QM D         │  │
│  │ (ASIL D) │ │ (ASIL B) │ │        │ │              │  │
│  └────┬─────┘ └────┬─────┘ └───┬────┘ └────┬─────────┘  │
├───────┼─────────────┼───────────┼───────────┼────────────┤
│       │    EoS Kernel (Privileged Mode)     │            │
│  ┌────┴─────────────┴───────────┴───────────┴──────────┐ │
│  │               System Call Interface                  │ │
│  ├────────────┬─────────────┬─────────┬───────────────┤  │
│  │ Scheduler  │ MPU Manager │  IPC    │ Timer / WDT   │  │
│  │ (ASIL D)   │ (ASIL D)    │(ASIL C) │ (ASIL D)      │  │
│  ├────────────┴─────────────┴─────────┴───────────────┤  │
│  │            Hardware Abstraction Layer                │  │
│  │ ┌──────┐ ┌──────┐ ┌─────┐ ┌─────┐ ┌─────────────┐ │  │
│  │ │ GPIO │ │ UART │ │ SPI │ │ ADC │ │ HW Watchdog │ │  │
│  │ └──────┘ └──────┘ └─────┘ └─────┘ └─────────────┘ │  │
│  └────────────────────────────────────────────────────┘  │
├──────────────────────────────────────────────────────────┤
│                    Hardware (MCU)                         │
│  ┌──────┐ ┌──────┐ ┌───────┐ ┌──────────────────────┐   │
│  │ CPU  │ │ MPU  │ │ NVIC  │ │ Independent WDT      │   │
│  └──────┘ └──────┘ └───────┘ └──────────────────────┘   │
└──────────────────────────────────────────────────────────┘
```

## 3. Freedom from Interference

EoS achieves freedom from interference through three mechanisms per ISO 26262-6, Clause 7.4.4:

| Interference Type | Mechanism | EoS Implementation | Safety Requirement |
|-------------------|-----------|--------------------|--------------------|
| **Spatial** | Memory partitioning | MPU regions per task | SR-010, SR-011 |
| **Temporal** | Execution time budgeting | Deadline monitor + preemption | SR-001, SR-003 |
| **Communication** | Message integrity | CRC-32 + sequence numbers on IPC | SR-041, SR-042 |

### 3.1 Spatial Isolation (Memory Partitioning)

The MPU enforces memory access permissions per task. Each safety task receives dedicated MPU regions for its stack and data segments. No task can access another task's memory without an explicit shared-memory region.

### 3.2 Temporal Isolation (Scheduling Guarantees)

The priority-preemptive scheduler with deadline monitoring guarantees that:
- Higher-priority tasks always preempt lower-priority tasks
- Priority inheritance prevents unbounded priority inversion
- Deadline overruns are detected and reported within one tick

### 3.3 Communication Isolation (IPC Integrity)

All inter-task communication uses validated IPC channels with CRC-32 integrity checks and sequence numbering to detect corruption, loss, or duplication.

## 4. Memory Partitioning with MPU

### 4.1 Memory Layout

```
0xFFFF_FFFF ┌──────────────────────────────┐
            │ Peripheral Region            │ MPU: Device, no-exec
0x4000_0000 ├──────────────────────────────┤
            │ Kernel Code (Flash)          │ MPU: RO, exec, privileged
            ├──────────────────────────────┤
            │ Kernel Data (RAM)            │ MPU: RW, no-exec, privileged
            ├──────────────────────────────┤
            │ Safety Task A Stack + Guard  │ MPU: Task-A RW only
            │ Safety Task A Data (.bss)    │
            ├──────────────────────────────┤
            │ Safety Task B Stack + Guard  │ MPU: Task-B RW only
            │ Safety Task B Data (.bss)    │
            ├──────────────────────────────┤
            │ QM Task Pool (shared)        │ MPU: All QM tasks RW
            ├──────────────────────────────┤
            │ Vector Table (Flash)         │ MPU: RO, exec
0x0000_0000 └──────────────────────────────┘
```

### 4.2 MPU Region Assignment

| MPU Region | Assignment | Access | Attributes |
|------------|-----------|--------|------------|
| Region 0 | Flash — Kernel code | Privileged RO + Exec | Cacheable |
| Region 1 | RAM — Kernel data | Privileged RW, No-exec | Cacheable |
| Region 2 | Peripherals | Privileged RW, No-exec | Device, non-cacheable |
| Region 3 | Shared read-only data | All RO, No-exec | Cacheable |
| Region 4–5 | Safety Task A (stack + data) | Task-A RW, No-exec | Cacheable |
| Region 6–7 | Safety Task B (stack + data) | Task-B RW, No-exec | Cacheable |
| Background | Default deny | No access | Fault on any unmapped access |

### 4.3 MPU Context Switching

On each context switch the kernel reconfigures MPU regions 4–7 to match the incoming task's memory map:

1. Save current task's register context (including PSP)
2. Disable interrupts for MPU reconfiguration (bounded to <1 µs)
3. Write new MPU region base address and attribute registers for regions 4–7
4. Issue DSB + ISB barriers to ensure MPU update takes effect
5. Restore incoming task's register context and PSP
6. Re-enable interrupts

### 4.4 Stack Guard Regions

Each task stack is bounded by a 32-byte MPU guard region with no-access permissions. Stack overflow into the guard region triggers a MemManage fault, which the kernel handles by:

1. Halting the offending task
2. Logging stack pointer, fault address, and task ID
3. Invoking the registered error handler
4. Continuing execution of remaining tasks

## 5. Stack Isolation

| Property | Implementation |
|----------|---------------|
| **Per-task stacks** | Each task has a dedicated stack region sized at compile-time via linker script |
| **Guard regions** | 32-byte no-access MPU guard below each stack |
| **Stack usage analysis** | Static analysis with `-fstack-usage` (GCC); validated against allocated size |
| **Runtime detection** | MPU guard region triggers MemManage fault on overflow |
| **Stack canaries** | Optional `__stack_chk_guard` canary (GCC `-fstack-protector-strong`) as defense-in-depth |
| **Stack watermarking** | Debug builds fill stack with `0xDEADBEEF` pattern for usage measurement |

## 6. Error Containment

### 6.1 Error Containment Regions

```
┌─────────────────────────────────────────────────┐
│           Error Containment Region 1            │
│  Safety Tasks (ASIL B–D)                        │
│  - Isolated memory regions                      │
│  - Deadline monitoring                          │
│  - Watchdog supervision                         │
│  - Fault → halt task, log, notify               │
├─────────────────────────────────────────────────┤
│           Error Containment Region 2            │
│  QM Tasks                                       │
│  - Shared memory pool                           │
│  - Best-effort scheduling                       │
│  - Fault → halt task, log, continue             │
├─────────────────────────────────────────────────┤
│           Error Containment Region 3            │
│  Kernel (Privileged)                            │
│  - Self-test at boot (BIST)                     │
│  - Assertion-based defensive programming        │
│  - Fault → safe-state transition (system reset) │
└─────────────────────────────────────────────────┘
```

### 6.2 Fault Handling Strategy

| Fault Type | Source | Handler | Recovery Action |
|-----------|--------|---------|-----------------|
| MemManage | MPU access violation | `MemManage_Handler` | Halt offending task; log; continue others |
| BusFault | Invalid bus access | `BusFault_Handler` | Halt offending task; log; continue others |
| UsageFault | Undefined instruction, divide-by-zero | `UsageFault_Handler` | Halt offending task; log; continue others |
| HardFault | Escalated fault | `HardFault_Handler` | Log fault info to persistent storage; system reset |
| Deadline Overrun | Scheduler deadline monitor | `deadline_miss_handler` | Invoke registered callback; log; optional task restart |
| Watchdog Timeout | HW watchdog expiry | Hardware reset | System reset; boot into safe mode |
| Stack Overflow | MPU guard region hit | `MemManage_Handler` | Halt offending task; log; continue others |
| Assertion Failure | Kernel defensive check | `eos_panic()` | Log; safe-state transition; system reset |

### 6.3 Safe-State Definition

The safe state for EoS depends on the system integrator's definition. EoS provides the following safe-state transition mechanisms:

1. **Task-level safe state:** Halt individual task, continue others
2. **System-level safe state:** Controlled shutdown sequence → hardware watchdog reset
3. **Persistent logging:** Fault information written to non-volatile storage before reset

> **[TODO]:** Define system-specific safe states in collaboration with integrator.

## 7. Watchdog Integration

### 7.1 Architecture

```
┌──────────────────────────────────────────────────┐
│  Safety Task A    Safety Task B    QM Task C     │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐   │
│  │ wdt_kick │    │ wdt_kick │    │          │   │
│  │ (periodic)│    │ (periodic)│    │ (no WDT) │   │
│  └─────┬────┘    └─────┬────┘    └──────────┘   │
│        │               │                         │
│  ┌─────┴───────────────┴────────────────────┐    │
│  │        Software Watchdog Manager         │    │
│  │  - Per-task timeout tracking             │    │
│  │  - Sequence number validation            │    │
│  │  - All-tasks-alive → kick HW WDT        │    │
│  └─────────────────┬────────────────────────┘    │
│                    │                              │
├────────────────────┼──────────────────────────────┤
│  ┌─────────────────┴──────────────────────────┐  │
│  │     Hardware Watchdog (Independent Clock)  │  │
│  │  - Reset if not kicked within timeout      │  │
│  └────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────┘
```

### 7.2 Watchdog Supervision Protocol

1. Each safety task registers with the software watchdog manager at init, specifying its timeout
2. Each safety task must call `eos_wdt_kick(task_id, sequence_num)` within its timeout period
3. The software watchdog manager validates:
   - Kick received within timeout window
   - Sequence number is monotonically increasing (detects stuck-in-loop)
4. Only when **all** registered safety tasks have reported alive does the manager kick the hardware watchdog
5. If any safety task fails to report, the hardware watchdog expires and triggers system reset

### 7.3 Configuration

| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| `EOS_WDT_ENABLE` | `1` | `0`, `1` | Enable/disable watchdog subsystem |
| `EOS_WDT_HW_TIMEOUT_MS` | `500` | `100–10000` | Hardware watchdog timeout |
| `EOS_WDT_TASK_TIMEOUT_MS` | `200` | `10–10000` | Per-task software watchdog timeout |
| `EOS_WDT_MAX_TASKS` | `8` | `1–32` | Maximum number of supervised tasks |

## 8. Dependent Failure Analysis (DFA) Summary

| Common Cause | Affected Components | Mitigation |
|--------------|-------------------|------------|
| CPU failure | All software | Hardware watchdog with independent clock detects CPU hang |
| Clock failure | Scheduler, timer, WDT | Independent watchdog clock; clock monitoring unit |
| Power supply glitch | RAM, peripherals | Brown-out detection + reset; RAM BIST at boot |
| Shared memory corruption | All tasks | MPU isolation; tasks cannot access each other's memory |
| Compiler bug | All compiled code | Tool qualification (see [tool_classification.md](tool_classification.md)); diverse compilers for ASIL D |

## 9. Cross-References

| Document | Relationship |
|----------|-------------|
| [safety_requirements.md](safety_requirements.md) | Requirements implemented by this architecture |
| [hazard_analysis.md](hazard_analysis.md) | Hazards addressed by freedom from interference |
| [verification_plan.md](verification_plan.md) | Verification of architectural properties |
| [overview.md](overview.md) | ASIL classification of components |

## 10. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1 | [TODO] | [TODO] | Initial draft |

---

> **[TODO]:** Add platform-specific MPU register configuration details.
> **[TODO]:** Complete DFA with quantitative common-cause failure analysis.
> **[TODO]:** Document AUTOSAR-compatible error handling interfaces.
