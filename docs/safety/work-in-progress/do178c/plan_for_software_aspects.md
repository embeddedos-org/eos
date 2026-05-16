<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 EoS Project -->

# DO-178C — Plan for Software Aspects of Certification (PSAC)

## 1. Purpose

This document is the Plan for Software Aspects of Certification (PSAC) for the EoS embedded operating system. It provides the certification authority with an overview of the EoS software, the certification basis, and the means of compliance with DO-178C objectives.

> **Standard Reference:** RTCA DO-178C, §11.1 — Plan for Software Aspects of Certification

## 2. System Overview

### 2.1 System Description

EoS is a real-time embedded operating system designed for safety-critical applications. It provides deterministic task scheduling, memory protection, inter-process communication, and hardware abstraction for ARM Cortex-M microcontrollers.

### 2.2 Software Overview

| Attribute | Description |
|-----------|-------------|
| **Software Name** | EoS (Embedded Operating System) |
| **Version** | [TODO] |
| **Part Number** | [TODO] |
| **Target Hardware** | ARM Cortex-M4/M7 MCUs (STM32F407 reference platform) |
| **Programming Language** | C (C11), ARM Assembly (startup/context switch) |
| **Estimated SLOC** | [TODO] |
| **Operating System** | EoS is the OS (bare-metal, no underlying OS) |

### 2.3 Software Functions

| Function | Description | DAL |
|----------|-------------|-----|
| Task Scheduling | Priority-preemptive RTOS scheduler with deadline monitoring | DAL A |
| Memory Protection | MPU-based spatial isolation between tasks | DAL A |
| Watchdog Management | Per-task liveness monitoring and HW watchdog integration | DAL A |
| Timer Management | System tick, periodic timers, delay functions | DAL B |
| Interrupt Management | NVIC configuration, ISR dispatch, latency bounding | DAL B |
| Inter-Process Communication | Message passing with CRC integrity and sequence numbers | DAL C |
| Hardware Abstraction | GPIO, UART, SPI, I2C, ADC driver interfaces | DAL C–D |
| Diagnostic Logging | Event logging to persistent storage | DAL D |

## 3. Certification Basis

### 3.1 Applicable Standards

| Standard | Edition | Applicability |
|----------|---------|--------------|
| RTCA DO-178C | 2011 | Primary software certification standard |
| RTCA DO-330 | 2011 | Tool qualification |
| RTCA DO-248C | 2011 | Supporting information (FAQ/discussion papers) |
| RTCA DO-333 | 2011 | Formal methods supplement (future — DAL A components) |

### 3.2 Certification Authority

| Attribute | Value |
|-----------|-------|
| Certification Authority | [TODO] — FAA / EASA / [other] |
| Type Certificate / TSO | [TODO] |
| DER / CVE | [TODO] |
| Certification Basis Document | [TODO] |

## 4. Software Lifecycle

### 4.1 Lifecycle Model

EoS follows a **V-model** software lifecycle aligned with DO-178C:

```
Requirements ──────────────────────────── Validation
     │                                        ▲
     ▼                                        │
  HLR Design ──────────────────────── Integration Test
     │                                        ▲
     ▼                                        │
  LLR Design ────────────────────── Unit Test
     │                                    ▲
     ▼                                    │
   Coding ────────────────────── Code Review
```

### 4.2 Software Lifecycle Data

| Data Item | DO-178C Reference | EoS Document |
|-----------|------------------|-------------|
| Plan for Software Aspects of Certification | §11.1 | This document |
| Software Development Plan | §11.2 | [software_development_plan.md](software_development_plan.md) |
| Software Verification Plan | §11.3 | [../iso26262/verification_plan.md](../iso26262/verification_plan.md) |
| Software Configuration Management Plan | §11.4 | [configuration_management.md](configuration_management.md) |
| Software Quality Assurance Plan | §11.5 | [TODO] |
| Software Requirements Standards | §11.6 | [software_requirements.md](software_requirements.md) |
| Software Design Standards | §11.7 | [software_design.md](software_design.md) |
| Software Code Standards | §11.8 | MISRA C:2012 + EoS coding guidelines |
| Software Requirements Data | §11.9 | [software_requirements.md](software_requirements.md) |
| Design Description | §11.10 | [software_design.md](software_design.md) |
| Source Code | §11.11 | EoS repository (version-controlled) |
| Executable Object Code | §11.12 | Build output (reproducible builds) |
| Software Verification Cases and Procedures | §11.13 | Test case database |
| Software Verification Results | §11.14 | [verification_results.md](verification_results.md) |
| Software Life Cycle Environment Configuration Index | §11.15 | [configuration_management.md](configuration_management.md) |
| Software Configuration Index | §11.16 | Release package manifest |
| Problem Reports | §11.17 | Issue tracker |
| Software Configuration Management Records | §11.18 | Git history + release baselines |
| Software Quality Assurance Records | §11.19 | [TODO] |
| Software Accomplishment Summary | §11.20 | [TODO] |

## 5. Compliance Matrix

| DO-178C Objective | Section | DAL A | DAL B | DAL C | DAL D | Means of Compliance |
|-------------------|---------|-------|-------|-------|-------|---------------------|
| A-1: Software plans comply with §4.0 | §4.0 | ✅ | ✅ | ✅ | ✅ | This PSAC + plans |
| A-2: Development standards defined | §4.5 | ✅ | ✅ | ✅ | ✅ | SDP, coding standards |
| A-3: SW requirements developed | §5.1 | ✅ | ✅ | ✅ | ✅ | HLR/LLR documents |
| A-4: SW design developed | §5.2 | ✅ | ✅ | ✅ | — | Design document |
| A-5: Source code developed | §5.3 | ✅ | ✅ | ✅ | ✅ | Source repository |
| A-6: Verification performed | §6.0 | ✅ | ✅ | ✅ | ✅ | Test results, coverage |
| A-7: CM activities performed | §7.0 | ✅ | ✅ | ✅ | ✅ | SCM plan, baselines |
| A-8: QA activities performed | §8.0 | ✅ | ✅ | ✅ | ✅ | QA records |
| A-9: Certification liaison | §9.0 | ✅ | ✅ | ✅ | ✅ | SAS, SOI attendance |

> **[TODO]:** Complete full compliance matrix with all 71 objectives.

## 6. Means of Compliance Summary

| Compliance Method | Application |
|------------------|-------------|
| Review | Plans, requirements, design documents |
| Analysis | Code complexity, stack usage, timing analysis |
| Testing | Unit, integration, system-level testing with coverage |
| Inspection | Code reviews per coding standards |

## 7. Schedule and Milestones

| Milestone | Description | Target Date |
|-----------|-------------|-------------|
| SOI #1 | Planning review | [TODO] |
| SOI #2 | Development review | [TODO] |
| SOI #3 | Verification review | [TODO] |
| SOI #4 | Final certification review | [TODO] |

## 8. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1 | [TODO] | [TODO] | Initial draft |

---

> **[TODO]:** Complete software part number assignment.
> **[TODO]:** Engage certification authority and assign DER/CVE.
> **[TODO]:** Expand compliance matrix to all DO-178C objectives.
> **[TODO]:** Create Software Quality Assurance Plan.
