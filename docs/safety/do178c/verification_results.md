<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 EoS Project -->

# DO-178C — Verification Results for EoS

## 1. Purpose

This document provides the template for reporting software verification results, including test coverage evidence (MC/DC, decision, statement coverage) per DO-178C §6.

> **Standard Reference:** RTCA DO-178C, §6 — Software Verification Process; §11.14 — Software Verification Results

## 2. Coverage Requirements by DAL

| Coverage Metric | DAL A | DAL B | DAL C | DAL D |
|-----------------|-------|-------|-------|-------|
| **Statement Coverage** | Required | Required | Required | — |
| **Decision Coverage** | Required | Required | — | — |
| **MC/DC Coverage** | Required | — | — | — |
| **Data Coupling / Control Coupling** | Required | Required | — | — |

## 3. Statement Coverage Report Template

### 3.1 Summary

| Module | DAL | Total Statements | Covered | Not Covered | Coverage % | Target | Status |
|--------|-----|-----------------|---------|-------------|------------|--------|--------|
| `kernel/sched/` | A | [TODO] | [TODO] | [TODO] | [TODO]% | 100% | [TODO] |
| `kernel/mpu/` | A | [TODO] | [TODO] | [TODO] | [TODO]% | 100% | [TODO] |
| `drivers/watchdog/` | A | [TODO] | [TODO] | [TODO] | [TODO]% | 100% | [TODO] |
| `kernel/timer/` | B | [TODO] | [TODO] | [TODO] | [TODO]% | 100% | [TODO] |
| `kernel/irq/` | B | [TODO] | [TODO] | [TODO] | [TODO]% | 100% | [TODO] |
| `kernel/ipc/` | C | [TODO] | [TODO] | [TODO] | [TODO]% | 100% | [TODO] |
| `drivers/gpio/` | C | [TODO] | [TODO] | [TODO] | [TODO]% | 100% | [TODO] |
| `services/log/` | D | [TODO] | [TODO] | [TODO] | [TODO]% | N/A | [TODO] |
| **Total** | — | [TODO] | [TODO] | [TODO] | [TODO]% | — | — |

### 3.2 Uncovered Statements Justification

| Module | File:Line | Statement | Justification |
|--------|-----------|-----------|---------------|
| [TODO] | [TODO] | [TODO] | [TODO] — e.g., defensive code unreachable in normal operation; verified by analysis |

## 4. Decision (Branch) Coverage Report Template

### 4.1 Summary

| Module | DAL | Total Decisions | True Taken | False Taken | Coverage % | Target | Status |
|--------|-----|----------------|------------|-------------|------------|--------|--------|
| `kernel/sched/` | A | [TODO] | [TODO] | [TODO] | [TODO]% | 100% | [TODO] |
| `kernel/mpu/` | A | [TODO] | [TODO] | [TODO] | [TODO]% | 100% | [TODO] |
| `drivers/watchdog/` | A | [TODO] | [TODO] | [TODO] | [TODO]% | 100% | [TODO] |
| `kernel/timer/` | B | [TODO] | [TODO] | [TODO] | [TODO]% | 100% | [TODO] |
| `kernel/irq/` | B | [TODO] | [TODO] | [TODO] | [TODO]% | 100% | [TODO] |
| **Total** | — | [TODO] | [TODO] | [TODO] | [TODO]% | — | — |

### 4.2 Uncovered Decisions Justification

| Module | File:Line | Decision | Justification |
|--------|-----------|----------|---------------|
| [TODO] | [TODO] | [TODO] | [TODO] |

## 5. MC/DC Coverage Report Template (DAL A Only)

### 5.1 MC/DC Methodology

Modified Condition/Decision Coverage requires that:
1. Every entry and exit point is invoked
2. Every decision takes each possible outcome
3. Each condition in a decision independently affects the decision outcome

### 5.2 Summary

| Module | DAL | Total Conditions | Independently Shown | Coverage % | Target | Status |
|--------|-----|-----------------|--------------------| -----------|--------|--------|
| `kernel/sched/` | A | [TODO] | [TODO] | [TODO]% | 100% | [TODO] |
| `kernel/mpu/` | A | [TODO] | [TODO] | [TODO]% | 100% | [TODO] |
| `drivers/watchdog/` | A | [TODO] | [TODO] | [TODO]% | 100% | [TODO] |
| **Total** | — | [TODO] | [TODO] | [TODO]% | — | — |

### 5.3 MC/DC Analysis Example

```
Decision: if (task->state == READY && task->priority < current->priority)

Conditions:
  A = (task->state == READY)
  B = (task->priority < current->priority)

Truth Table:
  Test | A | B | Decision | Independence
  T1   | T | T |    T     | A shown by T1/T3
  T2   | T | F |    F     | B shown by T1/T2
  T3   | F | T |    F     | A shown by T1/T3
  T4   | F | F |    F     | (redundant)

MC/DC achieved with tests T1, T2, T3 (minimum 3 of 4).
```

### 5.4 Uncovered Conditions Justification

| Module | File:Line | Condition | Justification |
|--------|-----------|-----------|---------------|
| [TODO] | [TODO] | [TODO] | [TODO] |

## 6. Data Coupling / Control Coupling Analysis (DAL A/B)

### 6.1 Data Coupling

| Module Pair | Shared Data | Type | Verified | Method |
|------------|------------|------|----------|--------|
| Scheduler → MPU Manager | `eos_tcb_t.mpu_config` | Parameter | [TODO] | T: Context switch test |
| Scheduler → Watchdog | `eos_tcb_t.wdt_sequence` | Parameter | [TODO] | T: Watchdog kick test |
| IPC → CRC | Message buffer | Parameter | [TODO] | T: Message integrity test |

### 6.2 Control Coupling

| Caller | Callee | Control Relationship | Verified | Method |
|--------|--------|---------------------|----------|--------|
| SysTick → PendSV | PendSV trigger | Flag (SCB->ICSR) | [TODO] | T: Preemption test |
| MemManage → Scheduler | Task termination + reschedule | Function call | [TODO] | T: Fault injection test |
| WDT check → HW WDT | Kick / no-kick | Conditional call | [TODO] | T: Timeout detection test |

## 7. Test Results Summary

### 7.1 Unit Test Results

| Test Suite | Total Tests | Passed | Failed | Blocked | Pass Rate |
|-----------|------------|--------|--------|---------|-----------|
| `test_scheduler` | [TODO] | [TODO] | [TODO] | [TODO] | [TODO]% |
| `test_mpu` | [TODO] | [TODO] | [TODO] | [TODO] | [TODO]% |
| `test_watchdog` | [TODO] | [TODO] | [TODO] | [TODO] | [TODO]% |
| `test_timer` | [TODO] | [TODO] | [TODO] | [TODO] | [TODO]% |
| `test_ipc` | [TODO] | [TODO] | [TODO] | [TODO] | [TODO]% |
| `test_irq` | [TODO] | [TODO] | [TODO] | [TODO] | [TODO]% |
| **Total** | [TODO] | [TODO] | [TODO] | [TODO] | [TODO]% |

### 7.2 Integration Test Results

| Test Suite | Total Tests | Passed | Failed | Blocked | Pass Rate |
|-----------|------------|--------|--------|---------|-----------|
| `test_sched_mpu_integration` | [TODO] | [TODO] | [TODO] | [TODO] | [TODO]% |
| `test_sched_wdt_integration` | [TODO] | [TODO] | [TODO] | [TODO] | [TODO]% |
| `test_ipc_integration` | [TODO] | [TODO] | [TODO] | [TODO] | [TODO]% |
| `test_fault_handling` | [TODO] | [TODO] | [TODO] | [TODO] | [TODO]% |
| **Total** | [TODO] | [TODO] | [TODO] | [TODO] | [TODO]% |

## 8. Coverage Tool Information

| Tool | Version | Coverage Type | Output Format | Qualified? |
|------|---------|--------------|---------------|------------|
| gcov | GCC 12.x | Statement, branch, function | gcov data files | Per [../iso26262/tool_classification.md](../iso26262/tool_classification.md) |
| lcov | 1.16+ | Report generation from gcov | HTML, text summary | N/A (reporting only) |
| Custom MC/DC tool | [TODO] | MC/DC analysis | [TODO] | [TODO] |

## 9. Problem Reports

| PR-ID | Test | Description | Severity | Resolution | Status |
|-------|------|-------------|----------|------------|--------|
| [TODO] | [TODO] | [TODO] | [TODO] | [TODO] | [TODO] |

## 10. Cross-References

| Document | Relationship |
|----------|-------------|
| [software_requirements.md](software_requirements.md) | Requirements verified by these tests |
| [software_design.md](software_design.md) | Design verified by structural coverage |
| [software_development_plan.md](software_development_plan.md) | Verification methods defined |
| [configuration_management.md](configuration_management.md) | Test baselines and artifacts |
| [../iso26262/verification_plan.md](../iso26262/verification_plan.md) | Coverage targets and methods |

## 11. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1 | [TODO] | [TODO] | Initial template |

---

> **[TODO]:** Populate all coverage data after test campaign execution.
> **[TODO]:** Document justification for any uncovered code.
> **[TODO]:** Complete MC/DC analysis for all DAL A modules.
> **[TODO]:** Archive coverage reports with release baseline.
