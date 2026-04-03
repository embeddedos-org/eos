<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 EoS Project -->

# DO-178C — Software Development Plan (SDP)

## 1. Purpose

This Software Development Plan (SDP) defines the standards, methods, tools, and development environment for EoS software development in compliance with DO-178C §4.2.

> **Standard Reference:** RTCA DO-178C, §11.2 — Software Development Plan

## 2. Scope

This plan covers all EoS software components at DAL A through DAL D. DAL E components are excluded from safety-related development activities.

## 3. Development Standards

### 3.1 Requirements Standards

| Standard | Description |
|----------|-------------|
| Requirement format | "The [component] shall [action] [constraint]" — shall-language only |
| Unique identification | HLR-nnn (High-Level Requirements), LLR-nnn (Low-Level Requirements) |
| Traceability | Each LLR traces to ≥1 HLR; each HLR traces to ≥1 system requirement |
| Derived requirements | Marked with "[DERIVED]" tag; require safety assessment |
| Verification method | Each requirement specifies: T (Test), A (Analysis), I (Inspection), R (Review) |

### 3.2 Design Standards

| Standard | Description |
|----------|-------------|
| Architecture | Modular decomposition; kernel / HAL / driver / service layers |
| Coupling | Minimize inter-module coupling; document all interfaces |
| Cohesion | High cohesion within modules; single-responsibility principle |
| Data flow | All data flows documented in design description |
| Control flow | All control flows documented; no unintended recursion |

### 3.3 Coding Standards

| Standard | Description |
|----------|-------------|
| Language | C11 (ISO/IEC 9899:2011) |
| Coding standard | MISRA C:2012 (mandatory for DAL A–C; recommended for DAL D) |
| Formatter | `.clang-format` (project-specific configuration) |
| Static analysis | `.clang-tidy` + cppcheck with MISRA addon |
| Naming convention | `snake_case` for functions/variables; `UPPER_CASE` for macros/constants |
| File organization | One module per `.c`/`.h` pair; header guards via `#pragma once` or include guards |
| Comments | Doxygen-style `/** */` for API documentation |
| Complexity limit | Cyclomatic complexity ≤ 15 per function |
| Function length | ≤ 75 lines of executable code per function |
| MISRA deviations | Documented per-instance with rationale in deviation log |

## 4. Development Methods

### 4.1 Requirements Development

1. System requirements are allocated to EoS software by the integrator
2. High-Level Requirements (HLR) are derived from system/safety requirements
3. Low-Level Requirements (LLR) are derived from HLR during detailed design
4. All requirements are reviewed for correctness, completeness, and verifiability
5. Derived requirements identified during design receive safety assessment

### 4.2 Design Method

1. Top-down decomposition from software requirements to architecture to detailed design
2. Data flow and control flow documented for each module
3. Interface control documents (ICDs) for all inter-module interfaces
4. Defensive programming with parameter validation at module boundaries

### 4.3 Coding Method

1. Implement from approved LLR and detailed design
2. Apply coding standards (MISRA C:2012)
3. Developer self-review + independent peer review
4. Static analysis clean (zero warnings/errors) before review

## 5. Development Tools

| Tool | Version | Purpose | DO-330 TQL |
|------|---------|---------|-----------|
| GCC (arm-none-eabi) | 12.x | Cross-compiler | TQL-1 to TQL-4 (by DAL) |
| CMake | 3.20+ | Build system | N/A (output verifiable) |
| Git | 2.x | Version control | N/A (CM tool) |
| Doxygen | 1.9+ | API documentation generation | N/A (documentation) |
| cppcheck | 2.x | Static analysis | TQL-5 |
| Clang-Tidy | 15+ | Static analysis | TQL-5 |
| gcov / lcov | (GCC bundled) | Code coverage | TQL-4 to TQL-5 |
| Valgrind | 3.20+ | Dynamic memory analysis | TQL-5 |
| QEMU | 7.x | ARM emulation for testing | N/A (test environment) |

See [../iso26262/tool_classification.md](../iso26262/tool_classification.md) for detailed tool assessment.

## 6. Development Environment

### 6.1 Host Environment

| Component | Specification |
|-----------|--------------|
| Host OS | Linux (Ubuntu 22.04 LTS) or Windows 10/11 with WSL2 |
| IDE | VS Code with C/C++ extension, or any text editor |
| Build system | CMake 3.20+ with Ninja generator |
| CI/CD | GitHub Actions (see `.github/workflows/ci.yml`) |

### 6.2 Target Environment

| Component | Specification |
|-----------|--------------|
| Target MCU | ARM Cortex-M4F (STM32F407VGT6 reference) |
| Clock | 168 MHz (HSE + PLL) |
| RAM | 192 KB SRAM |
| Flash | 1 MB |
| Debug interface | SWD via ST-Link / J-Link |
| Emulator | QEMU (qemu-system-arm, machine: netduinoplus2 or [TODO]) |

## 7. Review Process

### 7.1 Review Types

| Review Type | Participants | Trigger | DAL Applicability |
|------------|-------------|---------|-------------------|
| Requirements Review | Author, reviewer, safety engineer | HLR/LLR baseline | DAL A–D |
| Design Review | Author, reviewer, architect | Design baseline | DAL A–C |
| Code Review | Author, independent reviewer | Pull request / commit | DAL A–D |
| Test Review | Author, independent reviewer | Test plan/results baseline | DAL A–D |
| Audit | QA, CM, safety | Major milestone | DAL A–D |

### 7.2 Independence Requirements

| DAL | Code Review | Test Review | Verification |
|-----|------------|-------------|-------------|
| DAL A | Independent person | Independent person | Independent person |
| DAL B | Independent person | Independent person | — |
| DAL C | — | — | — |
| DAL D | — | — | — |

## 8. Problem Reporting

All software problems (bugs, requirement ambiguities, design issues, test failures) shall be tracked in the issue tracker with:

- Unique problem report ID
- Description and reproduction steps
- Affected baseline/version
- Severity classification
- Resolution and verification
- Impact analysis on safety

## 9. Cross-References

| Document | Relationship |
|----------|-------------|
| [plan_for_software_aspects.md](plan_for_software_aspects.md) | PSAC referencing this SDP |
| [software_requirements.md](software_requirements.md) | Requirements developed per this plan |
| [software_design.md](software_design.md) | Design developed per this plan |
| [configuration_management.md](configuration_management.md) | CM procedures |
| [verification_results.md](verification_results.md) | Verification per this plan |

## 10. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1 | [TODO] | [TODO] | Initial draft |

---

> **[TODO]:** Pin exact tool versions for certification baseline.
> **[TODO]:** Create MISRA C deviation log template.
> **[TODO]:** Define review checklist templates for each review type.
