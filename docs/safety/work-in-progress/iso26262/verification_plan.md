<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 EoS Project -->

# ISO 26262 — Verification Plan for EoS

## 1. Purpose

This document defines the verification strategy, test methods, coverage metrics, and test environment for EoS software verification per ISO 26262-6, Clauses 9–11.

> **Standard Reference:** ISO 26262:2018, Part 6, Clauses 9 (unit testing), 10 (integration testing), 11 (software verification)

## 2. Verification Methods per ASIL Level

### 2.1 ISO 26262-6 Table 7 — Methods for Software Unit Testing

| Method | ASIL A | ASIL B | ASIL C | ASIL D |
|--------|--------|--------|--------|--------|
| Requirements-based testing | ++ | ++ | ++ | ++ |
| Interface testing | + | ++ | ++ | ++ |
| Fault injection testing | o | + | + | ++ |
| Resource usage testing | + | + | + | ++ |
| Back-to-back testing | o | o | + | ++ |

> **++** = Highly recommended, **+** = Recommended, **o** = No recommendation

### 2.2 ISO 26262-6 Table 8 — Methods for Software Integration Testing

| Method | ASIL A | ASIL B | ASIL C | ASIL D |
|--------|--------|--------|--------|--------|
| Interface testing | ++ | ++ | ++ | ++ |
| Testing of external interfaces | + | + | ++ | ++ |
| Fault injection testing | o | + | + | ++ |
| Resource usage testing | + | + | + | ++ |

### 2.3 ISO 26262-6 Table 9 — Structural Coverage Metrics

| Coverage Metric | ASIL A | ASIL B | ASIL C | ASIL D | EoS Tool |
|-----------------|--------|--------|--------|--------|----------|
| Statement coverage | ++ | ++ | + | + | gcov / lcov |
| Branch coverage (decision) | + | ++ | ++ | ++ | gcov / lcov |
| MC/DC coverage | o | + | ++ | ++ | gcov + custom MC/DC analysis |
| Function coverage | ++ | ++ | ++ | ++ | gcov / lcov |
| Call coverage | + | + | ++ | ++ | gcov / lcov |

## 3. Coverage Targets

| ASIL Level | Statement | Branch | MC/DC | Function | Call |
|------------|-----------|--------|-------|----------|------|
| **ASIL D** | ≥ 100% | ≥ 100% | ≥ 100% | 100% | 100% |
| **ASIL C** | ≥ 95% | ≥ 95% | ≥ 90% | 100% | ≥ 95% |
| **ASIL B** | ≥ 90% | ≥ 90% | N/A | 100% | ≥ 90% |
| **ASIL A** | ≥ 80% | ≥ 80% | N/A | ≥ 95% | ≥ 80% |
| **QM** | ≥ 70% | N/A | N/A | ≥ 80% | N/A |

> Coverage below target requires documented justification in the safety case.

## 4. Test Levels

### 4.1 Unit Testing

| Attribute | Specification |
|-----------|--------------|
| **Scope** | Individual C functions and modules |
| **Framework** | CTest + Unity (or custom test harness) |
| **Isolation** | Stubs/mocks for hardware dependencies |
| **Coverage** | Per ASIL table above |
| **Execution** | Host-based (native compilation) + target-based (QEMU/hardware) |
| **Criteria** | All tests pass; coverage targets met; no memory leaks (Valgrind) |

### 4.2 Integration Testing

| Attribute | Specification |
|-----------|--------------|
| **Scope** | Inter-module interfaces (scheduler + MPU, IPC + tasks, WDT + tasks) |
| **Framework** | CTest + custom integration test harness |
| **Environment** | QEMU emulation and/or target hardware |
| **Focus** | Interface correctness, data flow integrity, timing behavior |
| **Criteria** | All interfaces verified; resource usage within bounds |

### 4.3 System Testing

| Attribute | Specification |
|-----------|--------------|
| **Scope** | Complete EoS system with representative application tasks |
| **Environment** | Target hardware (STM32F4 Discovery or equivalent) |
| **Focus** | Safety goal validation, timing under worst-case load, fault injection |
| **Criteria** | All safety goals validated; safe-state transitions correct |

## 5. Test Environment

### 5.1 Host-Based Testing

| Component | Tool/Version | Purpose |
|-----------|-------------|---------|
| Compiler | GCC arm-none-eabi 12.x / host GCC 12.x | Cross-compilation / native compilation |
| Build System | CMake 3.20+ | Build orchestration |
| Test Framework | CTest (CMake) | Test execution and reporting |
| Coverage | gcov + lcov | Statement, branch, function coverage |
| Memory Checker | Valgrind (memcheck) | Memory leak and error detection |
| Static Analysis | cppcheck 2.x | MISRA C:2012 compliance, defect detection |
| Emulator | QEMU ARM | Target-like execution without hardware |

### 5.2 Target-Based Testing

| Component | Tool/Version | Purpose |
|-----------|-------------|---------|
| Target Board | STM32F407 Discovery (or [TODO]) | Reference hardware platform |
| Debugger | OpenOCD + GDB | Flash, debug, trace |
| Logic Analyzer | Saleae Logic (or [TODO]) | Timing verification, ISR latency measurement |
| Oscilloscope | [TODO] | Interrupt latency, watchdog timing verification |

### 5.3 CI/CD Pipeline

```
┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐
│  Commit  │───▶│  Build   │───▶│  Test    │───▶│ Coverage │
│          │    │ (CMake)  │    │ (CTest)  │    │ (lcov)   │
└──────────┘    └──────────┘    └──────────┘    └────┬─────┘
                                                     │
                ┌──────────┐    ┌──────────┐    ┌────┴─────┐
                │  Report  │◀───│ Valgrind │◀───│ cppcheck │
                │          │    │          │    │ (MISRA)  │
                └──────────┘    └──────────┘    └──────────┘
```

## 6. Test Case Documentation Template

| Field | Description |
|-------|-------------|
| **TC-ID** | Unique test case identifier |
| **Requirement** | SR-ID(s) being verified |
| **ASIL** | Inherited ASIL level |
| **Preconditions** | System state before test |
| **Test Steps** | Sequence of actions |
| **Expected Result** | Pass/fail criteria |
| **Actual Result** | Filled in during execution |
| **Verdict** | Pass / Fail / Blocked |
| **Coverage Contribution** | Which code paths are exercised |

### Example Test Case

| Field | Value |
|-------|-------|
| **TC-ID** | TC-MPU-001 |
| **Requirement** | SR-010, SR-012 |
| **ASIL** | ASIL C |
| **Preconditions** | Two tasks created: Task-A (safety), Task-B (QM). Both running. |
| **Test Steps** | 1. From Task-B, attempt to read Task-A's data region address. |
| **Expected Result** | MemManage fault triggered; Task-B halted; Task-A continues executing. |
| **Actual Result** | [TODO] |
| **Verdict** | [TODO] |

## 7. Regression Testing

All safety-relevant test cases shall be included in the regression test suite and executed:
- On every commit to safety-relevant modules
- Before every release baseline
- After any toolchain update

## 8. Cross-References

| Document | Relationship |
|----------|-------------|
| [safety_requirements.md](safety_requirements.md) | Requirements being verified |
| [software_architecture.md](software_architecture.md) | Architecture under test |
| [tool_classification.md](tool_classification.md) | Tool qualification for test tools |
| [overview.md](overview.md) | ASIL levels determining coverage targets |

## 9. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1 | [TODO] | [TODO] | Initial draft |

---

> **[TODO]:** Add complete test case inventory for each safety requirement.
> **[TODO]:** Document MC/DC analysis tooling and methodology.
> **[TODO]:** Add target hardware test bench setup instructions.
