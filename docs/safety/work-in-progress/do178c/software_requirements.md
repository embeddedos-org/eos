<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 EoS Project -->

# DO-178C — Software Requirements for EoS

## 1. Purpose

This document provides High-Level Requirements (HLR) and Low-Level Requirements (LLR) templates for EoS software, along with traceability to system requirements, per DO-178C §5.1.

> **Standard Reference:** RTCA DO-178C, §5.1 — Software Requirements Process

## 2. Requirements Conventions

### 2.1 Identification Scheme

| Prefix | Level | Description |
|--------|-------|-------------|
| **SYS-** | System | System-level requirements (from integrator) |
| **HLR-** | High-Level | Software high-level requirements (what the software must do) |
| **LLR-** | Low-Level | Software low-level requirements (how the software does it) |
| **DER-** | Derived | Derived requirements not directly traceable to system requirements |

### 2.2 Requirement Attributes

| Attribute | Description |
|-----------|-------------|
| **ID** | Unique identifier (HLR-nnn or LLR-nnn) |
| **Title** | Short descriptive name |
| **Description** | Full requirement text using shall-language |
| **DAL** | Design Assurance Level (A–E) |
| **Traces To** | Parent requirement(s) |
| **Verification** | T (Test), A (Analysis), I (Inspection), R (Review) |
| **Status** | Draft / Reviewed / Approved / Deleted |

## 3. High-Level Requirements (HLR)

### 3.1 Scheduler HLR (DAL A)

| ID | Title | Description | Traces To | Verification |
|----|-------|-------------|-----------|-------------|
| HLR-SCHED-001 | Priority-Preemptive Scheduling | The scheduler shall dispatch the highest-priority ready task within a bounded and deterministic time. | SYS-[TODO] | T |
| HLR-SCHED-002 | Deadline Monitoring | The scheduler shall detect when a task exceeds its configured deadline and invoke the registered deadline-miss handler. | SYS-[TODO] | T |
| HLR-SCHED-003 | Priority Inheritance | The scheduler shall implement priority inheritance to prevent unbounded priority inversion. | SYS-[TODO] | T |
| HLR-SCHED-004 | Task States | The scheduler shall support task states: Ready, Running, Blocked, Suspended, and Terminated. | SYS-[TODO] | T, I |
| HLR-SCHED-005 | Idle Task | The scheduler shall execute a configurable idle task when no other task is ready. | SYS-[TODO] | T |

### 3.2 Memory Protection HLR (DAL A)

| ID | Title | Description | Traces To | Verification |
|----|-------|-------------|-----------|-------------|
| HLR-MPU-001 | Spatial Isolation | The MPU manager shall configure hardware memory protection to prevent any task from accessing memory outside its allocated regions. | SYS-[TODO] | T |
| HLR-MPU-002 | Kernel Protection | The MPU manager shall protect kernel code and data from modification by unprivileged tasks. | SYS-[TODO] | T |
| HLR-MPU-003 | Fault Containment | On memory access violation, the kernel shall terminate the offending task without affecting other tasks. | SYS-[TODO] | T |
| HLR-MPU-004 | Stack Guard | Each task stack shall be bounded by a guard region that triggers a fault on overflow. | SYS-[TODO] | T |

### 3.3 Watchdog HLR (DAL A)

| ID | Title | Description | Traces To | Verification |
|----|-------|-------------|-----------|-------------|
| HLR-WDT-001 | Per-Task Monitoring | The watchdog subsystem shall monitor each registered safety task for periodic liveness indication. | SYS-[TODO] | T |
| HLR-WDT-002 | Timeout Detection | The watchdog subsystem shall detect failure of any registered task to provide liveness indication within the configured timeout. | SYS-[TODO] | T |
| HLR-WDT-003 | Safe-State Transition | Upon detecting a task timeout, the watchdog subsystem shall initiate a system reset via the hardware watchdog. | SYS-[TODO] | T |

### 3.4 IPC HLR (DAL C)

| ID | Title | Description | Traces To | Verification |
|----|-------|-------------|-----------|-------------|
| HLR-IPC-001 | Message Passing | The IPC subsystem shall provide message passing between tasks with configurable message size and queue depth. | SYS-[TODO] | T |
| HLR-IPC-002 | Message Integrity | The IPC subsystem shall detect corruption of message data using CRC-32. | SYS-[TODO] | T |
| HLR-IPC-003 | Message Ordering | The IPC subsystem shall detect message loss or duplication using sequence numbers. | SYS-[TODO] | T |

## 4. Low-Level Requirements (LLR)

### 4.1 Scheduler LLR (DAL A)

| ID | Title | Description | Traces To | Verification |
|----|-------|-------------|-----------|-------------|
| LLR-SCHED-001 | Ready Queue Structure | The scheduler shall maintain a priority-indexed ready queue using a bitmap for O(1) highest-priority lookup. | HLR-SCHED-001 | T, I |
| LLR-SCHED-002 | Context Switch Sequence | On task switch, the scheduler shall: (1) save current task registers to TCB, (2) update MPU regions, (3) restore new task registers from TCB, (4) return to new task via exception return. | HLR-SCHED-001 | T, A |
| LLR-SCHED-003 | PendSV Handler | Context switches shall be triggered via the PendSV exception at the lowest priority to avoid nesting issues. | HLR-SCHED-001 | T, I |
| LLR-SCHED-004 | Deadline Timer | Each task with a configured deadline shall have a software timer that triggers the deadline-miss callback upon expiry. | HLR-SCHED-002 | T |
| LLR-SCHED-005 | Priority Inheritance Ceiling | When a lower-priority task holds a mutex needed by a higher-priority task, the lower-priority task's effective priority shall be raised to that of the blocked task. | HLR-SCHED-003 | T |

### 4.2 Memory Protection LLR (DAL A)

| ID | Title | Description | Traces To | Verification |
|----|-------|-------------|-----------|-------------|
| LLR-MPU-001 | Region Configuration | Each task's MPU regions shall be stored in the Task Control Block (TCB) and loaded during context switch. | HLR-MPU-001 | T, I |
| LLR-MPU-002 | Background Region | The MPU background region shall be configured as no-access to enforce explicit region configuration for all memory access. | HLR-MPU-001 | T |
| LLR-MPU-003 | MemManage Handler | The MemManage fault handler shall: (1) identify faulting task from PSP, (2) log MMFAR and MMFSR, (3) mark task as Terminated, (4) trigger rescheduling. | HLR-MPU-003 | T |
| LLR-MPU-004 | Guard Region Size | Stack guard regions shall be 32 bytes minimum, aligned to MPU region alignment constraints. | HLR-MPU-004 | T, A |

### 4.3 LLR Template (for additional requirements)

| ID | Title | Description | Traces To | Verification |
|----|-------|-------------|-----------|-------------|
| LLR-[MODULE]-[NNN] | [TODO] | The [component] shall [action] [constraint]. | HLR-[MODULE]-[NNN] | [T/A/I/R] |

## 5. Derived Requirements

Derived requirements are those identified during design that are not directly traceable to system requirements. They require safety assessment per DO-178C §5.1.2.

| ID | Title | Description | Rationale | DAL | Safety Assessment |
|----|-------|-------------|-----------|-----|-------------------|
| DER-001 | Interrupt Disable Bound | All critical sections in the kernel shall limit interrupt disable time to ≤5 µs. | Ensures bounded interrupt latency for safety ISRs. Design constraint not in system requirements. | DAL A | [TODO] — assess impact on safety |
| DER-002 | Static Allocation Only | The kernel shall not use dynamic memory allocation (malloc/free). | Deterministic memory usage. Design constraint. | DAL A | [TODO] |
| DER-003 | No Recursion | No kernel function shall use recursion. | Enables static stack analysis. Design constraint. | DAL A | [TODO] |

## 6. Traceability Matrix

### 6.1 System → HLR Traceability

| System Requirement | HLR |
|-------------------|-----|
| SYS-[TODO] | HLR-SCHED-001, HLR-SCHED-002, HLR-SCHED-003 |
| SYS-[TODO] | HLR-MPU-001, HLR-MPU-002, HLR-MPU-003, HLR-MPU-004 |
| SYS-[TODO] | HLR-WDT-001, HLR-WDT-002, HLR-WDT-003 |
| SYS-[TODO] | HLR-IPC-001, HLR-IPC-002, HLR-IPC-003 |

### 6.2 HLR → LLR Traceability

| HLR | LLR |
|-----|-----|
| HLR-SCHED-001 | LLR-SCHED-001, LLR-SCHED-002, LLR-SCHED-003 |
| HLR-SCHED-002 | LLR-SCHED-004 |
| HLR-SCHED-003 | LLR-SCHED-005 |
| HLR-MPU-001 | LLR-MPU-001, LLR-MPU-002 |
| HLR-MPU-003 | LLR-MPU-003 |
| HLR-MPU-004 | LLR-MPU-004 |

## 7. Cross-References

| Document | Relationship |
|----------|-------------|
| [software_design.md](software_design.md) | Design implementing these requirements |
| [verification_results.md](verification_results.md) | Test evidence for requirements verification |
| [plan_for_software_aspects.md](plan_for_software_aspects.md) | PSAC referencing requirements data |
| [../iso26262/safety_requirements.md](../iso26262/safety_requirements.md) | ISO 26262 safety requirements (complementary) |

## 8. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1 | [TODO] | [TODO] | Initial draft |

---

> **[TODO]:** Populate system requirement IDs from integrator.
> **[TODO]:** Complete LLR for watchdog, IPC, timer, and HAL modules.
> **[TODO]:** Conduct requirements review per SDP review process.
> **[TODO]:** Complete safety assessment for all derived requirements.
