# Chapter 36: Contributing to EoS

*Author: Srikanth Patchava & EmbeddedOS Contributors*

---

## 36.1 Introduction

EoS is an open-source project that welcomes contributions from the embedded
systems community. This chapter covers everything you need to know to
contribute effectively — from setting up your development environment to
submitting your first pull request.

---

## 36.2 Prerequisites

| Tool        | Minimum Version | Notes                           |
|-------------|-----------------|----------------------------------|
| **CMake**   | 3.15+           | Build system generator           |
| **GCC**     | 10+             | Or Clang 10+, or MSVC 2019+     |
| **Git**     | 2.25+           | Version control                  |
| **Python**  | 3.8+            | Test scripts and tooling         |
| **Doxygen** | 1.9+            | Optional — API documentation     |

---

## 36.3 Development Setup

```bash
git clone https://github.com/embeddedos-org/eos.git
cd eos
cmake -B build -DEOS_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

To build a specific product profile:

```bash
cmake -B build -DEOS_PRODUCT=robot
cmake --build build
```

---

## 36.4 Contribution Workflow

1. **Fork** the repository on GitHub
2. **Create a feature branch:** `git checkout -b feat/my-feature`
3. **Make your changes** following the code guidelines
4. **Build and test** locally on at least one OS
5. **Commit** with conventional commit messages
6. **Submit a pull request** against the `master` branch

---

## 36.5 Code Guidelines

### C Style

- **Standard:** C11 (`-std=c11`)
- **Warnings:** `-Wall -Wextra` must compile clean (zero warnings)
- **Naming:** `snake_case` for all functions, types, and variables
- **Prefix:** All public symbols use the `eos_` prefix
- **Documentation:** Doxygen-compatible `/** */` doc comments
- **Include guards:** `#ifndef HEADER_NAME_H` / `#define` / `#endif`
- **Types:** Use `<stdint.h>` types (`uint32_t`, `int8_t`, etc.)

### Platform Guards

All platform-specific code must be guarded:

```c
#ifdef _WIN32
    // Windows-specific code
#elif defined(__APPLE__)
    // macOS-specific code
#else
    // Linux/POSIX code
#endif
```

### Portability Rules

- No `__builtin_*` without MSVC fallback
- No hardcoded Unix paths — use `getenv("TEMP")` on Windows
- Always `#include <stddef.h>` when using `size_t`
- Always `#include <stdlib.h>` when using `getenv()`, `malloc()`

---

## 36.6 Commit Messages

Follow Conventional Commits:

```
feat: add CAN bus driver for automotive profile
fix: datacenter popcount portability for MSVC
docs: add quickstart guide for nRF52
ci: add aerospace product build to CI matrix
chore: bump version to 0.6.0
```

---

## 36.7 CI Requirements

All pull requests must pass these checks before merging:

| Check                  | What It Validates                    |
|------------------------|--------------------------------------|
| Build (Linux)          | GCC build with all libraries         |
| Build (Windows)        | MSVC cross-platform portability      |
| Build (macOS)          | Clang build verification             |
| Product profiles (6x)  | Robot, gateway, medical, automotive, iot, aerospace |

### How to Verify Locally

```bash
# Full build with tests
cmake -B build -DEOS_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build --output-on-failure -C Release

# Product profiles
cmake -B build-robot -DEOS_PRODUCT=robot && cmake --build build-robot
cmake -B build-medical -DEOS_PRODUCT=medical && cmake --build build-medical
```

---

## 36.8 Adding New Features

### New HAL Peripheral

1. Add type declarations to `hal/include/eos/hal_extended.h`
2. Add stub implementations to `hal/src/hal_extended_stubs.c`
3. Guard with `#if EOS_ENABLE_*` preprocessor block
4. Add tests to `tests/test_hal.c`

### New Product Profile

1. Create `products/<name>.h` with `EOS_ENABLE_*` flags
2. Add `#elif` case to `include/eos/eos_config.h`
3. Add to CI matrix in `.github/workflows/ci.yml`
4. Test: `cmake -B build -DEOS_PRODUCT=<name> && cmake --build build`

### New Service

1. Create directory under `services/<name>/`
2. Add `CMakeLists.txt` with proper `target_link_libraries`
3. Add header to `include/eos/`
4. Add tests to `tests/test_<name>.c`

---

## 36.9 Pull Request Checklist

- [ ] Code compiles with zero warnings on GCC, Clang, and MSVC
- [ ] All existing tests pass
- [ ] New HAL peripherals include stubs
- [ ] New services include a `CMakeLists.txt`
- [ ] Platform-specific code has `#ifdef` guards
- [ ] No GCC-only builtins without portable fallbacks
- [ ] No hardcoded filesystem paths
- [ ] Commit messages follow conventional commits format

---

## 36.10 Project Structure

```
eos/
├── hal/          # Hardware Abstraction Layer (33 peripherals)
├── kernel/       # RTOS kernel + multicore (SMP/AMP)
├── core/         # OS core, config, logging
├── include/eos/  # Global headers
├── products/     # 41+ product profile headers
├── services/     # Crypto, security, OTA, sensor, motor, filesystem
├── drivers/      # Driver framework
├── power/        # Power management
├── net/          # Networking (sockets, HTTP, MQTT, mDNS)
├── systems/      # Firmware assembly, rootfs
├── toolchains/   # Cross-compilation configs
├── tests/        # Unit test suites
├── examples/     # Example applications
└── docs/         # Documentation
```

---

## 36.11 Reporting Issues

Use GitHub Issues with appropriate labels:

- `bug` — something is broken
- `enhancement` — feature request
- `question` — need clarification

Include: OS, compiler version, product profile, and full error output.
For build failures, attach the full CMake configure and build log.

---

## 36.12 License

By contributing, you agree that your contributions will be licensed under
the MIT License.

---

## 36.13 Summary

Contributing to EoS follows established open-source practices with
additional emphasis on cross-platform portability and safety-critical
code quality.

**Key takeaways:**

- C11 with `-Wall -Wextra` zero-warning policy
- All code must work on GCC, Clang, and MSVC
- Platform-specific code requires `#ifdef` guards
- Conventional commits for clear change history
- CI validates builds on 3 OS platforms and 6 product profiles
- New features need tests, stubs, and documentation

---

*End of Part 7*
