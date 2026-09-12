# AGENTS.md — EoS

EoS (CMake project `EoS`, v0.5.0) is a multi-platform embedded OS framework
written in pure C11: a lightweight RTOS kernel, a hardware abstraction layer
(HAL) with host (Linux) and bare-metal backends, a driver framework, and
networking, power-management, and runtime-service layers. Board/product
selection happens at configure time via descriptors in `boards/` and profiles
in `products/`. (Provenance: `README.md` intro.)

## Layout (from `README.md` "What's inside")

- `kernel/` — tasks, sync primitives, IPC, multicore — builds `eos_kernel`
- `hal/` — HAL platform split (`hal_linux.c` / `hal_rtos.c`, plus `hal_win32.c`)
  — builds `eos_hal`
- `drivers/` — driver framework and the `devicetree/` parser — builds
  `eos_drivers`
- `net/` — networking abstraction (POSIX backend on host builds; bare-metal
  `eos_net_connect()` returns `-1`) — builds `eos_net`
- `power/` — power-management abstraction — builds `eos_power`
- `core/` — OS config, logging, layer plumbing
- `services/` — runtime services: `crypto`, `security`, `os`, `linux`, `gps`,
  `motor`, `ota`, `filesystem`, `sensor`, `ui`, `init`, and more
- `systems/` — rootfs, image, and firmware assembly
- `boards/` — board descriptor files (`boards/*.yaml`) plus linker scripts
- `products/` — product-profile headers selected by `EOS_PRODUCT`
- `layers/`, `toolchains/`, `cmake/` — layer definitions, cross-compile
  toolchain files, CMake helpers
- `backends/`, `sim/`, `pkg/`, `debug/` — backend registry, simulation,
  packaging, debug (GDB stub, core dump)
- `examples/` — example apps (`blink-gpio`, `ble-sensor`, …)
- `tests/` — C unit tests plus `functional/`, `fuzz/`, `performance/`,
  `simulation/`
- `docs/` — full documentation set (`mkdocs.yml`); API docs via `Doxyfile`

## Build (from `README.md` "Build" and `CONTRIBUTING.md` "Development Setup")

Requires CMake ≥ 3.16 and a C11 compiler (GCC/Clang; MSVC on Windows). Native
host build:

```bash
cmake -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host --parallel
```

Build options: `EOS_BUILD_TESTS` (default `OFF`), `EOS_PLATFORM`
(`linux` | `rtos`, default `linux`), `EOS_PRODUCT` (one of the product
profiles; empty = full build).

Cross-compile for a target board with a toolchain file:

```bash
cmake -B build/arm -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=toolchains/arm-cortex-m4.cmake \
  -DEOS_BUILD_TESTS=OFF
cmake --build build/arm --parallel
```

## Test (from `README.md` "Test" and `CONTRIBUTING.md` "Development Setup")

`EOS_PRODUCT=vbox_test` is required alongside `EOS_BUILD_TESTS=ON`: the OTA,
sensor, motor, and power tests only compile against a product profile that
enables those services, and `vbox_test` is the profile meant for host-side
testing.

```bash
cmake -B build/host -DEOS_BUILD_TESTS=ON -DEOS_PRODUCT=vbox_test
cmake --build build/host --parallel
ctest --test-dir build/host --output-on-failure
python run_all_tests.py
```

Python tooling setup: `pip install -r requirements-dev.txt` (installs `pytest`
and `pytest-cov`). `run_all_tests.py` runs pytest over `tests/unit`,
`tests/functional`, `tests/performance`, and `tests/simulation`; direct run:
`python -m pytest tests/ -v`.

## Lint / format

Not defined: no lint or format command is documented in `README.md` or
`CONTRIBUTING.md`. (A `.clang-format` and a `.clang-tidy` config file exist in
the tree, and `ci.yml` runs a cppcheck + clang-tidy static-analysis job, but
the repo documents no command to invoke a formatter or linter locally; C style
rules — C11, `-Wall -Wextra` clean, `eos_` prefix, Doxygen doc comments — are
in `CONTRIBUTING.md` "Code Guidelines".)

## Contributing

See `CONTRIBUTING.md`: fork, create a feature branch (`git checkout -b
feat/my-feature`), run the build and tests locally, then submit a pull request.
Follow Conventional Commits; the PR checklist requires zero-warning compiles on
GCC, Clang, and MSVC, passing tests, HAL stubs for new peripherals,
`#ifdef` platform guards, no GCC-only builtins without portable fallbacks, and
no hardcoded filesystem paths.

## Security

See `SECURITY.md`. Report vulnerabilities to security@embeddedos.org — do NOT
open public issues for vulnerabilities. Response within 48 hours.
