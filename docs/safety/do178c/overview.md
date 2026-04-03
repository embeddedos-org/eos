<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 EoS Project -->

# DO-178C — Software Considerations in Airborne Systems for EoS

## 1. Purpose

This document defines the applicability of DO-178C (Software Considerations in Airborne Systems and Equipment Certification) to the EoS embedded operating system for avionics and airborne applications.

> **Standard Reference:** RTCA DO-178C / EUROCAE ED-12C (2011)

## 2. Design Assurance Levels (DAL)

DO-178C defines five Design Assurance Levels (DAL A through E) based on the failure condition severity:

| DAL | Failure Condition | Description | EoS Relevance |
|-----|-------------------|-------------|---------------|
| **DAL A** | Catastrophic | Failure may cause crash or loss of life | Kernel scheduler, MPU, watchdog |
| **DAL B** | Hazardous/Severe-Major | Large reduction in safety margins | Timer subsystem, interrupt controller |
| **DAL C** | Major | Significant reduction in safety margins | IPC, communication drivers |
| **DAL D** | Minor | Slight reduction in safety margins | Logging, diagnostic subsystem |
| **DAL E** | No Effect | No effect on safety | Debug shell, build tools |

## 3. EoS Component DAL Assignment

| EoS Component | Module Path | Target DAL | Failure Condition Rationale |
|----------------|-------------|-----------|---------------------------|
| Kernel Scheduler | `kernel/sched/` | DAL A | Scheduling failure may cause loss of flight control computer output |
| Memory Protection (MPU) | `kernel/mpu/` | DAL A | Memory corruption may propagate to flight-critical functions |
| Watchdog Timer | `drivers/watchdog/` | DAL A | Failure to detect hung system leaves aircraft in uncontrolled state |
| Timer Subsystem | `kernel/timer/` | DAL B | Timing errors degrade control loop accuracy |
| Interrupt Controller | `kernel/irq/` | DAL B | Interrupt latency affects sensor data freshness |
| IPC / Message Passing | `kernel/ipc/` | DAL C | Message loss degrades but does not eliminate redundant function |
| GPIO / Digital I/O | `drivers/gpio/` | DAL C | Discrete signal misread affects system monitoring |
| UART / SPI / I2C | `drivers/serial/` | DAL C | Communication error with LRU components |
| Logging Subsystem | `services/log/` | DAL D | Loss of diagnostic data is minor operational impact |
| Shell / Debug Console | `services/shell/` | DAL E | Disabled in production; no safety impact |
| Build System / Tooling | `cmake/`, `tools/` | DAL E | Not deployed on target; no direct safety impact |

## 4. DO-178C Objectives Applicability

### 4.1 Objectives Summary by DAL

| Process Area | DAL A | DAL B | DAL C | DAL D | DAL E |
|-------------|-------|-------|-------|-------|-------|
| Software Planning | 7 obj. | 7 obj. | 7 obj. | 7 obj. | 0 obj. |
| Software Development | 7 obj. | 7 obj. | 7 obj. | 5 obj. | 0 obj. |
| Verification | 43 obj. | 34 obj. | 22 obj. | 7 obj. | 0 obj. |
| Configuration Management | 6 obj. | 6 obj. | 6 obj. | 3 obj. | 0 obj. |
| Quality Assurance | 5 obj. | 5 obj. | 5 obj. | 3 obj. | 0 obj. |
| Certification Liaison | 3 obj. | 3 obj. | 3 obj. | 3 obj. | 0 obj. |

### 4.2 Independence Requirements

| Activity | DAL A | DAL B | DAL C | DAL D |
|----------|-------|-------|-------|-------|
| Verification of outputs | Independent | Independent | — | — |
| Verification of test cases | Independent | — | — | — |
| Review of test procedures | Independent | Independent | — | — |

## 5. DO-178C Supplements

EoS may leverage the following DO-178C supplements where applicable:

| Supplement | Standard | EoS Applicability |
|-----------|----------|-------------------|
| Model-Based Development | DO-331 | 🔲 Future — if model-based design is adopted |
| Object-Oriented Technology | DO-332 | ❌ N/A — EoS is written in C |
| Formal Methods | DO-333 | 🔲 Future — for DAL A kernel verification |
| Tool Qualification | DO-330 | ✅ Required — see [tool_qualification section below](#6-tool-qualification) |

## 6. Tool Qualification (DO-330)

Per DO-178C §12.2, tools that could introduce errors or fail to detect errors require qualification. See [../iso26262/tool_classification.md](../iso26262/tool_classification.md) for detailed assessment. DO-330 adds:

| Tool Category | Criteria | EoS Tools |
|--------------|----------|-----------|
| **Criteria 1** | Output is part of airborne software; tool could introduce error | GCC compiler |
| **Criteria 2** | Tool automates verification; could fail to detect error | gcov, cppcheck |
| **Criteria 3** | Tool could fail to detect error but verification is also done by other means | Valgrind, CTest |

| Tool | DO-330 TQL | Qualification Required |
|------|-----------|----------------------|
| GCC | TQL-4 (DAL C) to TQL-1 (DAL A) | Yes — qualification or alternative means |
| gcov/lcov | TQL-5 (DAL C) to TQL-4 (DAL A) | Qualification at DAL A/B |
| cppcheck | TQL-5 | Not required if supplemented by review |
| Valgrind | TQL-5 | Not required (Criteria 3) |

## 7. Certification Strategy

EoS pursues a **Technical Standard Order (TSO)** approach:
1. EoS provides a reusable software component with DO-178C lifecycle data
2. The avionics system integrator includes EoS in their certification application
3. EoS lifecycle data (PSAC, SDP, plans, test results) is part of the certification package
4. The DER/CVE reviews EoS data as part of the system-level certification

## 8. Cross-References

| Document | Purpose |
|----------|---------|
| [plan_for_software_aspects.md](plan_for_software_aspects.md) | PSAC for EoS |
| [software_development_plan.md](software_development_plan.md) | Development standards and methods |
| [software_requirements.md](software_requirements.md) | HLR and LLR templates |
| [software_design.md](software_design.md) | Architecture per DO-178C §5.2 |
| [verification_results.md](verification_results.md) | Test coverage evidence |
| [configuration_management.md](configuration_management.md) | SCM plan |
| [../iso26262/overview.md](../iso26262/overview.md) | ISO 26262 applicability |
| [../iec61508/overview.md](../iec61508/overview.md) | IEC 61508 applicability |

## 9. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1 | [TODO] | [TODO] | Initial draft |

---

> **[TODO]:** Confirm DAL assignments with avionics system integrator.
> **[TODO]:** Engage DER/CVE for preliminary certification basis review.
> **[TODO]:** Assess DO-333 formal methods supplement for DAL A kernel.
