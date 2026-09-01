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
and bare-metal MCUs through a HAL split, and carries descriptors for 84 boards
and 48 product profiles selected at configure time. A descriptor is not a port:
71 of the 84 are named `generic-*`, and none has been validated on hardware.

EoS is the OS core of the **EmbeddedOS** ([embeddedos-org](https://github.com/embeddedos-org))
ecosystem, alongside [eBoot](https://github.com/embeddedos-org/eBoot) (secure
bootloader), [eAI](https://github.com/embeddedos-org/eAI) (on-device AI), and
[eNI](https://github.com/embeddedos-org/eNI) (neural interface). Project version
0.5.0.

## Status

Every subsystem carries a §25 maturity state in [`STATUS.md`](STATUS.md), with
the evidence behind it. The repository as a whole is **Experimental**.

Two things worth knowing before building on it:

- **Networking is `Planned`.** `net/` is an API with a POSIX backend for host
  builds. There is no IP stack for bare-metal targets; `eos_net_connect()`
  returns `-1` there. lwIP is the recorded decision ([ADR-014](docs/adr/ADR-014-tcpip-via-lwip.md)).
- **RSA and ECC signature verification is `Planned`.** Those routines are stubs
  and refuse to run unless a build defines `EOS_ALLOW_STUB_CRYPTO`. AES,
  SHA-256 and SHA-512 are implemented and pass NIST vectors.

Nothing here has been observed to boot on hardware. No latency, throughput or
power figure is published, because no reproducible benchmark exists to link to.

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
| `boards/` | 84 board descriptor files (`boards/*.yaml`) plus linker scripts |
| `products/` | 48 product-profile headers selected by `EOS_PRODUCT` |
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
| `EOS_PRODUCT` | *(empty)* | One of 48 product profiles; empty = full build |

`EOS_PLATFORM` resolves to the host HAL when set to `linux` or when not
cross-compiling.

### Cross-compiling

```bash
cmake -B build/arm -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=toolchains/arm-cortex-m4.cmake \
  -DEOS_BUILD_TESTS=OFF
cmake --build build/arm --parallel
```

`toolchains/` carries six toolchain files. Three are exercised on every
release: `arm-cortex-m4`, `arm-none-eabi-stm32f4` and `arm-none-eabi-r5`, which
produce `armv7e-m` and `armv7` objects. The RISC-V and AArch64 files exist but
are unverified — see [`STATUS.md`](STATUS.md).

Footprint of the full library set on Cortex-M4, via `arm-none-eabi-size -t`:
**15.8 KB flash, 26.0 KB RAM**.

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
