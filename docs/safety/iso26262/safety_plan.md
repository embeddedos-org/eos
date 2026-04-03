<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 EoS Project -->

# ISO 26262 — Safety Plan for EoS

## 1. Purpose

This safety plan defines the safety lifecycle activities, organizational roles, milestones, deliverables, and review schedule for achieving ISO 26262 compliance of the EoS embedded operating system. It serves as the governing document for all functional safety activities.

> **Standard Reference:** ISO 26262:2018, Part 2 — Management of functional safety

## 2. Scope

This plan covers the development of EoS as a Safety Element out of Context (SEooC) per ISO 26262-10. The target ASIL levels range from QM to ASIL D depending on the component (see [overview.md](overview.md)).

## 3. Safety Lifecycle

The EoS safety lifecycle follows ISO 26262 Part 2 and is structured into the following phases:

```
┌─────────────────────────────────────────────────────────────┐
│  1. Concept Phase                                           │
│     ├── Item Definition                                     │
│     ├── Hazard Analysis & Risk Assessment (HARA)            │
│     └── Functional Safety Concept                           │
├─────────────────────────────────────────────────────────────┤
│  2. Product Development (Software — Part 6)                 │
│     ├── Software Safety Requirements Specification          │
│     ├── Software Architectural Design                       │
│     ├── Software Unit Design & Implementation               │
│     ├── Software Unit Testing                               │
│     ├── Software Integration & Testing                      │
│     └── Software Safety Validation                          │
├─────────────────────────────────────────────────────────────┤
│  3. Supporting Processes (Part 8)                           │
│     ├── Configuration Management                            │
│     ├── Change Management                                   │
│     ├── Verification                                        │
│     └── Documentation                                       │
├─────────────────────────────────────────────────────────────┤
│  4. Safety Assessment & Confirmation                        │
│     ├── Confirmation Reviews                                │
│     ├── Functional Safety Assessment                        │
│     └── Safety Case                                         │
└─────────────────────────────────────────────────────────────┘
```

## 4. Organizational Roles and Responsibilities

| Role | Responsibility | Independence Requirement |
|------|---------------|-------------------------|
| **Safety Manager** | Overall safety plan execution, milestone tracking, safety case compilation | Reports to project management; independent of development team |
| **Safety Engineer** | HARA, safety requirements derivation, safety analyses (FMEA, FTA, DFA) | May be part of development team |
| **Software Safety Architect** | Freedom from interference design, MPU partitioning, error containment | Part of development team |
| **Safety Assessor** | Independent assessment of safety lifecycle compliance | Must be independent from the project (ISO 26262-2, Clause 6.4.6) |
| **Verification Engineer** | Test planning, execution, coverage analysis | Independent from unit under test author |
| **Configuration Manager** | Baseline management, change control, traceability | Part of project infrastructure team |
| **Project Manager** | Resource allocation, schedule management, management reviews | Overall project authority |

### 4.1 Independence Requirements per ASIL

| Activity | ASIL A | ASIL B | ASIL C | ASIL D |
|----------|--------|--------|--------|--------|
| Safety plan review | I1 | I1 | I2 | I2 |
| HARA review | I1 | I1 | I2 | I2 |
| Safety requirements review | I1 | I1 | I2 | I2 |
| Architectural design review | I0 | I1 | I1 | I2 |
| Unit design/code review | I0 | I1 | I1 | I2 |
| Test case review | I0 | I1 | I1 | I2 |
| Safety assessment | I1 | I2 | I2 | I3 |

> **I0** = No independence required
> **I1** = Different person
> **I2** = Different team / department
> **I3** = Different organization

## 5. Personnel

| Role | Assigned To | Organization |
|------|------------|--------------|
| Safety Manager | [TODO] | [TODO] |
| Safety Engineer | [TODO] | [TODO] |
| Software Safety Architect | [TODO] | [TODO] |
| Safety Assessor | [TODO] | [TODO — must be external or independent dept.] |
| Verification Engineer | [TODO] | [TODO] |
| Configuration Manager | [TODO] | [TODO] |
| Project Manager | [TODO] | [TODO] |

## 6. Milestones and Deliverables

### 6.1 Milestone Schedule

| ID | Milestone | Target Date | Phase | Entry Criteria | Exit Criteria |
|----|-----------|-------------|-------|----------------|---------------|
| M1 | Safety Plan Approved | [TODO] | Concept | Project charter signed | Safety plan reviewed and baselined |
| M2 | HARA Complete | [TODO] | Concept | Item definition approved | All hazards identified, ASILs assigned |
| M3 | Safety Requirements Baselined | [TODO] | Concept | HARA approved | All safety requirements reviewed, traced to safety goals |
| M4 | Software Architecture Review | [TODO] | Development | Safety requirements baselined | Architecture satisfies freedom from interference; DFA complete |
| M5 | Unit Implementation Complete | [TODO] | Development | Architecture approved | All safety-relevant units implemented per coding guidelines |
| M6 | Unit Test Complete | [TODO] | Development | Implementation complete | Coverage targets met per ASIL (see [verification_plan.md](verification_plan.md)) |
| M7 | Integration Test Complete | [TODO] | Development | Unit tests passed | Integration test coverage targets met |
| M8 | Safety Validation Complete | [TODO] | Validation | Integration tests passed | All safety goals validated at system level |
| M9 | Safety Assessment Complete | [TODO] | Assessment | All deliverables submitted | Independent assessor sign-off |
| M10 | Safety Case Released | [TODO] | Release | Assessment passed | Safety case document package released |

### 6.2 Deliverables Matrix

| Deliverable | ISO 26262 Reference | Responsible Role | Milestone |
|------------|---------------------|-----------------|-----------|
| Safety Plan | Part 2, Clause 6.4.2 | Safety Manager | M1 |
| Item Definition | Part 3, Clause 5 | Safety Engineer | M1 |
| HARA Report | Part 3, Clause 7 | Safety Engineer | M2 |
| Functional Safety Concept | Part 3, Clause 8 | Safety Engineer | M3 |
| Software Safety Requirements | Part 6, Clause 6 | Safety Engineer | M3 |
| Software Architecture Document | Part 6, Clause 7 | Software Safety Architect | M4 |
| DFA Report | Part 9, Clause 7 | Safety Engineer | M4 |
| Software Unit Design | Part 6, Clause 8 | Development Team | M5 |
| Coding Guidelines | Part 6, Clause 8 | Development Team | M5 |
| Unit Test Report | Part 6, Clause 9 | Verification Engineer | M6 |
| Integration Test Report | Part 6, Clause 10 | Verification Engineer | M7 |
| Safety Validation Report | Part 4, Clause 8 | Verification Engineer | M8 |
| Confirmation Review Reports | Part 2, Clause 6.4.5 | Safety Assessor | M9 |
| Safety Case | Part 2, Clause 6.4.7 | Safety Manager | M10 |

## 7. Review Schedule

### 7.1 Confirmation Reviews

| Review | Frequency | Participants | Inputs | Outputs |
|--------|-----------|-------------|--------|---------|
| Safety Plan Review | Once (at M1) | Safety Manager, Assessor, PM | Draft safety plan | Approved safety plan |
| HARA Review | Once (at M2) | Safety Engineer, Assessor | HARA worksheets | Approved HARA report |
| Requirements Review | Per baseline | Safety Engineer, Architect, Assessor | Requirements spec | Review findings, approved spec |
| Architecture Review | Per baseline | Architect, Safety Engineer, Assessor | Architecture doc, DFA | Review findings, approved architecture |
| Code Review | Per commit/PR | Developer (author), Reviewer (independent) | Source code, coding guidelines | Review checklist, approved code |
| Test Review | Per test phase | Verification Engineer, Safety Engineer | Test plans, results | Review findings, approved test reports |
| Safety Assessment | Once (at M9) | Safety Assessor | All deliverables | Assessment report |

### 7.2 Periodic Reviews

| Review Type | Frequency | Purpose |
|-------------|-----------|---------|
| Safety Status Meeting | Bi-weekly | Track progress against milestones, identify risks |
| Management Review | Monthly | Resource allocation, schedule review, escalation |
| Metrics Review | Monthly | Coverage metrics, defect trends, open findings |
| Supplier/Tool Review | Quarterly | Tool qualification status, third-party component status |

## 8. Safety Culture and Training

### 8.1 Training Requirements

| Role | Required Training | Refresher |
|------|------------------|-----------|
| Safety Manager | ISO 26262 Parts 1–2, safety management | Annual |
| Safety Engineer | ISO 26262 Parts 1–6, 8–10; FMEA/FTA methods | Annual |
| Software Safety Architect | ISO 26262 Part 6, AUTOSAR safety concepts | Annual |
| Developers | ISO 26262 Part 6 awareness, MISRA C, coding guidelines | Annual |
| Verification Engineers | ISO 26262 Part 6, MC/DC coverage, test methods | Annual |
| Safety Assessor | ISO 26262 complete, assessment methodology | Per certification body |

### 8.2 Training Records

[TODO]: Maintain training records per ISO 26262-2, Clause 5.4.2. Records shall include:
- Training topic and date
- Attendee list and signatures
- Training materials version
- Assessment/quiz results (if applicable)

## 9. Tailoring and ASIL Decomposition

### 9.1 ASIL Decomposition

Where applicable, ASIL decomposition per ISO 26262-9, Clause 5 may be used to distribute safety requirements across redundant elements:

| Original ASIL | Decomposition | Element 1 | Element 2 |
|---------------|---------------|-----------|-----------|
| ASIL D | D = C + A | ASIL C(D) | ASIL A(D) |
| ASIL D | D = B + B | ASIL B(D) | ASIL B(D) |
| ASIL C | C = B + A | ASIL B(C) | ASIL A(C) |
| ASIL B | B = A + A | ASIL A(B) | ASIL A(B) |

> **[TODO]:** Document any ASIL decomposition decisions with rationale and independence evidence.

## 10. Configuration and Change Management

All safety work products shall be managed under version control with the following requirements:

- **Baseline identification:** Each milestone establishes a configuration baseline
- **Change control:** Changes to baselined safety work products require impact analysis and re-review
- **Traceability:** Bidirectional traceability from safety goals → requirements → design → code → tests
- **Tool support:** Git for version control; issue tracker for change requests and findings

See also: [../../docs/contributing.md](../../contributing.md) for general development workflow.

## 11. Document Cross-References

| Document | Relationship |
|----------|-------------|
| [overview.md](overview.md) | ASIL classification and applicability |
| [hazard_analysis.md](hazard_analysis.md) | HARA results feeding into safety requirements |
| [safety_requirements.md](safety_requirements.md) | Derived from HARA; input to architecture |
| [software_architecture.md](software_architecture.md) | Implements freedom from interference |
| [verification_plan.md](verification_plan.md) | Test methods and coverage criteria |
| [tool_classification.md](tool_classification.md) | Tool qualification for development/verification tools |

## 12. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1 | [TODO] | [TODO] | Initial draft |

---

> **[TODO]:** Assign all personnel roles with names and organizations.
> **[TODO]:** Set target dates for all milestones.
> **[TODO]:** Schedule and conduct initial safety plan review (M1).
> **[TODO]:** Establish training program and record-keeping infrastructure.
