# Chapter 35: Safety and Compliance

*Author: Srikanth Patchava & EmbeddedOS Contributors*

---

## 35.1 Introduction

EoS targets safety-critical domains including automotive, aerospace, medical,
and industrial systems. This chapter covers the safety documentation,
compliance frameworks, and engineering practices that enable EoS to meet
international safety standards.

---

## 35.2 Applicable Standards

| Standard       | Domain              | Key Requirements                     |
|----------------|----------------------|--------------------------------------|
| **IEC 61508**  | Industrial           | SIL 1-4 functional safety            |
| **ISO 26262**  | Automotive           | ASIL A-D functional safety           |
| **DO-178C**    | Aerospace            | DAL A-E software assurance           |
| **IEC 62304**  | Medical devices      | Software lifecycle for medical SW    |
| **ISO 15288**  | Systems engineering  | System lifecycle processes           |
| **ISO 25000**  | Software quality     | SQuaRE quality model                 |
| **ISO 27001**  | Information security | Security management system           |
| **FIPS 140-3** | Cryptography         | Cryptographic module validation      |

---

## 35.3 IEC 61508 — Industrial Functional Safety

EoS provides documentation for IEC 61508 compliance in the
`docs/safety/iec61508/` directory:

| Document                    | Purpose                              |
|-----------------------------|--------------------------------------|
| `overview.md`               | IEC 61508 applicability analysis     |
| `sil_checklist.md`          | SIL determination checklist          |
| `software_safety_manual.md` | Software safety manual               |
| `validation_plan.md`        | Validation plan and procedures       |

### SIL Levels and EoS

| SIL Level | Target Failure Rate     | EoS Support                    |
|-----------|-------------------------|--------------------------------|
| SIL 1     | < 10^-5 /hr             | Full (standard configuration)  |
| SIL 2     | < 10^-6 /hr             | Full (with safety profile)     |
| SIL 3     | < 10^-7 /hr             | Partial (requires certification)|
| SIL 4     | < 10^-8 /hr             | Not targeted                   |

---

## 35.4 ISO 26262 — Automotive Functional Safety

Automotive safety documentation in `docs/safety/iso26262/`:

| Document                  | Purpose                              |
|---------------------------|--------------------------------------|
| `overview.md`             | ISO 26262 applicability              |
| `safety_plan.md`          | Safety plan                          |
| `hazard_analysis.md`      | Hazard analysis and risk assessment  |
| `safety_requirements.md`  | Safety requirements specification    |
| `software_architecture.md`| Software architectural design        |
| `tool_classification.md`  | Development tool classification      |
| `verification_plan.md`    | Verification plan                    |

### ASIL Levels and EoS Product Profiles

| ASIL Level | Products        | EoS Profile     |
|------------|-----------------|-----------------|
| ASIL A     | Infotainment    | `infotainment`  |
| ASIL B     | Body electronics| `automotive`    |
| ASIL C     | Chassis systems | `automotive`    |
| ASIL D     | Powertrain, ADAS| `cockpit`       |

---

## 35.5 DO-178C — Aerospace Software Assurance

Aerospace documentation in `docs/safety/do178c/`:

| Document                     | Purpose                            |
|------------------------------|-------------------------------------|
| `overview.md`                | DO-178C applicability               |
| `software_development_plan.md`| Software development plan          |
| `software_requirements.md`   | Software requirements               |
| `software_design.md`         | Software design documentation      |
| `verification_results.md`    | Verification results summary       |
| `configuration_management.md`| Configuration management plan      |
| `plan_for_software_aspects.md`| Plan for Software Aspects of Cert. |

### Design Assurance Levels

| DAL   | Failure Condition       | EoS Profile     |
|-------|-------------------------|-----------------|
| DAL A | Catastrophic            | `aerospace`     |
| DAL B | Hazardous/Severe-Major  | `aerospace`     |
| DAL C | Major                   | `drone`         |
| DAL D | Minor                   | `ground_control`|
| DAL E | No Effect               | Any profile     |

---

## 35.6 Safety-Critical Engineering Practices

### Memory Safety

- Stack overflow detection via `-fstack-protector-strong`
- Static stack analysis with compiler warnings
- No dynamic memory allocation in safety-critical paths
- Memory protection unit (MPU) configuration for task isolation

### Deterministic Behavior

- Worst-case execution time (WCET) analysis for critical tasks
- Bounded interrupt latency
- No unbounded loops in interrupt handlers
- Priority ceiling protocol for mutex operations

### Defensive Programming

```c
// All public APIs validate inputs
int eos_gpio_init(const eos_gpio_config_t *cfg) {
    if (!cfg) return EOS_ERR_INVALID;
    if (cfg->pin >= EOS_MAX_GPIO_PINS) return EOS_ERR_INVALID;
    // ...
}
```

### Redundancy

- Watchdog timer for system health monitoring
- A/B firmware slots for safe rollback (via eBoot)
- CRC/SHA-256 integrity checks on critical data
- Redundant sensor readings with voting

---

## 35.7 Coding Standards

For safety-critical profiles, EoS follows:

| Standard     | Application                              |
|--------------|------------------------------------------|
| MISRA C:2012 | Automotive and industrial code           |
| CERT C       | Secure coding guidelines                 |
| CWE Top 25   | Common weakness enumeration avoidance    |

### Key Rules

- No `malloc()`/`free()` in safety-critical code
- No recursion in interrupt context
- All switch statements have `default` cases
- All function return values are checked
- No implicit type conversions that lose precision

---

## 35.8 Traceability

EoS maintains bidirectional traceability:

```
Requirements --> Design --> Implementation --> Tests --> Verification
     ▲              ▲             ▲              ▲            │
     └──────────────┴─────────────┴──────────────┴────────────┘
```

Each safety requirement is traced to:
1. The design element that addresses it
2. The source code that implements it
3. The test case that verifies it
4. The verification evidence that confirms it

---

## 35.9 Configuration Management

- Git-based version control with signed commits
- Tagged releases with semantic versioning
- SBOM generation (SPDX and CycloneDX formats)
- Change impact analysis for safety-relevant modifications

---

## 35.10 Summary

EoS provides the documentation and engineering practices needed to pursue
certification under multiple international safety standards.

**Key takeaways:**

- Documentation for IEC 61508, ISO 26262, and DO-178C
- Product profiles mapped to safety integrity levels
- Memory safety, determinism, and defensive programming practices
- MISRA C:2012 and CERT C coding standards
- Bidirectional requirements traceability
- SBOM generation for supply chain compliance

---

*Next: Chapter 36 — Contributing to EoS*
