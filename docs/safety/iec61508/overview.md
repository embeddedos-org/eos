<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 EoS Project -->

# IEC 61508 — Functional Safety Overview for EoS

## 1. Purpose

This document defines the applicability of IEC 61508 (Functional safety of electrical/electronic/programmable electronic safety-related systems) to the EoS embedded operating system for industrial control system applications.

> **Standard Reference:** IEC 61508:2010 (Edition 2.0), Parts 1–7

## 2. IEC 61508 and EoS

IEC 61508 is the base standard for functional safety of E/E/PE systems used in industrial applications including process control, machinery, and industrial automation. EoS can serve as the OS platform for Safety Instrumented Systems (SIS) in these domains.

## 3. SIL Classification Overview

| SIL Level | PFD (Low Demand) | PFH (High Demand/Continuous) | Typical Application |
|-----------|-------------------|------------------------------|---------------------|
| **SIL 1** | ≥ 10⁻² to < 10⁻¹ | ≥ 10⁻⁶ to < 10⁻⁵ | Advisory alarms, non-critical monitoring |
| **SIL 2** | ≥ 10⁻³ to < 10⁻² | ≥ 10⁻⁷ to < 10⁻⁶ | Emergency shutdown systems, pressure relief |
| **SIL 3** | ≥ 10⁻⁴ to < 10⁻³ | ≥ 10⁻⁸ to < 10⁻⁷ | Burner management, toxic gas detection |
| **SIL 4** | ≥ 10⁻⁵ to < 10⁻⁴ | ≥ 10⁻⁹ to < 10⁻⁸ | Nuclear protection, high-consequence systems |

## 4. EoS SIL Applicability

### 4.1 Target SIL Determination

EoS is designed to support applications up to **SIL 3** in its standard configuration. SIL 4 applications require additional measures beyond the current EoS architecture.

| EoS Component | Target SIL | Rationale |
|----------------|-----------|-----------|
| Kernel Scheduler | SIL 3 | Deterministic scheduling required for safety function timing |
| Memory Protection (MPU) | SIL 3 | Spatial isolation prevents safety function corruption |
| Watchdog Subsystem | SIL 3 | Fault detection for stuck/hung safety functions |
| Timer Subsystem | SIL 3 | Accurate timing for safety function periodicity |
| IPC | SIL 2 | Message integrity between safety function components |
| HAL Drivers (GPIO, ADC) | SIL 2 | Interface to safety sensors and actuators |
| Communication Drivers | SIL 1 | Non-safety communication channels |
| Logging Subsystem | SIL 1 | Diagnostic data for safety function monitoring |

### 4.2 Systematic Capability

EoS targets a **Systematic Capability (SC) of 3**, meaning it can be used as a software element in systems claiming up to SIL 3, provided:

- Hardware meets SIL 3 requirements per IEC 61508 Parts 2 and 5
- System integrator completes system-level safety assessment
- EoS Safety Manual constraints are followed

### 4.3 Proven-in-Use Considerations

Where EoS components have sufficient operational history, a proven-in-use argument per IEC 61508-2, Clause 7.4.7.6 may supplement the development lifecycle evidence. See [software_safety_manual.md](software_safety_manual.md) for proven-in-use criteria.

## 5. Mapping IEC 61508 to ISO 26262

For projects requiring dual compliance (automotive + industrial), the following mapping applies:

| IEC 61508 SIL | Approximate ISO 26262 ASIL |
|---------------|---------------------------|
| SIL 1 | ASIL A |
| SIL 2 | ASIL B |
| SIL 3 | ASIL C / ASIL D |
| SIL 4 | Beyond ASIL D |

> This mapping is approximate. Formal SIL/ASIL correspondence requires analysis per the specific safety function.

## 6. IEC 61508 Compliance Strategy for EoS

| IEC 61508 Part | Title | EoS Approach |
|----------------|-------|-------------|
| Part 1 | General requirements | Safety management via [../iso26262/safety_plan.md](../iso26262/safety_plan.md) (adapted) |
| Part 2 | E/E/PE safety-related systems | Hardware requirements — integrator responsibility |
| Part 3 | Software requirements | Software lifecycle compliance — see [sil_checklist.md](sil_checklist.md) |
| Part 4 | Definitions and abbreviations | Terminology alignment |
| Part 5 | Examples of methods | Informative — referenced in checklist |
| Part 6 | Guidelines on Part 2 and 3 | Informative — referenced in safety manual |
| Part 7 | Overview of techniques | Informative — referenced in validation plan |

## 7. Document Cross-References

| Document | Purpose |
|----------|---------|
| [sil_checklist.md](sil_checklist.md) | Technique/measure checklist per SIL level |
| [software_safety_manual.md](software_safety_manual.md) | Assumptions, constraints, proven-in-use |
| [validation_plan.md](validation_plan.md) | Statistical testing and validation approach |
| [../iso26262/overview.md](../iso26262/overview.md) | ISO 26262 applicability (automotive) |
| [../do178c/overview.md](../do178c/overview.md) | DO-178C applicability (avionics) |

## 8. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1 | [TODO] | [TODO] | Initial draft |

---

> **[TODO]:** Confirm target SIL levels with industrial system integrator requirements.
> **[TODO]:** Assess SIL 4 gap analysis for potential future support.
> **[TODO]:** Document operational history data for proven-in-use argument.
