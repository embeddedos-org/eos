<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 EoS Project -->

# IEC 61508 — Software Safety Manual for EoS

## 1. Purpose

This Software Safety Manual documents the assumptions, constraints, proven-in-use argument, and safety function allocation for using EoS in IEC 61508 compliant Safety Instrumented Systems (SIS).

> **Standard Reference:** IEC 61508:2010, Part 3, Clause 7.4.2.12

## 2. Scope

This manual accompanies every EoS release intended for use in SIL-rated applications. The system integrator must review this manual and verify all assumptions before deploying EoS in a safety-related system.

## 3. Assumptions of Use

The following assumptions must be satisfied by the system integrator for EoS to meet its claimed Systematic Capability:

### 3.1 Hardware Assumptions

| ID | Assumption | Rationale |
|----|-----------|-----------|
| AU-HW-001 | Target MCU includes a Memory Protection Unit (MPU) with ≥8 configurable regions | Required for spatial isolation (see [../iso26262/software_architecture.md](../iso26262/software_architecture.md)) |
| AU-HW-002 | An independent hardware watchdog timer (IWDG) with separate clock source is available | Required for temporal fault detection independent of system clock |
| AU-HW-003 | Brown-out detection (BOD) with system reset is enabled in hardware | Prevents operation with unstable power supply |
| AU-HW-004 | ECC or parity-protected RAM is used for SIL 2+ applications | Required for random hardware fault coverage of memory |
| AU-HW-005 | Clock monitoring unit (CMU) is available for SIL 3 applications | Detects clock drift or failure |

### 3.2 Integration Assumptions

| ID | Assumption | Rationale |
|----|-----------|-----------|
| AU-INT-001 | System integrator performs system-level hazard and risk analysis | EoS provides component-level safety only |
| AU-INT-002 | Application code running on EoS follows MISRA C:2012 or equivalent coding standard | Prevents application-level defects from compromising OS integrity |
| AU-INT-003 | Safety-critical tasks are assigned priorities higher than all QM tasks | Required for temporal isolation to function correctly |
| AU-INT-004 | Stack sizes for all tasks are determined through static analysis and include ≥25% margin | Prevents stack overflow |
| AU-INT-005 | Integrator validates EoS safe-state transitions match their system safe-state requirements | EoS provides generic safe-state mechanisms |

### 3.3 Operational Assumptions

| ID | Assumption | Rationale |
|----|-----------|-----------|
| AU-OP-001 | Periodic proof testing of safety functions is performed per the system's SIF requirements | Required for maintaining PFD claims |
| AU-OP-002 | No modifications to EoS source code are made without re-assessment of safety claims | Modifications may invalidate safety evidence |
| AU-OP-003 | EoS is compiled with the qualified toolchain version specified in the release notes | Compiler qualification is version-specific |

## 4. Constraints

### 4.1 Functional Constraints

| ID | Constraint | Impact |
|----|-----------|--------|
| CO-001 | Maximum number of concurrent tasks: 32 | Exceeding this limit is undefined behavior |
| CO-002 | Maximum number of watchdog-supervised tasks: 8 (configurable) | Additional tasks require configuration change |
| CO-003 | No dynamic memory allocation (malloc/free) in kernel or safety tasks | Static allocation only for determinism |
| CO-004 | Maximum interrupt disable time in kernel: 5 µs | Bounds worst-case interrupt latency |
| CO-005 | Minimum tick period: 1 ms | Finer granularity requires timer hardware verification |

### 4.2 Environmental Constraints

| ID | Constraint | Impact |
|----|-----------|--------|
| CO-ENV-001 | Operating temperature range per MCU datasheet | EoS does not add thermal margins |
| CO-ENV-002 | EMC immunity per application standard (e.g., IEC 61326) | Hardware design responsibility |
| CO-ENV-003 | Single-core operation only; SMP not supported | Multi-core requires separate safety analysis |

## 5. Proven-in-Use Argument

Per IEC 61508-2, Clause 7.4.7.6, a proven-in-use argument may supplement development lifecycle evidence.

### 5.1 Criteria for Proven-in-Use

| Criterion | Requirement | EoS Status |
|-----------|-------------|------------|
| Operating hours | ≥ 10⁶ component-hours for SIL 2; ≥ 10⁷ for SIL 3 | [TODO] — tracking deployment hours |
| Configuration match | Identical hardware, compiler version, and configuration | [TODO] — define reference configuration |
| Failure reporting | Systematic failure reporting and tracking | ✅ — GitHub Issues + safety defect tracking |
| Change management | No changes since proven-in-use evidence collection | Per release version |
| Environment match | Similar environmental conditions | [TODO] — define reference environment |

### 5.2 Operational History Log Template

| Deployment | Platform | EoS Version | Start Date | Operating Hours | Failures Reported |
|-----------|----------|-------------|------------|-----------------|-------------------|
| [TODO] | [TODO] | [TODO] | [TODO] | [TODO] | [TODO] |

## 6. Safety Function Allocation

### 6.1 EoS-Provided Safety Mechanisms

| Safety Mechanism | Description | SIL Claim | Diagnostic Coverage |
|-----------------|-------------|-----------|-------------------|
| MPU Spatial Isolation | Prevents cross-task memory access | SC 3 | 99% for spatial interference |
| Priority-Preemptive Scheduler | Deterministic task scheduling | SC 3 | N/A (systematic) |
| Deadline Monitoring | Detects task deadline overruns | SC 3 | 99% for temporal violations |
| Software Watchdog | Per-task liveness monitoring | SC 3 | 90% for task hang detection |
| Hardware Watchdog Kick | Aggregated liveness to HW WDT | SC 3 | 99% for system-level hang |
| IPC Integrity (CRC-32) | Message corruption detection | SC 2 | 99.99% for random bit errors |
| IPC Sequence Numbers | Message loss/duplication detection | SC 2 | 99% for message loss |
| Stack Overflow Detection | MPU guard region on task stacks | SC 3 | 99% for stack overflow |
| Boot-Time RAM BIST | March C- RAM test before task init | SC 2 | 90% for stuck-at RAM faults |

### 6.2 Integrator-Provided Safety Mechanisms

The following safety mechanisms must be provided by the system integrator:

| Safety Mechanism | Responsibility | Rationale |
|-----------------|---------------|-----------|
| Input validation of sensor data | Integrator | Application-specific range checks |
| Actuator monitoring / feedback | Integrator | Hardware-dependent |
| Safe-state definition | Integrator | Application-specific |
| Redundancy architecture | Integrator | System-level design decision |
| Communication protocol safety | Integrator | Protocol-specific (e.g., PROFIsafe) |
| Proof test procedures | Integrator | SIF-specific |

## 7. Cross-References

| Document | Relationship |
|----------|-------------|
| [overview.md](overview.md) | SIL applicability and determination |
| [sil_checklist.md](sil_checklist.md) | Technique compliance checklist |
| [validation_plan.md](validation_plan.md) | Validation testing approach |
| [../iso26262/software_architecture.md](../iso26262/software_architecture.md) | Architecture details |

## 8. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1 | [TODO] | [TODO] | Initial draft |

---

> **[TODO]:** Populate proven-in-use operational history data.
> **[TODO]:** Define reference hardware configuration for proven-in-use.
> **[TODO]:** Assign diagnostic coverage values based on FMEDA.
