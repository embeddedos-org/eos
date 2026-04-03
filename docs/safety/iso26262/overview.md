<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 EoS Project -->

# ISO 26262 — Functional Safety Overview for EoS

## 1. Purpose

This document defines the applicability of ISO 26262 (Road vehicles — Functional safety) to the EoS embedded operating system. It establishes ASIL classification for EoS components and defines the scope of certification activities.

> **Standard Reference:** ISO 26262:2018 (Second Edition), Parts 1–12

## 2. ASIL Classification Overview

ISO 26262 defines four Automotive Safety Integrity Levels (ASIL A through D), where ASIL D represents the most stringent safety requirements.

| ASIL Level | Integrity | Typical Application | EoS Relevance |
|------------|-----------|---------------------|---------------|
| **ASIL A** | Lowest | Comfort functions, infotainment | Peripheral drivers, logging subsystem |
| **ASIL B** | Moderate | Lighting, wipers | Sensor drivers, communication stack |
| **ASIL C** | High | Cruise control, ABS | Timer subsystem, interrupt management |
| **ASIL D** | Highest | Steering, braking, airbags | Kernel scheduler, memory protection, watchdog |
| **QM** | Non-safety | No safety requirement | Build tools, documentation generators |

## 3. EoS Component Applicability Matrix

The following matrix maps EoS subsystems to their target ASIL classification based on Hazard Analysis and Risk Assessment (HARA) results. See [hazard_analysis.md](hazard_analysis.md) for detailed HARA.

| EoS Component | Module Path | Target ASIL | Rationale |
|----------------|-------------|-------------|-----------|
| Kernel Scheduler | `kernel/sched/` | ASIL D | Task scheduling failure can cause loss of control in safety-critical actuators |
| Memory Protection Unit (MPU) | `kernel/mpu/` | ASIL D | Memory corruption can propagate to safety-critical functions |
| Watchdog Timer | `drivers/watchdog/` | ASIL D | Failure to detect system hang may result in uncontrolled state |
| Interrupt Controller | `kernel/irq/` | ASIL C | Missed or delayed interrupts may degrade control loop timing |
| Timer Subsystem | `kernel/timer/` | ASIL C | Timing violations can cause control loop instability |
| IPC / Message Passing | `kernel/ipc/` | ASIL C | Communication failure between safety tasks may cause inconsistent state |
| GPIO / Digital I/O Driver | `drivers/gpio/` | ASIL B | Incorrect I/O state may cause sensor misreads |
| UART / SPI / I2C Drivers | `drivers/serial/` | ASIL B | Communication errors with safety sensors/actuators |
| ADC / DAC Drivers | `drivers/adc/` | ASIL B | Analog measurement errors affect control accuracy |
| OTA Update Manager | `services/ota/` | ASIL B | Corrupted update could disable safety functions |
| Crypto Subsystem | `crypto/` | ASIL A | Integrity verification of safety-relevant data |
| Logging Subsystem | `services/log/` | ASIL A | Diagnostic data recording for post-incident analysis |
| Shell / Debug Console | `services/shell/` | QM | Development-only; disabled in production safety builds |
| Build System / Tooling | `cmake/`, `tools/` | QM | Not deployed on target hardware |

## 4. Scope of Certification

### 4.1 In-Scope

- All kernel components (scheduler, MPU, IRQ, timer, IPC)
- All hardware abstraction layer (HAL) drivers used in safety-critical signal paths
- Watchdog timer driver and supervisor
- Boot sequence and initialization code
- OTA update verification (signature and integrity checks)
- Safety-relevant configuration management

### 4.2 Out-of-Scope

- Application-layer code (responsibility of system integrator)
- Third-party libraries not modified by EoS project (assessed separately)
- Development tools and host-side utilities
- Simulation and test frameworks (covered under [tool_classification.md](tool_classification.md))

### 4.3 Assumptions of Use

1. The system integrator is responsible for system-level HARA and safety case
2. EoS is used as a Safety Element out of Context (SEooC) per ISO 26262-10
3. Hardware platform meets ISO 26262 Part 5 requirements for the target ASIL
4. The MPU hardware supports at least 8 memory protection regions
5. A hardware watchdog with independent clock source is available

## 5. Safety Element out of Context (SEooC)

EoS is developed as a SEooC per ISO 26262-10, Clause 9. This means:

- **Assumed safety requirements** are defined based on typical automotive use cases
- The system integrator must verify that assumed requirements match their actual system-level requirements
- A **Safety Manual** accompanies the EoS release to document assumptions, constraints, and integration guidelines

## 6. Compliance Strategy

| ISO 26262 Part | Title | EoS Compliance Approach |
|----------------|-------|-------------------------|
| Part 2 | Management of functional safety | Safety plan, confirmation reviews — see [safety_plan.md](safety_plan.md) |
| Part 4 | Product development at system level | System-level HARA (integrator responsibility); EoS provides assumed safety goals |
| Part 6 | Product development at software level | Full compliance for ASIL B–D components — see [software_architecture.md](software_architecture.md) |
| Part 8 | Supporting processes | Configuration management, change management, documentation |
| Part 9 | ASIL-oriented and safety-oriented analyses | FMEA, FTA for kernel subsystems |
| Part 10 | Guidelines on ISO 26262 | SEooC development approach |

## 7. Document Cross-References

| Document | Purpose |
|----------|---------|
| [safety_plan.md](safety_plan.md) | Safety lifecycle, roles, milestones |
| [hazard_analysis.md](hazard_analysis.md) | HARA with example hazards |
| [safety_requirements.md](safety_requirements.md) | Functional and technical safety requirements |
| [software_architecture.md](software_architecture.md) | Freedom from interference, MPU partitioning |
| [verification_plan.md](verification_plan.md) | Test methods and coverage metrics |
| [tool_classification.md](tool_classification.md) | Tool Confidence Level assessment |
| [../iec61508/overview.md](../iec61508/overview.md) | IEC 61508 applicability for industrial use |
| [../do178c/overview.md](../do178c/overview.md) | DO-178C applicability for avionics use |

## 8. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1 | [TODO] | [TODO] | Initial draft |

---

> **[TODO]:** Update ASIL assignments after completing system-level HARA with integrator.
> **[TODO]:** Add project-specific hardware platform details and MPU capability matrix.
> **[TODO]:** Reference final SEooC safety manual document number.
