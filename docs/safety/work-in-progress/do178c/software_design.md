<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 EoS Project -->

# DO-178C — Software Design Description for EoS

## 1. Purpose

This document describes the EoS software architecture per DO-178C §5.2, including data flow, control flow, and module interfaces for each software component.

> **Standard Reference:** RTCA DO-178C, §5.2 — Software Design Process

## 2. Architectural Layers

```
┌─────────────────────────────────────────────┐
│              Application Layer              │  User tasks (safety + QM)
├─────────────────────────────────────────────┤
│          System Call Interface (SVC)        │  Privilege boundary
├─────────────────────────────────────────────┤
│              Kernel Services                │
│  ┌─────────┐ ┌──────┐ ┌─────┐ ┌─────────┐  │
│  │Scheduler│ │ MPU  │ │ IPC │ │Timer/WDT│  │
│  └─────────┘ └──────┘ └─────┘ └─────────┘  │
├─────────────────────────────────────────────┤
│        Hardware Abstraction Layer           │
│  ┌────┐ ┌────┐ ┌────┐ ┌────┐ ┌──────────┐  │
│  │GPIO│ │UART│ │SPI │ │ADC │ │HW WDT   │  │
│  └────┘ └────┘ └────┘ └────┘ └──────────┘  │
├─────────────────────────────────────────────┤
│        Board Support Package (BSP)          │
│  Startup, linker script, clock config       │
└─────────────────────────────────────────────┘
```

## 3. Module Descriptions

### 3.1 Scheduler Module

| Attribute | Description |
|-----------|-------------|
| **Module** | `kernel/sched/` |
| **DAL** | DAL A |
| **Purpose** | Priority-preemptive task scheduling with deadline monitoring |
| **Implements** | HLR-SCHED-001 through HLR-SCHED-005 |

#### 3.1.1 Data Structures

```c
typedef struct eos_tcb {
    uint32_t *sp;                    /* Saved stack pointer */
    uint32_t priority;               /* Task priority (0 = highest) */
    uint32_t effective_priority;     /* Effective priority (for inheritance) */
    eos_task_state_t state;          /* Ready, Running, Blocked, etc. */
    uint32_t deadline_ticks;         /* Deadline in ticks (0 = none) */
    uint32_t deadline_remaining;     /* Ticks until deadline expiry */
    eos_mpu_config_t mpu_config;    /* Per-task MPU region config */
    uint32_t wdt_sequence;           /* Watchdog sequence counter */
    struct eos_tcb *next;            /* Next TCB in queue */
} eos_tcb_t;
```

#### 3.1.2 Control Flow — Task Dispatch

```
SysTick_Handler()
  │
  ├──► Increment system tick counter
  ├──► Update deadline timers for all active tasks
  │     └──► If deadline expired → call deadline_miss_handler()
  ├──► Check for ready tasks with higher priority than current
  │     └──► If yes → set PendSV pending
  └──► Return from ISR

PendSV_Handler()
  │
  ├──► Save current task context (R4–R11, LR) to current TCB
  ├──► Call eos_sched_get_next() → returns highest-priority ready TCB
  ├──► Update MPU regions for new task (eos_mpu_switch())
  ├──► Restore new task context (R4–R11, LR) from new TCB
  └──► Exception return → new task resumes
```

#### 3.1.3 Data Flow

```
                    ┌─────────────┐
 Task Create ──────►│ Ready Queue │──────► Scheduler
 Task Resume ──────►│  (bitmap)   │       Dispatch
                    └─────────────┘          │
                          ▲                  ▼
                          │            ┌──────────┐
 Task Block ◄─────────────┤            │ Running  │
 Task Suspend ◄───────────┤            │  Task    │
 Task Terminate ◄─────────┘            └──────────┘
```

### 3.2 MPU Manager Module

| Attribute | Description |
|-----------|-------------|
| **Module** | `kernel/mpu/` |
| **DAL** | DAL A |
| **Purpose** | Configure and manage MPU for spatial isolation |
| **Implements** | HLR-MPU-001 through HLR-MPU-004 |

#### 3.2.1 Control Flow — MPU Context Switch

```
eos_mpu_switch(eos_tcb_t *new_task)
  │
  ├──► Disable interrupts (bounded: <1 µs)
  ├──► For each task-specific MPU region (4–7):
  │     ├──► Write MPU_RNR (region number register)
  │     ├──► Write MPU_RBAR (region base address)
  │     └──► Write MPU_RASR (region attribute and size)
  ├──► Execute DSB (data synchronization barrier)
  ├──► Execute ISB (instruction synchronization barrier)
  └──► Re-enable interrupts
```

#### 3.2.2 Data Flow

```
 Task TCB ──► mpu_config[] ──► MPU Registers ──► Hardware Enforcement
                                                       │
                                            Access Violation?
                                                  │
                                            MemManage_Handler()
                                                  │
                                          ┌───────┴────────┐
                                          │ Log fault info │
                                          │ Terminate task │
                                          │ Reschedule     │
                                          └────────────────┘
```

### 3.3 Watchdog Module

| Attribute | Description |
|-----------|-------------|
| **Module** | `drivers/watchdog/` |
| **DAL** | DAL A |
| **Purpose** | Per-task liveness monitoring with HW watchdog aggregation |
| **Implements** | HLR-WDT-001 through HLR-WDT-003 |

#### 3.3.1 Control Flow

```
eos_wdt_kick(task_id, sequence_num)
  │
  ├──► Validate task_id is registered
  ├──► Validate sequence_num > last_sequence[task_id]
  ├──► Update last_kick_time[task_id]
  ├──► Update last_sequence[task_id]
  └──► Check if ALL registered tasks have kicked:
        ├──► Yes → Kick hardware watchdog (IWDG_KR = 0xAAAA)
        └──► No  → Do nothing (HW WDT continues counting down)

eos_wdt_check() — called from SysTick
  │
  ├──► For each registered task:
  │     ├──► If (current_time - last_kick_time) > task_timeout:
  │     │     ├──► Log timeout event
  │     │     └──► Set timeout_detected = true
  │     └──► Continue
  └──► If timeout_detected:
        └──► Do NOT kick HW WDT → HW WDT will reset system
```

### 3.4 IPC Module

| Attribute | Description |
|-----------|-------------|
| **Module** | `kernel/ipc/` |
| **DAL** | DAL C |
| **Purpose** | Message passing with integrity checking |
| **Implements** | HLR-IPC-001 through HLR-IPC-003 |

#### 3.4.1 Data Flow

```
Sender Task                              Receiver Task
    │                                         ▲
    ▼                                         │
┌──────────┐                           ┌──────────┐
│ Compute  │                           │ Validate │
│ CRC-32   │                           │ CRC-32   │
│ Add SeqNo│                           │ Check Seq│
└────┬─────┘                           └────┬─────┘
     │                                      │
     ▼          ┌──────────────┐            │
     └─────────►│ Message Queue│────────────┘
                │ (ring buffer)│
                └──────────────┘
```

## 4. Interface Control

### 4.1 Public Kernel API

| Function | Parameters | Return | Description |
|----------|-----------|--------|-------------|
| `eos_task_create` | config, stack, entry | `eos_status_t` | Create a new task |
| `eos_task_delay` | ticks | `eos_status_t` | Delay current task |
| `eos_task_suspend` | task_id | `eos_status_t` | Suspend a task |
| `eos_task_resume` | task_id | `eos_status_t` | Resume a suspended task |
| `eos_mutex_lock` | mutex, timeout | `eos_status_t` | Lock mutex with priority inheritance |
| `eos_mutex_unlock` | mutex | `eos_status_t` | Unlock mutex |
| `eos_ipc_send` | channel, msg, len, timeout | `eos_status_t` | Send IPC message |
| `eos_ipc_receive` | channel, buf, len, timeout | `eos_status_t` | Receive IPC message |
| `eos_wdt_register` | task_id, timeout_ms | `eos_status_t` | Register task with watchdog |
| `eos_wdt_kick` | task_id, seq_num | `eos_status_t` | Kick watchdog for task |

### 4.2 Internal Kernel Interfaces

| Function | Caller | Callee | Description |
|----------|--------|--------|-------------|
| `eos_sched_get_next` | PendSV_Handler | Scheduler | Get highest-priority ready task |
| `eos_mpu_switch` | PendSV_Handler | MPU Manager | Reconfigure MPU for new task |
| `eos_fault_handle` | Fault handlers | Fault manager | Process and log fault |

## 5. Cross-References

| Document | Relationship |
|----------|-------------|
| [software_requirements.md](software_requirements.md) | Requirements implemented by this design |
| [verification_results.md](verification_results.md) | Verification of design implementation |
| [software_development_plan.md](software_development_plan.md) | Design standards applied |
| [../iso26262/software_architecture.md](../iso26262/software_architecture.md) | Safety-focused architecture view |

## 6. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1 | [TODO] | [TODO] | Initial draft |

---

> **[TODO]:** Add detailed design for timer, HAL, and boot modules.
> **[TODO]:** Add sequence diagrams for all inter-module interactions.
> **[TODO]:** Complete ICD with exact register-level data formats.
