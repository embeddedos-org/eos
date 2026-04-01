# Contributing to EoS

Thank you for your interest in contributing to the Embedded Operating System!


## Prerequisites

| Tool | Minimum Version | Notes |
|---|---|---|
| **CMake** | 3.15+ | Build system generator |
| **GCC** | 10+ | Or Clang 10+, or MSVC 2019+ |
| **Clang** | 10+ | Alternative to GCC |
| **Git** | 2.25+ | Version control |
| **Python** | 3.8+ | For test scripts and tooling |
| **Doxygen** | 1.9+ | Optional — for API documentation generation |
## Getting Started

1. Fork the repository
2. Create a feature branch: `git checkout -b feat/my-feature`
3. Make your changes
4. Run the build and tests locally
5. Submit a pull request

## Development Setup

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

## CI Requirements

All pull requests must pass the following CI checks before merging:

### Required Status Checks

| Check | Platform | What It Validates |
|---|---|---|
| **Build (ubuntu-latest)** | Linux | GCC build with all libraries and services |
| **Build (windows-latest)** | Windows | MSVC build — verifies cross-platform portability |
| **Build (macos-latest)** | macOS | Clang build — verifies Apple platform support |
| **Product (robot)** | Linux | Robot product profile compiles cleanly |
| **Product (gateway)** | Linux | IoT gateway product profile compiles cleanly |
| **Product (medical)** | Linux | Medical device product profile compiles cleanly |
| **Product (automotive)** | Linux | Automotive product profile compiles cleanly |
| **Product (iot)** | Linux | IoT sensor node product profile compiles cleanly |
| **Product (aerospace)** | Linux | Aerospace product profile compiles cleanly |

### How to Verify Locally

Before submitting a PR, ensure all checks pass:

```bash
# 1. Full build with tests
cmake -B build -DEOS_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build --output-on-failure -C Release

# 2. Product profiles (test at least one)
cmake -B build-robot -DEOS_PRODUCT=robot && cmake --build build-robot
cmake -B build-gateway -DEOS_PRODUCT=gateway && cmake --build build-gateway
```

### Nightly Regression (Informational)

The nightly workflow runs additional checks not required for PRs but monitored for regressions:

- Full test suite with `EOS_BUILD_TESTS=ON` on all 3 OS platforms
- All 6 product profile builds
- Cross-compilation for AArch64, ARM hard-float, and RISC-V 64

## Code Guidelines

### C Style

- **Standard:** C11 (`-std=c11`)
- **Warnings:** `-Wall -Wextra` must compile clean (zero warnings)
- **Platform guards:** All platform-specific code must be guarded:
  - `#ifdef _WIN32` for Windows-specific code
  - `#ifdef __APPLE__` for macOS-specific code
  - `#ifdef _MSC_VER` for MSVC compiler intrinsics
  - `#if defined(__GNUC__) || defined(__clang__)` for GCC/Clang builtins
- **Portability rules:**
  - No `__builtin_*` without MSVC fallback (use helper functions like `eos_popcount32()`)
  - No hardcoded Unix paths (`/tmp/`) — use `getenv("TEMP")` on Windows
  - Always `#include <stddef.h>` when using `size_t`
  - Always `#include <stdlib.h>` when using `getenv()`, `malloc()`, etc.
- **Naming:** `snake_case` for all functions, types, and variables
- **Prefix:** All public symbols must use the `eos_` prefix (e.g., `eos_gpio_init`, `eos_task_create`)
- **Documentation:** All public APIs must have Doxygen-compatible `/** */` doc comments
- **Include guards:** Use `#ifndef HEADER_NAME_H` / `#define` / `#endif`
- **Types:** Use `<stdint.h>` types (`uint32_t`, `int8_t`, etc.)

### HAL Extended Peripherals

When adding or modifying HAL peripheral interfaces in `hal/include/eos/hal_extended.h`:

- All type names must be consistent (e.g., use `eos_imu_vec3_t` not `eos_imu_data_t`)
- Types used in `eos_hal_ext_backend_t` must match the types in the API declarations above
- Every `#if EOS_ENABLE_*` block must be self-contained with all required types
- Stub implementations in `hal/src/hal_extended_stubs.c` must match the header exactly

### Product Profiles

When adding a new product profile:

1. Create `products/<name>.h` with `EOS_ENABLE_*` flags
2. Add the `#elif` case to `include/eos/eos_config.h`
3. Add the profile to the CI matrix in `.github/workflows/ci.yml`
4. Test: `cmake -B build -DEOS_PRODUCT=<name> && cmake --build build`

### Commit Messages

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
feat: add CAN bus driver for automotive profile
fix: datacenter popcount portability for MSVC
docs: add quickstart guide for nRF52
ci: add aerospace product build to CI matrix
chore: bump version to 0.6.0
```

### Pull Request Checklist

- [ ] Code compiles with zero warnings on GCC, Clang, and MSVC
- [ ] All existing tests pass
- [ ] New HAL peripherals include stubs in `hal_extended_stubs.c`
- [ ] New services include a `CMakeLists.txt` with proper `target_link_libraries`
- [ ] Platform-specific code has `#ifdef` guards for Windows, macOS, and Linux
- [ ] No GCC-only builtins without portable fallbacks
- [ ] No hardcoded filesystem paths
- [ ] Commit messages follow conventional commits format


## Project Structure Overview

```
eos/
+-- hal/          # Hardware Abstraction Layer (33 peripherals)
+-- kernel/       # RTOS kernel + multicore (SMP/AMP)
+-- core/         # OS core, config, logging, compatibility layers
+-- include/eos/  # Global headers (eos_config.h, backend.h, package.h)
+-- products/     # 41 product profile headers
+-- services/     # Crypto, security, OTA, sensor, motor, filesystem
+-- drivers/      # Driver framework
+-- power/        # Power management
+-- net/          # Networking (sockets, HTTP, MQTT, mDNS)
+-- systems/      # Firmware assembly, rootfs, system image
+-- toolchains/   # Cross-compilation configs
+-- tests/        # Unit test suites
+-- examples/     # Example applications
+-- docs/         # Documentation
```
## Reporting Issues

- Use GitHub Issues with the appropriate label (`bug`, `enhancement`, `question`)
- Include: OS, compiler version, product profile, and full error output
- For build failures: attach the full CMake configure + build log

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
