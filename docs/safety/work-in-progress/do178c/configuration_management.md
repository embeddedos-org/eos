<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 EoS Project -->

# DO-178C — Configuration Management Plan for EoS

## 1. Purpose

This document defines the Software Configuration Management (SCM) plan for EoS, covering baseline identification, change control, problem reporting, and environment configuration per DO-178C §7.

> **Standard Reference:** RTCA DO-178C, §7 — Software Configuration Management Process; §11.4 — SCM Plan

## 2. SCM Activities

### 2.1 Overview

| SCM Activity | DO-178C Reference | Description |
|-------------|-------------------|-------------|
| Configuration Identification | §7.2.1 | Identify and label all configuration items |
| Baselines | §7.2.2 | Establish and maintain baselines |
| Traceability | §7.2.3 | Trace configuration items to requirements |
| Problem Reporting | §7.2.4 | Report and track software problems |
| Change Control | §7.2.5 | Control changes to baselined items |
| Change Review | §7.2.6 | Review and approve changes |
| Configuration Status Accounting | §7.2.7 | Track status of configuration items |
| Archive and Retrieval | §7.2.8 | Archive and retrieve configuration items |
| Software Load Control | §7.2.9 | Control loading of executable object code |
| Software Life Cycle Environment Control | §7.2.10 | Control the development/test environment |

## 3. Configuration Items

### 3.1 Software Configuration Items (CI)

| CI Category | Examples | Storage | Identification |
|-------------|---------|---------|----------------|
| Source Code | `.c`, `.h`, `.s` files | Git repository | File path + Git commit hash |
| Build Configuration | `CMakeLists.txt`, toolchain files, linker scripts | Git repository | File path + Git commit hash |
| Requirements | HLR/LLR documents (`.md`) | Git repository | Document version + Git commit hash |
| Design Documents | Architecture, design descriptions (`.md`) | Git repository | Document version + Git commit hash |
| Test Cases | Test source files, test scripts | Git repository | File path + Git commit hash |
| Test Results | Coverage reports, test logs | Build artifacts / archive | Build ID + timestamp |
| Plans | PSAC, SDP, SCM Plan, V&V Plan | Git repository | Document version + Git commit hash |
| Executable Object Code | `.elf`, `.bin`, `.hex` | Build output / release archive | Build ID + SHA-256 checksum |
| Tool Configuration | `.clang-format`, `.clang-tidy`, cppcheck config | Git repository | File path + Git commit hash |

### 3.2 Environment Configuration Items

| CI | Version | Checksum | Purpose |
|----|---------|----------|---------|
| GCC (arm-none-eabi) | [TODO] | [TODO] SHA-256 | Cross-compiler |
| CMake | [TODO] | [TODO] | Build system |
| Git | [TODO] | [TODO] | Version control |
| gcov/lcov | [TODO] | [TODO] | Coverage analysis |
| cppcheck | [TODO] | [TODO] | Static analysis |
| Valgrind | [TODO] | [TODO] | Dynamic analysis |
| QEMU | [TODO] | [TODO] | ARM emulator |
| Host OS | [TODO] | [TODO] | Build/test host |

## 4. Baseline Identification

### 4.1 Baseline Types

| Baseline Type | Trigger | Content | Naming Convention |
|--------------|---------|---------|-------------------|
| **Requirements Baseline** | Requirements review approved | All HLR + LLR documents | `REQ-v{major}.{minor}` |
| **Design Baseline** | Design review approved | Architecture + design documents | `DES-v{major}.{minor}` |
| **Code Baseline** | Code freeze for testing | All source code | `CODE-v{major}.{minor}.{patch}` |
| **Test Baseline** | Test campaign start | Test cases + test environment | `TEST-v{major}.{minor}` |
| **Release Baseline** | Release approval | All CIs for the release | `REL-v{major}.{minor}.{patch}` |

### 4.2 Baseline Procedure

1. CM manager creates a Git tag with the baseline name
2. All CIs in the baseline are identified by the tag
3. Baseline manifest is generated listing all files and their SHA-256 checksums
4. Baseline is archived in a read-only location
5. Baseline is communicated to all project members

### 4.3 Baseline Register

| Baseline ID | Type | Date | Git Tag | Description | Approved By |
|------------|------|------|---------|-------------|-------------|
| [TODO] | [TODO] | [TODO] | [TODO] | [TODO] | [TODO] |

## 5. Change Control

### 5.1 Change Control Process

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│ Change       │    │ Impact       │    │ Review &     │
│ Request (CR) │───►│ Analysis     │───►│ Approval     │
│ Submitted    │    │              │    │              │
└──────────────┘    └──────────────┘    └──────┬───────┘
                                               │
┌──────────────┐    ┌──────────────┐    ┌──────┴───────┐
│ Baseline     │    │ Verification │    │ Implement    │
│ Updated      │◄───│ of Change    │◄───│ Change       │
└──────────────┘    └──────────────┘    └──────────────┘
```

### 5.2 Change Request Template

| Field | Description |
|-------|-------------|
| **CR-ID** | Unique change request identifier |
| **Title** | Short description of the change |
| **Requestor** | Name and role |
| **Date** | Submission date |
| **Affected CIs** | List of configuration items affected |
| **Affected Baselines** | Which baselines are impacted |
| **Rationale** | Why the change is needed |
| **Safety Impact** | Does the change affect safety requirements or ASIL/DAL? |
| **Impact Analysis** | Detailed analysis of downstream effects |
| **Approval** | Reviewer name, date, decision (approve/reject) |
| **Implementation** | Git commit hash(es) implementing the change |
| **Verification** | How the change was verified (test, review, analysis) |

### 5.3 Change Authority per DAL

| Change Type | DAL A | DAL B | DAL C | DAL D |
|------------|-------|-------|-------|-------|
| Safety requirement change | Safety Manager + Assessor | Safety Manager | Safety Engineer | Developer |
| Design change (architecture) | Safety Architect + Safety Manager | Safety Architect | Developer + Reviewer | Developer |
| Code change (safety module) | Developer + Independent Reviewer | Developer + Reviewer | Developer + Reviewer | Developer |
| Test case change | Verification Engineer + Independent | Verification Engineer | Verification Engineer | Developer |
| Plan/document change | Document owner + Safety Manager | Document owner | Document owner | Document owner |

## 6. Problem Reporting

### 6.1 Problem Report Template

| Field | Description |
|-------|-------------|
| **PR-ID** | Unique problem report identifier |
| **Title** | Short description |
| **Reported By** | Name and role |
| **Date Reported** | Date |
| **Category** | Requirements / Design / Code / Test / Tool / Documentation |
| **Severity** | Critical / Major / Minor / Cosmetic |
| **DAL Impact** | Which DAL level is affected |
| **Description** | Detailed problem description |
| **Reproduction Steps** | How to reproduce |
| **Root Cause** | Analysis of underlying cause |
| **Resolution** | Description of fix |
| **Affected CIs** | Configuration items changed |
| **Verification** | How fix was verified |
| **Status** | Open / In Progress / Resolved / Closed |

### 6.2 Problem Report Register

| PR-ID | Title | Severity | DAL | Status | Resolution |
|-------|-------|----------|-----|--------|------------|
| [TODO] | [TODO] | [TODO] | [TODO] | [TODO] | [TODO] |

## 7. Software Load Control

### 7.1 Build Reproducibility

All release builds shall be reproducible:
1. Build from exact Git tag (release baseline)
2. Use pinned toolchain versions from environment CI list
3. Generate SHA-256 checksum of output `.elf` / `.bin`
4. Compare checksum with archived baseline checksum
5. Any mismatch triggers investigation

### 7.2 Release Package Contents

| Item | File | Description |
|------|------|-------------|
| Executable | `eos-v{X.Y.Z}.elf` | ELF binary for target |
| Binary image | `eos-v{X.Y.Z}.bin` | Raw binary for flashing |
| Checksum manifest | `eos-v{X.Y.Z}.sha256` | SHA-256 checksums |
| Release notes | `RELEASE-v{X.Y.Z}.md` | Changes, known issues |
| Safety manual | `safety-manual-v{X.Y.Z}.pdf` | Integration instructions |
| Source archive | `eos-v{X.Y.Z}-src.tar.gz` | Complete source snapshot |

## 8. Archive and Retrieval

| Item | Retention Period | Storage | Access Control |
|------|-----------------|---------|---------------|
| Release baselines | Product lifetime + 10 years | [TODO] — secure archive | Read-only after baseline |
| Test results | Product lifetime + 10 years | [TODO] — secure archive | Read-only |
| Problem reports | Product lifetime + 10 years | Issue tracker + archive | Project team |
| Development artifacts | 5 years after superseded | [TODO] — archive | Project team |

## 9. Cross-References

| Document | Relationship |
|----------|-------------|
| [plan_for_software_aspects.md](plan_for_software_aspects.md) | PSAC referencing this SCM plan |
| [software_development_plan.md](software_development_plan.md) | Development process using CM |
| [verification_results.md](verification_results.md) | Test artifacts under CM |
| [../iso26262/safety_plan.md](../iso26262/safety_plan.md) | Safety lifecycle CM requirements |

## 10. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1 | [TODO] | [TODO] | Initial draft |

---

> **[TODO]:** Pin exact tool versions and record SHA-256 checksums in Section 3.2.
> **[TODO]:** Establish archive storage location and access controls.
> **[TODO]:** Create first requirements baseline after HLR/LLR review.
> **[TODO]:** Set up automated baseline manifest generation in CI/CD.
