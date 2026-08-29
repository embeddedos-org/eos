# EoS — Embedded OS Framework

[![CI](https://github.com/embeddedos-org/eos/actions/workflows/ci.yml/badge.svg)](https://github.com/embeddedos-org/eos/actions/workflows/ci.yml)
[![CodeQL](https://github.com/embeddedos-org/eos/actions/workflows/codeql.yml/badge.svg)](https://github.com/embeddedos-org/eos/actions/workflows/codeql.yml)
[![Scorecard](https://github.com/embeddedos-org/eos/actions/workflows/scorecard.yml/badge.svg)](https://github.com/embeddedos-org/eos/actions/workflows/scorecard.yml)
[![Release](https://github.com/embeddedos-org/eos/actions/workflows/release.yml/badge.svg)](https://github.com/embeddedos-org/eos/actions/workflows/release.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

EoS is a multi-platform embedded OS framework written in pure C11. A single
source tree provides a lightweight RTOS kernel, a hardware abstraction layer
(HAL) with host (Linux) and bare-metal backends, a driver framework, and
networking, power-management, and runtime-service layers. It targets Linux hosts
and bare-metal MCUs through a HAL split, and carries descriptors for 83 boards
and 40+ product profiles selected at configure time.

EoS is the OS core of the **EmbeddedOS** ([embeddedos-org](https://github.com/embeddedos-org))
ecosystem, alongside [eBoot](https://github.com/embeddedos-org/eBoot) (secure
bootloader), [eAI](https://github.com/embeddedos-org/eAI) (on-device AI), and
[eNI](https://github.com/embeddedos-org/eNI) (neural interface). Project version
0.5.0.

## What's inside

| Path | Contents |
|------|----------|
| `kernel/` | Tasks, sync primitives, IPC, multicore — builds `eos_kernel` |
| `hal/` | Hardware abstraction layer; platform split (`hal_linux.c` / `hal_rtos.c`) — builds `eos_hal` |
| `drivers/` | Driver framework and the `devicetree/` parser — builds `eos_drivers` |
| `net/` | Networking abstraction — builds `eos_net` |
| `power/` | Power-management abstraction — builds `eos_power` |
| `core/` | OS config, logging, layer plumbing |
| `services/` | Runtime services: `crypto`, `security`, `os`, `linux`, `linux_ipc`, `gps`, `motor`, `ota`, `filesystem`, `sensor`, `ui`, `init`, and more |
| `systems/` | Rootfs, image, and firmware assembly |
| `boards/` | 83 board descriptor files (`boards/*.yaml`) plus linker scripts |
| `products/` | ~40 product-profile headers selected by `EOS_PRODUCT` |
| `layers/` | `ai`, `bsp`, `core`, `distro`, `product`, `rtos` layer definitions |
| `toolchains/`, `cmake/` | Cross-compile toolchain and helper CMake files |
| `backends/`, `sim/`, `pkg/`, `debug/` | Backend abstraction, simulation, packaging, and debug (GDB stub, core dump) |
| `examples/` | 13 example apps (`blink-gpio`, `ble-sensor`, `multitask-rtos`, `secure-ota`, …) |
| `tests/` | C unit tests plus `functional/`, `fuzz/`, `performance/`, `simulation/` |

## Build

Requires CMake ≥ 3.16 and a C11 compiler (GCC/Clang; MSVC on Windows). C only —
the project declares `LANGUAGES C`.

```bash
cmake -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host --parallel
```

### Build options

| Option | Default | Meaning |
|--------|---------|---------|
| `EOS_BUILD_TESTS` | `OFF` | Enable `ctest` and build `tests/` |
| `EOS_PLATFORM` | `linux` | Target platform: `linux` \| `rtos` (selects the HAL backend) |
| `EOS_PRODUCT` | *(empty)* | One of ~40 product profiles; empty = full build |

`EOS_PLATFORM` resolves to the host HAL when set to `linux` or when not
cross-compiling.

## Test

```bash
# C unit tests
# EOS_PRODUCT is required here: several tests (OTA, sensor, motor, power)
# only compile against a product profile that enables those services.
# vbox_test is the profile meant for host-side testing.
cmake -B build/host -DEOS_BUILD_TESTS=ON -DEOS_PRODUCT=vbox_test
cmake --build build/host --parallel
ctest --test-dir build/host --output-on-failure

# Python suites (unit/functional/performance/simulation, via pytest)
python run_all_tests.py
```

## Getting started

- [`GETTING_STARTED.md`](GETTING_STARTED.md) — from host build to flashing an nRF52 or STM32
- [`examples/`](examples/) — runnable example applications
- [`AGENTS.md`](AGENTS.md) — repo orientation map (targets, options, file map, gotchas)
- [`DEPLOYMENT.md`](DEPLOYMENT.md) — deployment notes
- [`docs/`](docs/) — full documentation set (`mkdocs.yml`); API docs via `Doxyfile`

## License

Licensed under the [MIT License](LICENSE).
