<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 EoS Project -->

# ISO 26262 — Hazard Analysis and Risk Assessment (HARA)

## 1. Purpose

This document presents the Hazard Analysis and Risk Assessment (HARA) for EoS. It identifies hazards from OS malfunctions in embedded control systems, classifies them per ISO 26262, and derives safety goals.

> **Standard Reference:** ISO 26262:2018, Part 3, Clause 7

## 2. Scope

EoS is developed as a SEooC. Hazards listed are **assumed hazards** based on typical automotive/industrial use cases. The system integrator must validate these against their system-level HARA.

## 3. HARA Methodology

### 3.1 Risk Parameters

| Parameter | Description | Scale |
|-----------|-------------|-------|
| **Severity (S)** | Potential harm to persons | S0 (no injury) – S3 (life-threatening/fatal) |
| **Exposure (E)** | Probability of operating situation | E0 (incredible) – E4 (high probability) |
| **Controllability (C)** | Ability of operator to control | C0 (controllable) – C3 (uncontrollable) |

### 3.2 ASIL Determination Matrix

Per ISO 26262-3, Table 4:

|  | C1 | C2 | C3 |
|---|---|---|---|
| **S1, E1** | QM | QM | QM |
| **S1, E2** | QM | QM | QM |
| **S1, E3** | QM | QM | A |
| **S1, E4** | QM | A | B |
| **S2, E1** | QM | QM | QM |
| **S2, E2** | QM | QM | A |
| **S2, E3** | QM | A | B |
| **S2, E4** | A | B | C |
| **S3, E1** | QM | QM | A |
| **S3, E2** | QM | A | B |
| **S3, E3** | A | B | C |
| **S3, E4** | B | C | D |

## 4. Operating Situations

| ID | Operating Situation | Description |
|----|---------------------|-------------|
| OS-01 | Normal operation | Nominal conditions, all tasks within timing |
| OS-02 | High CPU load | Peak computational load (>90% utilization) |
| OS-03 | Startup / Init | Boot, peripheral init, task creation |
| OS-04 | Fault recovery | Watchdog reset, error handler active |
| OS-05 | Communication active | Data exchange via UART/SPI/I2C |

## 5. HARA Worksheet

### HAZ-001 — Scheduler Deadline Miss

| Field | Value |
|-------|-------|
| **Hazard ID** | HAZ-001 |
| **Description** | Scheduler fails to dispatch highest-priority safety task within deadline, causing actuator command timeout |
| **EoS Component** | Kernel Scheduler (`kernel/sched/`) |
| **Operating Situation** | OS-02: High CPU load |
| **Severity (S)** | **S3** — Loss of actuator control may cause collision or mechanical damage |
| **Exposure (E)** | **E4** — High CPU load occurs frequently in complex control systems |
| **Controllability (C)** | **C3** — Operator cannot intervene fast enough |
| **ASIL** | **ASIL D** |
| **Safety Goal** | **SG-001:** The scheduler shall guarantee execution of safety-critical tasks within their configured deadlines under all operating conditions |

### HAZ-002 — Memory Corruption Propagation

| Field | Value |
|-------|-------|
| **Hazard ID** | HAZ-002 |
| **Description** | Memory corruption in one task propagates to safety-critical task data, causing incorrect actuator output |
| **EoS Component** | Memory Protection (`kernel/mpu/`) |
| **Operating Situation** | OS-01: Normal operation |
| **Severity (S)** | **S3** — Corrupted control output causes hazardous actuator behavior |
| **Exposure (E)** | **E3** — Memory corruption from software bugs is reasonably probable |
| **Controllability (C)** | **C3** — Operator has no visibility into internal memory state |
| **ASIL** | **ASIL C** |
| **Safety Goal** | **SG-002:** The MPU shall prevent any non-safety task from accessing memory allocated to safety-critical tasks |

### HAZ-003 — Watchdog Failure to Detect Hang

| Field | Value |
|-------|-------|
| **Hazard ID** | HAZ-003 |
| **Description** | Watchdog timer fails to detect a hung safety task, causing system to remain in uncontrolled state indefinitely |
| **EoS Component** | Watchdog (`drivers/watchdog/`) |
| **Operating Situation** | OS-04: Fault recovery |
| **Severity (S)** | **S3** — Indefinite uncontrolled state may lead to physical harm |
| **Exposure (E)** | **E3** — Software hangs due to deadlocks or infinite loops are reasonably probable |
| **Controllability (C)** | **C2** — Operator may eventually notice but response time is uncertain |
| **ASIL** | **ASIL B** |
| **Safety Goal** | **SG-003:** The watchdog shall detect task non-responsiveness within the configured timeout and initiate safe-state transition |

### HAZ-004 — Interrupt Latency Exceeds Control Loop Period

| Field | Value |
|-------|-------|
| **Hazard ID** | HAZ-004 |
| **Description** | Interrupt latency exceeds control loop period, causing stale sensor data and divergent control output |
| **EoS Component** | Interrupt Controller (`kernel/irq/`) |
| **Operating Situation** | OS-02: High CPU load |
| **Severity (S)** | **S2** — Degraded control accuracy may cause instability but not immediate catastrophic failure |
| **Exposure (E)** | **E4** — Interrupt-heavy workloads are common in embedded control |
| **Controllability (C)** | **C2** — Operator may detect degraded performance with delay |
| **ASIL** | **ASIL B** |
| **Safety Goal** | **SG-004:** The interrupt controller shall ensure worst-case interrupt latency does not exceed the configured control loop period |

### HAZ-005 — IPC Message Loss Between Safety Tasks

| Field | Value |
|-------|-------|
| **Hazard ID** | HAZ-005 |
| **Description** | IPC message between safety tasks is lost or corrupted, causing inconsistent state in a safety function |
| **EoS Component** | IPC / Message Passing (`kernel/ipc/`) |
| **Operating Situation** | OS-01: Normal operation |
| **Severity (S)** | **S2** — Inconsistent state may cause degraded safety function but fallback may limit harm |
| **Exposure (E)** | **E3** — IPC is used continuously during normal safety function operation |
| **Controllability (C)** | **C3** — Operator cannot observe or correct internal IPC failures |
| **ASIL** | **ASIL B** |
| **Safety Goal** | **SG-005:** The IPC subsystem shall guarantee message delivery integrity and detect message loss within one control cycle |

## 6. Summary Table

| Hazard ID | Component | ASIL | Safety Goal ID |
|-----------|-----------|------|----------------|
| HAZ-001 | Kernel Scheduler | ASIL D | SG-001 |
| HAZ-002 | Memory Protection (MPU) | ASIL C | SG-002 |
| HAZ-003 | Watchdog Timer | ASIL B | SG-003 |
| HAZ-004 | Interrupt Controller | ASIL B | SG-004 |
| HAZ-005 | IPC / Message Passing | ASIL B | SG-005 |

## 7. Cross-References

| Document | Relationship |
|----------|-------------|
| [overview.md](overview.md) | ASIL classification derived from this HARA |
| [safety_requirements.md](safety_requirements.md) | Safety requirements derived from safety goals above |
| [software_architecture.md](software_architecture.md) | Architecture addresses freedom from interference per SG-002 |
| [verification_plan.md](verification_plan.md) | Verification methods per ASIL level |

## 8. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1 | [TODO] | [TODO] | Initial draft with 5 example hazards |

---

> **[TODO]:** Validate assumed hazards with system integrator's system-level HARA.
> **[TODO]:** Add additional hazards for boot sequence, OTA update, and communication drivers.
> **[TODO]:** Conduct HARA review per safety plan milestone M2.
<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 EoS Project -->

# ISO 26262 — Hazard Analysis and Risk Assessment (HARA) for EoS

## 1. Purpose

This document presents the Hazard Analysis and Risk Assessment (HARA) for the EoS embedded operating system. It identifies potential hazards arising from malfunctions of EoS components in embedded control system applications, classifies them per ISO 26262, and derives safety goals.

> **Standard Reference:** ISO 26262:2018, Part 3, Clause 7 — Hazard analysis and risk assessment

## 2. Scope

This HARA covers EoS operating in embedded control systems with the following assumed operational contexts:

- Automotive ECUs (engine control, body control, chassis)
- Industrial embedded controllers
- Robotics and motion control systems

Since EoS is developed as a SEooC, the hazards listed are **assumed hazards** based on typical use cases. The system integrator must validate these against their specific system-level HARA.

## 3. HARA Methodology

### 3.1 Risk Parameters

Each hazard is classified using three parameters:

| Parameter | Description | Scale |
|-----------|-------------|-------|
| **Severity (S)** | Potential harm to persons | S0 (no injury) – S3 (life-threatening / fatal) |
| **Exposure (E)** | Probability of operating situation | E0 (incredible) – E4 (high probability) |
| **Controllability (C)** | Ability of driver/operator to control | C0 (controllable) – C3 (uncontrollable) |

### 3.2 ASIL Determination Matrix

The ASIL is determined from the combination of S, E, and C per ISO 26262-3, Table 4:

| | C1 | C2 | C3 |
|---|---|---|---|
| **S1, E1** | QM | QM | QM |
| **S1, E2** | QM | QM | QM |
| **S1, E3** | QM | QM | A |
| **S1, E4** | QM | A | B |
| **S2, E1** | QM | QM | QM |
| **S2, E2** | QM | QM | A |
| **S2, E3** | QM | A | B |
| **S2, E4** | A | B | C |
| **S3, E1** | QM | QM | A |
| **S3, E2** | QM | A | B |
| **S3, E3** | A | B | C |
| **S3, E4** | B | C | D |

## 4. Operating Situations

| ID | Operating Situation | Description |
|----|---------------------|-------------|
| OS-01 | Normal operation | System running under nominal conditions, all tasks executing within timing constraints |
| OS-02 | High CPU load | System under peak computational load (>90% utilization) |
| OS-03 | Startup / Initialization | System boot, peripheral initialization, task creation |
| OS-04 | Fault recovery | System responding to detected fault (watchdog reset, error handler active) |
| OS-05 | Communication active | System actively exchanging data via UART/SPI/I2C with sensors and actuators |

## 5. Hazard Analysis and Risk Assessment Table

### HARA Worksheet

| Hazard ID | Hazard Description | EoS Component | Operating Situation | Severity (S) | Exposure (E) | Controllability (C) | ASIL | Safety Goal |
|-----------|-------------------|----------------|---------------------|---------------|--------------|---------------------|------|-------------|
| **HAZ-001** | Scheduler fails to dispatch highest-priority safety task within deadline, causing actuator command timeout | Kernel Scheduler (`kernel/sched/`) | OS-02: High CPU load | **S3** — Loss of actuator control may cause collision or mechanical damage | **E4** — High CPU load occurs frequently in complex control systems | **C3** — Operator cannot intervene fast enough to prevent physical consequence | **ASIL D** | **SG-001:** The scheduler shall guarantee execution of safety-critical tasks within their configured deadlines under all operating conditions |
| **HAZ-002** | Memory corruption in one task propagates to safety-critical task's data, causing incorrect actuator output | Memory Protection (`kernel/mpu/`) | OS-01: Normal operation | **S3** — Corrupted control output may cause hazardous actuator behavior | **E3** — Memory corruption from software bugs is reasonably probable | **C3** — Operator has no visibility into internal memory state | **ASIL C** | **SG-002:** The memory protection unit shall prevent any non-safety task from reading or writing memory allocated to safety-critical tasks |
| **HAZ-003** | Watchdog timer fails to detect a hung safety task, causing the system to remain in an uncontrolled state indefinitely | Watchdog (`drivers/watchdog/`) | OS-04: Fault recovery | **S3** — Indefinite uncontrolled state may lead to physical harm | **E3** — Software hangs due to deadlocks or infinite loops are reasonably probable | **C2** — Operator may eventually notice unresponsive system but response time is uncertain | **ASIL B** | **SG-003:** The watchdog subsystem shall detect task non-responsiveness within the configured timeout period and initiate a safe-state transition |
| **HAZ-004** | Interrupt latency exceeds control loop period, causing sensor data to be stale and control output to diverge | Interrupt Controller (`kernel/irq/`) | OS-02: High CPU load | **S2** — Degrad
