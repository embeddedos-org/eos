<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 EoS Project -->

# IEC 61508 — Validation Plan for EoS

## 1. Purpose

This document defines the validation approach for EoS when used in IEC 61508 Safety Instrumented Systems, including statistical testing methodology, failure rate evidence templates, and validation test cases.

> **Standard Reference:** IEC 61508:2010, Part 3, Clause 7.7 (Software aspects of system safety validation); Part 7, Annex D

## 2. Validation Objectives

1. Demonstrate that EoS safety mechanisms achieve the claimed Systematic Capability (SC 3)
2. Provide failure rate evidence to support SIL claims
3. Validate that EoS behaves correctly under fault conditions
4. Verify all assumptions documented in the [software_safety_manual.md](software_safety_manual.md)

## 3. Statistical Testing Approach

### 3.1 Methodology

Per IEC 61508-7, Annex D, statistical testing provides quantitative evidence of software reliability. The approach uses random test case generation based on an operational profile.

### 3.2 Operational Profile

| Input Category | Distribution | Parameters | Rationale |
|---------------|-------------|------------|-----------|
| Task creation rate | Poisson | λ = 5 tasks/boot cycle | Typical embedded application |
| Task priority distribution | Uniform | [0, 31] | Full priority range |
| Inter-arrival time of events | Exponential | µ = 10 ms | Sensor polling frequency |
| Message size (IPC) | Uniform | [1, 256] bytes | Typical control messages |
| CPU load | Normal | µ = 60%, σ = 20% | Normal operating conditions |
| Interrupt frequency | Poisson | λ = 1000/sec | Typical for control systems |
| Fault injection rate | Exponential | µ = 3600 sec | One fault per hour average |

### 3.3 Statistical Test Confidence Calculation

For SIL 3 (continuous/high demand mode), the target PFH is < 10⁻⁷ per hour.

To demonstrate this with 99% confidence:

```
N ≥ -ln(1 - C) / PFH_target

Where:
  N = number of test hours
  C = confidence level (0.99)
  PFH_target = 10⁻⁷

N ≥ -ln(0.01) / 10⁻⁷ = 4.605 / 10⁻⁷ ≈ 4.6 × 10⁷ hours
```

This can be achieved through:
- Accelerated testing with compressed time scales
- Parallel test execution across multiple QEMU instances
- Combination with deterministic testing to reduce required statistical test hours

### 3.4 Test Execution Strategy

| Strategy | Test Hours | Method | Coverage |
|----------|-----------|--------|----------|
| Deterministic functional testing | [TODO] | CTest unit/integration tests | Requirements coverage |
| Fault injection testing | [TODO] | Automated fault injection on QEMU | Fault handler coverage |
| Stress testing | [TODO] | Maximum CPU load + interrupt storm | Worst-case scenarios |
| Endurance testing | [TODO] | 30-day continuous run on target HW | Long-term stability |
| Statistical random testing | [TODO] | Random operational profile on QEMU | Quantitative reliability |

## 4. Failure Rate Evidence Template

### 4.1 Systematic Failure Evidence

| Evidence Type | Source | Result | SIL Contribution |
|--------------|--------|--------|-------------------|
| Development process compliance | [sil_checklist.md](sil_checklist.md) | [TODO]% HR compliance | Systematic capability claim |
| Static analysis (MISRA C) | cppcheck reports | [TODO] violations, [TODO] resolved | Coding standard compliance |
| Unit test coverage | gcov/lcov reports | [TODO]% statement, [TODO]% branch | Verification completeness |
| Integration test results | CTest reports | [TODO]/[TODO] pass | Interface correctness |
| Code review records | Review logs | [TODO]% code reviewed | Defect removal |
| Formal verification | [TODO] | [TODO] | Critical algorithm correctness |

### 4.2 Random Failure Evidence (Hardware-Dependent)

| Evidence Type | Source | Result | SIL Contribution |
|--------------|--------|--------|-------------------|
| FMEDA results | HW vendor / analysis | [TODO] | Hardware failure rates |
| Diagnostic coverage | Safety mechanism analysis | See [software_safety_manual.md](software_safety_manual.md) | SFF calculation |
| Proof test interval | System integrator | [TODO] | PFDavg calculation |
| Common cause factor (β) | IEC 61508-6, Annex D | [TODO] | CCF contribution |

### 4.3 Failure Log Template

| Failure ID | Date | Test Phase | Description | Root Cause | Severity | Resolution | Status |
|-----------|------|-----------|-------------|-----------|----------|------------|--------|
| F-001 | [TODO] | [TODO] | [TODO] | [TODO] | [TODO] | [TODO] | [TODO] |

## 5. Validation Test Cases

### 5.1 Safety Mechanism Validation

| TC-ID | Test Case | Safety Mechanism | Expected Result | SIL |
|-------|-----------|-----------------|-----------------|-----|
| VT-001 | Inject buffer overflow in QM task; verify safety task unaffected | MPU Spatial Isolation | QM task halted; safety task continues with correct output | SIL 3 |
| VT-002 | Force safety task to exceed deadline; verify detection and handler invocation | Deadline Monitoring | Deadline handler called within 1 tick; event logged | SIL 3 |
| VT-003 | Halt safety task execution (simulate hang); verify watchdog reset | Software + HW Watchdog | System reset within 2× configured timeout | SIL 3 |
| VT-004 | Corrupt IPC message payload; verify CRC rejection | IPC CRC-32 Integrity | Message rejected; error reported to sender | SIL 2 |
| VT-005 | Drop IPC message; verify sequence number detection | IPC Sequence Numbers | Missing message detected within 1 control cycle | SIL 2 |
| VT-006 | Overflow task stack; verify guard region detection | Stack Overflow Detection | MemManage fault; task halted; others continue | SIL 3 |
| VT-007 | Inject stuck-at fault in RAM test area; verify BIST detection | Boot-Time RAM BIST | Boot halts with RAM error indication | SIL 2 |
| VT-008 | Create priority inversion scenario; verify priority inheritance | Priority Inheritance | Higher-priority task resumes within bounded time | SIL 3 |

### 5.2 Stress and Endurance Tests

| TC-ID | Test Case | Duration | Pass Criteria |
|-------|-----------|----------|---------------|
| VT-100 | 100% CPU load with all safety tasks active | 24 hours | All deadlines met; no watchdog resets |
| VT-101 | Maximum interrupt rate (10,000/sec) sustained | 8 hours | All ISR latencies < configured limit |
| VT-102 | Continuous IPC messaging at maximum rate | 24 hours | Zero message losses; zero CRC errors |
| VT-103 | Repeated fault injection and recovery cycles | 1000 cycles | Correct recovery every cycle; no resource leaks |
| VT-104 | Long-term endurance run (normal load) | 30 days | No degradation; tick counter integrity maintained |

## 6. Validation Environment

| Component | Specification |
|-----------|--------------|
| Target Hardware | [TODO] — reference platform per safety manual |
| QEMU Version | [TODO] — for accelerated testing |
| Test Harness | Custom validation framework + CTest |
| Measurement Equipment | Logic analyzer for timing validation |
| Fault Injection Tool | Software-based (MPU register manipulation, memory corruption) |

## 7. Cross-References

| Document | Relationship |
|----------|-------------|
| [overview.md](overview.md) | SIL targets for validation |
| [sil_checklist.md](sil_checklist.md) | Development process compliance evidence |
| [software_safety_manual.md](software_safety_manual.md) | Assumptions to validate |
| [../iso26262/verification_plan.md](../iso26262/verification_plan.md) | Unit/integration test methods |

## 8. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1 | [TODO] | [TODO] | Initial draft |

---

> **[TODO]:** Calculate actual statistical test hours required and plan execution timeline.
> **[TODO]:** Define QEMU-based accelerated test infrastructure.
> **[TODO]:** Complete failure log with test campaign results.
