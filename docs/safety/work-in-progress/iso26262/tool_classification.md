<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 EoS Project -->

# ISO 26262 — Tool Classification for EoS

## 1. Purpose

This document classifies software tools used in EoS development and verification per ISO 26262-8, Clause 11 (Confidence in the use of software tools).

> **Standard Reference:** ISO 26262:2018, Part 8, Clause 11

## 2. Methodology

### 2.1 Tool Impact (TI)

| Level | Description |
|-------|-------------|
| **TI1** | Tool can introduce or fail to detect errors in a safety-related item |
| **TI2** | Tool provides verification outputs but cannot directly introduce errors |

### 2.2 Tool error Detection (TD)

| Level | Description |
|-------|-------------|
| **TD1** | High confidence that tool errors will be detected |
| **TD2** | Medium confidence in detecting tool errors |
| **TD3** | Low confidence in detecting tool errors |

### 2.3 TCL Determination Matrix

| | TD1 | TD2 | TD3 |
|---|---|---|---|
| **TI1** | TCL1 | TCL2 | TCL3 |
| **TI2** | TCL1 | TCL1 | TCL2 |

| TCL | Qualification Effort |
|-----|---------------------|
| **TCL1** | No qualification needed |
| **TCL2** | Increased confidence from use, or tool development per safety lifecycle |
| **TCL3** | Full qualification per ISO 26262-8, Clause 11.4.7 |

## 3. Tool Classifications

### 3.1 GCC (arm-none-eabi-gcc)

| Attribute | Assessment |
|-----------|-----------|
| **Tool** | GCC (GNU Compiler Collection) — arm-none-eabi cross-compiler |
| **Version** | 12.x (pin specific version for certification) |
| **Use** | Compilation of all EoS source code into target binaries |
| **Tool Impact** | **TI1** — Compiler bugs can silently introduce errors into the executable (incorrect code generation, optimization bugs) |
| **Tool error Detection** | **TD2** — Errors may be detected through unit/integration testing of compiled code, but not all compiler bugs are detectable through functional testing alone |
| **TCL** | **TCL2** |
| **Qualification Approach** | Increased confidence from use: GCC validation suite (DejaGnu), widespread industry use, compiler warning analysis, back-to-back testing with a second compiler (Clang) for ASIL D components |
| **Mitigation Measures** | 1. Pin exact compiler version across all builds. 2. Use `-Wall -Wextra -Werror`. 3. Disable aggressive optimizations for safety code (`-O1` or `-O2` max). 4. Back-to-back comparison with Clang for ASIL D modules. 5. Review generated assembly for critical functions. |

### 3.2 CMake

| Attribute | Assessment |
|-----------|-----------|
| **Tool** | CMake — Build system generator |
| **Version** | 3.20+ |
| **Use** | Build configuration, dependency management, test orchestration |
| **Tool Impact** | **TI1** — Incorrect build configuration (wrong flags, missing files, wrong link order) can produce incorrect binaries |
| **Tool error Detection** | **TD1** — Build errors are immediately visible (link failures, missing symbols); incorrect flag application is detectable through build log inspection and binary verification |
| **TCL** | **TCL1** |
| **Qualification Approach** | No qualification required. Build output verification through reproducible builds and binary checksums. |
| **Mitigation Measures** | 1. Version-controlled CMakeLists.txt files. 2. CI/CD verifies exact build flags. 3. Reproducible builds with checksum comparison. |

### 3.3 CTest

| Attribute | Assessment |
|-----------|-----------|
| **Tool** | CTest — Test execution framework (part of CMake) |
| **Version** | 3.20+ (bundled with CMake) |
| **Use** | Execution and reporting of unit, integration, and system tests |
| **Tool Impact** | **TI2** — CTest executes tests and reports results; it cannot introduce errors into the product but could fail to detect errors if tests are not executed or results are misreported |
| **Tool error Detection** | **TD1** — Test execution failures are visible (exit codes, output comparison); test result integrity can be verified by independent re-execution |
| **TCL** | **TCL1** |
| **Qualification Approach** | No qualification required. Test results independently verifiable by re-execution. |
| **Mitigation Measures** | 1. CI/CD enforces all tests pass before merge. 2. Test logs archived with build artifacts. 3. Manual spot-checks of test execution. |

### 3.4 Valgrind

| Attribute | Assessment |
|-----------|-----------|
| **Tool** | Valgrind (memcheck) — Dynamic memory error detector |
| **Version** | 3.20+ |
| **Use** | Detection of memory leaks, use-after-free, buffer overflows, uninitialized reads in host-based tests |
| **Tool Impact** | **TI2** — Valgrind is a verification tool; it detects defects but cannot introduce errors into the product. A false negative (missed defect) reduces verification confidence but does not corrupt the product. |
| **Tool error Detection** | **TD1** — Valgrind errors are clearly reported with stack traces; absence of errors is verifiable by running known-buggy test cases to confirm detection capability |
| **TCL** | **TCL1** |
| **Qualification Approach** | No qualification required. Confidence established through known-defect detection tests (inject known memory errors, verify Valgrind detects them). |
| **Mitigation Measures** | 1. Run Valgrind on all unit tests in CI. 2. Maintain "canary" test with known memory error to verify Valgrind detection. 3. Document Valgrind limitations (no target-hardware memory model). |

### 3.5 cppcheck

| Attribute | Assessment |
|-----------|-----------|
| **Tool** | cppcheck — Static analysis tool with MISRA C:2012 addon |
| **Version** | 2.x |
| **Use** | Static code analysis, MISRA C:2012 compliance checking, defect detection |
| **Tool Impact** | **TI2** — cppcheck is a verification tool; it detects coding standard violations and potential defects but cannot introduce errors. False negatives reduce verification confidence. |
| **Tool error Detection** | **TD2** — Some false negatives are possible (not all MISRA violations detected); effectiveness can be partially validated by running against known non-compliant code samples |
| **TCL** | **TCL1** |
| **Qualification Approach** | No qualification required. Supplement with manual code review for ASIL C/D components. Validate detection capability with MISRA C test suite. |
| **Mitigation Measures** | 1. Run cppcheck with MISRA addon on every commit. 2. Maintain MISRA violation test corpus for validation. 3. Supplement with Clang-Tidy for additional coverage. 4. Manual review for ASIL D code paths. |

## 4. Summary Table

| Tool | Version | Use Category | TI | TD | TCL | Qualification |
|------|---------|-------------|----|----|-----|--------------|
| GCC (arm-none-eabi) | 12.x | Compiler | TI1 | TD2 | **TCL2** | Increased confidence from use + back-to-back |
| CMake | 3.20+ | Build system | TI1 | TD1 | **TCL1** | None required |
| CTest | 3.20+ | Test execution | TI2 | TD1 | **TCL1** | None required |
| Valgrind | 3.20+ | Verification | TI2 | TD1 | **TCL1** | None required |
| cppcheck | 2.x | Verification | TI2 | TD2 | **TCL1** | None required |

## 5. Tool Version Management

All tools used in safety-relevant development shall be:
1. **Pinned to specific versions** in the CI/CD configuration
2. **Documented** with version, download source, and checksum
3. **Re-evaluated** upon any version upgrade (impact analysis required)
4. **Archived** with the release baseline for reproducibility

## 6. Cross-References

| Document | Relationship |
|----------|-------------|
| [verification_plan.md](verification_plan.md) | Tools used in test environment |
| [software_architecture.md](software_architecture.md) | Compilation and build requirements |
| [safety_plan.md](safety_plan.md) | Tool review schedule |

## 7. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1 | [TODO] | [TODO] | Initial draft |

---

> **[TODO]:** Pin exact tool versions and record SHA-256 checksums.
> **[TODO]:** Execute GCC back-to-back testing and document results.
> **[TODO]:** Create MISRA C test corpus for cppcheck validation.
