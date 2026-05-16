<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 EoS Project -->

# IEC 61508 — SIL Technique/Measure Checklist for EoS

## 1. Purpose

This document provides a checklist of techniques and measures recommended by IEC 61508-3 for software development at each Safety Integrity Level (SIL 1–4). Each entry indicates EoS project compliance status.

> **Standard Reference:** IEC 61508:2010, Part 3, Tables A.1–A.10

## 2. Legend

| Symbol | Meaning |
|--------|---------|
| **HR** | Highly Recommended for this SIL |
| **R** | Recommended for this SIL |
| **—** | No recommendation for this SIL |
| **NR** | Not Recommended (alternative preferred) |

### EoS Status Values

| Status | Meaning |
|--------|---------|
| ✅ Implemented | Technique fully implemented in EoS |
| 🔶 Partial | Partially implemented; gaps documented |
| 🔲 Planned | Planned for implementation |
| ❌ N/A | Not applicable to EoS |

## 3. Table A.1 — Software Safety Requirements Specification

| # | Technique/Measure | SIL 1 | SIL 2 | SIL 3 | SIL 4 | EoS Status |
|---|------------------|-------|-------|-------|-------|------------|
| 1 | Computer-aided specification tools | R | R | HR | HR | 🔲 Planned |
| 2 | Semi-formal methods | R | HR | HR | HR | ✅ Implemented — structured requirements in Markdown with traceability |
| 3 | Formal methods | — | R | R | HR | 🔲 Planned — for SIL 3/4 critical components |

## 4. Table A.2 — Software Design and Development: Software Architecture

| # | Technique/Measure | SIL 1 | SIL 2 | SIL 3 | SIL 4 | EoS Status |
|---|------------------|-------|-------|-------|-------|------------|
| 1 | Fault detection and diagnosis | R | R | HR | HR | ✅ Implemented — watchdog, MPU fault handler |
| 2 | Error detecting and correcting codes | — | R | R | HR | 🔶 Partial — CRC-32 on IPC; ECC RAM is HW-dependent |
| 3 | Failure assertion programming | R | R | HR | HR | ✅ Implemented — `eos_assert()` defensive checks |
| 4 | Graceful degradation | R | R | HR | HR | ✅ Implemented — task-level fault containment |
| 5 | Artificial intelligence — fault correction | — | — | — | — | ❌ N/A |
| 6 | Dynamic reconfiguration | — | R | R | R | 🔲 Planned |
| 7 | Modular approach | HR | HR | HR | HR | ✅ Implemented — kernel/HAL/driver separation |
| 8 | Use of well-tried components | HR | HR | HR | HR | ✅ Implemented — established algorithms (priority scheduler, CRC) |
| 9 | Forward recovery | — | R | R | R | 🔶 Partial — task restart supported |
| 10 | Backward recovery | — | R | R | R | 🔶 Partial — watchdog reset to known state |

## 5. Table A.3 — Software Design and Development: Support Tools

| # | Technique/Measure | SIL 1 | SIL 2 | SIL 3 | SIL 4 | EoS Status |
|---|------------------|-------|-------|-------|-------|------------|
| 1 | Suitable programming language (subset) | HR | HR | HR | HR | ✅ Implemented — C with MISRA C:2012 subset |
| 2 | Strongly typed programming language | HR | HR | HR | HR | 🔶 Partial — C with static analysis enforcement |
| 3 | Language subset | HR | HR | HR | HR | ✅ Implemented — MISRA C:2012 |
| 4 | Certified tools/translators | R | HR | HR | HR | 🔶 Partial — GCC qualified to TCL2 (see [../iso26262/tool_classification.md](../iso26262/tool_classification.md)) |

## 6. Table A.4 — Software Design and Development: Detailed Design

| # | Technique/Measure | SIL 1 | SIL 2 | SIL 3 | SIL 4 | EoS Status |
|---|------------------|-------|-------|-------|-------|------------|
| 1 | Structured programming | HR | HR | HR | HR | ✅ Implemented — enforced via MISRA C rules |
| 2 | Use of trusted/verified software elements | HR | HR | HR | HR | ✅ Implemented — kernel primitives formally specified |
| 3 | Defensive programming | R | HR | HR | HR | ✅ Implemented — parameter validation, assertions |
| 4 | Modular approach | HR | HR | HR | HR | ✅ Implemented |
| 5 | Design and coding standards | HR | HR | HR | HR | ✅ Implemented — MISRA C:2012, `.clang-format`, `.clang-tidy` |

## 7. Table A.5 — Software Design and Development: Code Implementation

| # | Technique/Measure | SIL 1 | SIL 2 | SIL 3 | SIL 4 | EoS Status |
|---|------------------|-------|-------|-------|-------|------------|
| 1 | No dynamic objects | HR | HR | HR | HR | ✅ Implemented — no `malloc`/`free` in kernel; static allocation only |
| 2 | No dynamic variables | R | R | HR | HR | ✅ Implemented — static task/stack allocation |
| 3 | Limited use of interrupts | R | R | HR | HR | ✅ Implemented — minimal ISR bodies; deferred processing |
| 4 | Limited use of pointers | R | R | HR | HR | 🔶 Partial — necessary for HW access; bounded by MISRA rules |
| 5 | Limited use of recursion | HR | HR | HR | HR | ✅ Implemented — no recursion in kernel code |

## 8. Table A.7 — Software Verification: Module Testing

| # | Technique/Measure | SIL 1 | SIL 2 | SIL 3 | SIL 4 | EoS Status |
|---|------------------|-------|-------|-------|-------|------------|
| 1 | Probabilistic/functional testing | HR | HR | HR | HR | ✅ Implemented — CTest unit tests |
| 2 | Test case execution from boundary value analysis | R | HR | HR | HR | ✅ Implemented |
| 3 | Test case execution from error guessing | R | R | HR | HR | 🔶 Partial |
| 4 | Performance testing | R | R | HR | HR | ✅ Implemented — timing measurement tests |
| 5 | Model-based testing | — | R | R | HR | 🔲 Planned |
| 6 | Interface testing | R | HR | HR | HR | ✅ Implemented |
| 7 | 100% statement coverage | R | HR | HR | HR | ✅ Implemented — gcov/lcov |
| 8 | 100% branch coverage | R | R | HR | HR | ✅ Implemented — gcov/lcov |
| 9 | 100% condition coverage | — | R | HR | HR | 🔶 Partial — MC/DC for critical modules |

## 9. Table A.9 — Software Verification: Integration Testing

| # | Technique/Measure | SIL 1 | SIL 2 | SIL 3 | SIL 4 | EoS Status |
|---|------------------|-------|-------|-------|-------|------------|
| 1 | Functional/black-box testing | HR | HR | HR | HR | ✅ Implemented |
| 2 | Performance testing | R | R | HR | HR | ✅ Implemented |
| 3 | Fault injection testing | — | R | HR | HR | 🔶 Partial — MPU and watchdog fault injection |
| 4 | Model-based testing | — | R | R | HR | 🔲 Planned |

## 10. Table A.10 — Functional Safety Assessment

| # | Technique/Measure | SIL 1 | SIL 2 | SIL 3 | SIL 4 | EoS Status |
|---|------------------|-------|-------|-------|-------|------------|
| 1 | Checklists | HR | HR | HR | HR | ✅ Implemented — this document |
| 2 | Decision/truth tables | — | R | HR | HR | 🔲 Planned |
| 3 | Failure analysis (FMEA) | R | R | HR | HR | 🔲 Planned |
| 4 | Cause consequence diagrams | — | R | R | HR | 🔲 Planned |

## 11. Compliance Summary

| SIL Level | HR Techniques Implemented | HR Techniques Total | Compliance Rate |
|-----------|--------------------------|--------------------|-----------------|
| SIL 1 | [TODO] | [TODO] | [TODO]% |
| SIL 2 | [TODO] | [TODO] | [TODO]% |
| SIL 3 | [TODO] | [TODO] | [TODO]% |
| SIL 4 | [TODO] | [TODO] | [TODO]% |

## 12. Cross-References

| Document | Relationship |
|----------|-------------|
| [overview.md](overview.md) | SIL determination and applicability |
| [software_safety_manual.md](software_safety_manual.md) | Proven-in-use argument, constraints |
| [validation_plan.md](validation_plan.md) | Validation testing approach |
| [../iso26262/tool_classification.md](../iso26262/tool_classification.md) | Tool qualification details |

## 13. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1 | [TODO] | [TODO] | Initial draft |

---

> **[TODO]:** Complete compliance rate calculation for each SIL level.
> **[TODO]:** Address all 🔲 Planned items before SIL 3 certification.
> **[TODO]:** Document rationale for any HR technique not implemented.
