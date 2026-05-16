<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 EoS Project -->

# ISO 26262 — Safety Requirements for EoS

## 1. Purpose

This document specifies the functional and technical safety requirements for EoS, derived from the Hazard Analysis and Risk Assessment (HARA). Each requirement is traced to its originating safety goal and includes verification method and ASIL classification.

> **Standard Reference:** ISO 26262:2018, Part 6, Clause 6 — Software safety requirements

## 2. Requirements Format

Each requirement follows this structure:

| Field | Description |
|-------|-------------|
| **SR-ID** | Unique safety requirement identifier (SR-nnn) |
| **Requirement** | Normative requirement statement (shall-language) |
| **Rationale** | Justification and traceability to safety goal |
| **ASIL** | Inherited ASIL from parent safety goal |
| **Verification Method** | How compliance is demonstrated (T=Test, A=Analysis, I=Inspection, R=Review) |

## 3. Functional Safety Requirements

### 3.1 Scheduler Safety Requirements (from SG-001, ASIL D)

| SR-ID | Requirement | Rationale | ASIL | Verification |
|-------|-------------|-----------|------|-------------|
| SR-001 | The scheduler shall implement priority-preemptive scheduling ensuring no lower-priority task can block a higher-priority safety task for more than the configured preemption latency threshold. | Derived from SG-001; prevents deadline miss due to priority inversion. | ASIL D | T: Worst-case response time analysis; stress test under 100% CPU load |
| SR-002 | The scheduler shall support a minimum of 32 priority levels with at least 4 levels reserved exclusively for safety-critical tasks. | Derived from SG-001; ensures sufficient granularity for safety task prioritization. | ASIL D | T: Unit test verifying priority level count; I: Code inspection |
| SR-003 | The scheduler shall detect deadline overrun for any task configured with a deadline and invoke the registered deadline-miss handler within 1 tick period. | Derived from SG-001; enables corrective action on timing violations. | ASIL D | T: Inject deadline overrun; verify handler invocation timing |
| SR-004 | The scheduler shall implement priority inheritance protocol to prevent unbounded priority inversion when safety tasks contend for shared resources. | Derived from SG-001; mitigates priority inversion hazard. | ASIL D | T: Create priority inversion scenario; measure blocking time |
| SR-005 | The scheduler shall maintain a monotonically increasing tick counter that does not overflow within a continuous runtime of 49.7 days (32-bit millisecond counter). | Derived from SG-001; ensures time-base integrity for deadline tracking. | ASIL D | A: Static analysis of counter arithmetic; T: Overflow boundary test |

### 3.2 Memory Protection Requirements (from SG-002, ASIL C)

| SR-ID | Requirement | Rationale | ASIL | Verification |
|-------|-------------|-----------|------|-------------|
| SR-010 | The MPU shall configure at least one dedicated memory region per safety task, with read/write access restricted to the owning task only. | Derived from SG-002; enforces spatial isolation. | ASIL C | T: Attempt cross-task memory access; verify fault exception |
| SR-011 | The MPU shall mark kernel code memory as read-only and executable; kernel data as read-write and non-executable. | Derived from SG-002; prevents code injection and kernel code corruption. | ASIL C | T: Attempt write to code region; verify fault; A: MPU config review |
| SR-012 | On MPU access violation, the kernel shall halt the offending task, log the fault address and type, and continue execution of remaining safety tasks. | Derived from SG-002; contains fault propagation. | ASIL C | T: Trigger access violation; verify offending task halted, others continue |
| SR-013 | The MPU configuration shall be validated at boot time against the compiled memory layout; any mismatch shall prevent task execution. | Derived from SG-002; ensures MPU setup integrity. | ASIL C | T: Corrupt MPU config table; verify boot halts before task launch |

### 3.3 Watchdog Requirements (from SG-003, ASIL B)

| SR-ID | Requirement | Rationale | ASIL | Verification |
|-------|-------------|-----------|------|-------------|
| SR-020 | The watchdog driver shall require periodic servicing from each registered safety task within its configured timeout period. | Derived from SG-003; enables per-task liveness monitoring. | ASIL B | T: Register task; withhold kick; verify timeout detection |
| SR-021 | If any registered safety task fails to service the watchdog within its timeout, the watchdog shall trigger a system reset within 2× the timeout period. | Derived from SG-003; bounds the time in uncontrolled state. | ASIL B | T: Measure time from missed kick to reset assertion |
| SR-022 | The watchdog shall use a clock source independent of the main system clock to ensure detection of main clock failure. | Derived from SG-003; prevents common-cause failure. | ASIL B | A: Hardware design review; I: Clock source inspection |
| SR-023 | The watchdog timeout period shall be configurable per-task in the range of 10 ms to 10,000 ms with 1 ms granularity. | Derived from SG-003; accommodates varying task timing requirements. | ASIL B | T: Configure various timeouts; verify detection accuracy |

### 3.4 Interrupt Controller Requirements (from SG-004, ASIL B)

| SR-ID | Requirement | Rationale | ASIL | Verification |
|-------|-------------|-----------|------|-------------|
| SR-030 | The interrupt controller shall guarantee a worst-case interrupt latency of ≤10 µs for the highest-priority interrupt on the target platform. | Derived from SG-004; ensures sensor data freshness. | ASIL B | T: Measure ISR entry latency under maximum interrupt load |
| SR-031 | The interrupt controller shall support at least 8 priority levels for nested interrupt handling. | Derived from SG-004; prevents low-priority ISRs from blocking safety-critical ISRs. | ASIL B | T: Verify nested preemption with priority assignments |
| SR-032 | Disabling interrupts globally shall be limited to a maximum of 5 µs in any code path within the kernel. | Derived from SG-004; bounds worst-case latency contribution. | ASIL B | A: Static timing analysis of critical sections; T: Runtime measurement |

### 3.5 IPC Requirements (from SG-005, ASIL B)

| SR-ID | Requirement | Rationale | ASIL | Verification |
|-------|-------------|-----------|------|-------------|
| SR-040 | The IPC subsystem shall provide message delivery confirmation (acknowledgment) for safety-tagged messages within the sender's timeout period. | Derived from SG-005; enables sender to detect delivery failure. | ASIL B | T: Send safety message; verify ACK within timeout |
| SR-041 | The IPC subsystem shall implement CRC-32 integrity checking on all safety-tagged messages and reject corrupted messages. | Derived from SG-005; detects message corruption. | ASIL B | T: Inject bit-flip in message payload; verify rejection |
| SR-042 | The IPC subsystem shall implement message sequence numbering to detect message loss or duplication for safety-tagged channels. | Derived from SG-005; detects message loss per SG-005. | ASIL B | T: Drop/duplicate message; verify sequence error detection |

## 4. Technical Safety Requirements

| SR-ID | Requirement | Rationale | ASIL | Verification |
|-------|-------------|-----------|------|-------------|
| SR-100 | All safety-relevant source code shall comply with MISRA C:2012, with documented deviations for justified exceptions. | ISO 26262-6 Table 1; coding guidelines for ASIL B–D. | ASIL D | A: Static analysis (cppcheck MISRA addon); I: Deviation log review |
| SR-101 | All safety-relevant functions shall have a cyclomatic complexity ≤15. | Reduces defect probability per ISO 26262-6 Table 1. | ASIL C | A: Static analysis metric extraction |
| SR-102 | Stack usage for each task shall be statically analyzed, and runtime stack overflow detection (via MPU guard region or stack canary) shall be enabled. | Prevents stack corruption from propagating to adjacent memory. | ASIL C | A: Static stack analysis; T: Stack overflow injection test |
| SR-103 | The boot sequence shall execute built-in self-test (BIST) for RAM integrity (march C- or equivalent) before initializing safety tasks. | Detects RAM faults before safety operation begins. | ASIL B | T: Inject stuck-at fault in RAM test area; verify detection |
| SR-104 | All safety-relevant configuration parameters shall be protected by CRC-32 and verified before use. | Detects configuration data corruption. | ASIL B | T: Corrupt config CRC; verify rejection at boot |

## 5. Traceability Matrix

| Safety Goal | Safety Requirements |
|-------------|-------------------|
| SG-001 (Scheduler deadline) | SR-001, SR-002, SR-003, SR-004, SR-005 |
| SG-002 (Memory protection) | SR-010, SR-011, SR-012, SR-013 |
| SG-003 (Watchdog detection) | SR-020, SR-021, SR-022, SR-023 |
| SG-004 (Interrupt latency) | SR-030, SR-031, SR-032 |
| SG-005 (IPC integrity) | SR-040, SR-041, SR-042 |
| General technical | SR-100, SR-101, SR-102, SR-103, SR-104 |

## 6. Cross-References

| Document | Relationship |
|----------|-------------|
| [hazard_analysis.md](hazard_analysis.md) | Safety goals from which these requirements are derived |
| [software_architecture.md](software_architecture.md) | Architecture implementing these requirements |
| [verification_plan.md](verification_plan.md) | Verification methods and coverage targets |
| [overview.md](overview.md) | ASIL classification context |

## 7. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1 | [TODO] | [TODO] | Initial draft |

---

> **[TODO]:** Add requirements for OTA update safety, boot sequence safety, and communication driver safety.
> **[TODO]:** Complete bidirectional traceability to design and test artifacts.
> **[TODO]:** Conduct requirements review per safety plan milestone M3.
