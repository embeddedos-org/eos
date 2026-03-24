# Contributing to EoS

Thank you for your interest in contributing to EoS! This document provides guidelines and instructions for contributing.

---

## Getting Started

1. **Fork** the repository on GitHub
2. **Clone** your fork locally:
   ```bash
   git clone https://github.com/YOUR_USERNAME/EoS.git
   cd EoS
   ```
3. **Create a branch** for your changes:
   ```bash
   git checkout -b feature/my-new-feature
   ```

---

## Project Structure

EoS is organized as three components:

| Directory | Component | Language |
|-----------|-----------|---------|
| `eos/` | Embedded OS (HAL, kernel, services) | C (C11) |
| `eboot/` | Bootloader | C (C11) |
| `ebuild/` | Build system & AI tools | Python (3.9+) |

---

## Development Setup

### For eos / eboot (C)

```bash
# Build and test
cd eos
cmake -B build -DEOS_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### For ebuild (Python)

```bash
cd ebuild
pip install -e ".[dev]"
python -m pytest tests/
```

---

## Code Standards

### C Code (eos, eboot)

- **Standard:** C11
- **Naming:** `eos_module_function()` for public APIs, `_module_internal()` for static functions
- **Types:** Use `stdint.h` types (`uint8_t`, `uint32_t`, etc.)
- **Headers:** Include guards using `#ifndef EOS_MODULE_H` / `#define EOS_MODULE_H`
- **Documentation:** Doxygen-style comments for all public functions
- **Error codes:** Return `0` for success, negative values for errors

### Python Code (ebuild)

- **Standard:** Python 3.9+
- **Style:** PEP 8, enforced by `black` and `ruff`
- **Type hints:** Required for all public functions
- **Docstrings:** Google style

---

## What to Contribute

### High-Impact Areas

- **HAL backends** — port EoS to new MCU families
- **Board support** — add new boards to eboot
- **Example applications** — add working examples with source code
- **Documentation** — quickstart guides, tutorials, API docs
- **Tests** — unit tests for kernel, HAL, services

### Adding a New HAL Peripheral

1. Define the API in `eos/hal/include/eos/hal_extended.h` behind an `#if EOS_ENABLE_*` guard
2. Add the stub implementation in `eos/hal/src/hal_extended_stubs.c`
3. Add Linux backend in `eos/hal/src/hal_linux.c`
4. Add RTOS backend in `eos/hal/src/hal_rtos.c`
5. Add the enable flag to `eos/include/eos/eos_config.h`
6. Add the peripheral to relevant product profiles in `eos/products/`

### Adding a New Board to eboot

1. Create `eboot/boards/<board>/board_<board>.h` with memory map and flash layout
2. Create `eboot/boards/<board>/board_<board>.c` with board init functions
3. Add the board to `eboot/CMakeLists.txt`
4. Create a quickstart doc in `eos/docs/quickstart-<board>.md`

---

## Submitting Changes

1. **Commit** your changes with clear, descriptive messages:
   ```bash
   git commit -m "hal: add CAN FD support to CAN peripheral"
   ```
2. **Push** to your fork:
   ```bash
   git push origin feature/my-new-feature
   ```
3. **Open a Pull Request** against the `main` branch
4. **Describe** your changes: what, why, and how to test

### Commit Message Format

```
<component>: <short description>

<optional longer description>

<optional references>
```

Examples:
```
kernel: fix priority inversion in mutex implementation
eboot: add STM32L4 board support package
ebuild: add --board flag to 'new' command
docs: add quickstart guide for ESP32-C3
```

---

## Testing

- All PRs must pass existing tests
- New features should include tests
- Run the full test suite before submitting:
  ```bash
  # eos tests
  cd eos && cmake -B build -DEOS_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build

  # ebuild tests
  cd ebuild && python -m pytest tests/
  ```

---

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
