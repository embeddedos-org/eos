# Chapter 1: Introduction to EmbeddedOS

*Srikanth Patchava & EmbeddedOS Contributors*

---

## 1.1 What Is EmbeddedOS?


![Figure: The Complete EmbeddedOS Ecosystem — 15 products spanning bootloader to browser](images/ecosystem-map.png)

EmbeddedOS (EoS) is a **universal embedded operating system framework** designed to let
engineers write hardware-independent firmware that compiles and runs on any target — from
8-bit microcontrollers to multi-core application processors, from smartwatches to
spacecraft.

At its core, EoS provides:

- A **Hardware Abstraction Layer (HAL)** covering 33 peripheral interfaces
- A lightweight **RTOS kernel** with preemptive multitasking
- A **multicore framework** supporting SMP and AMP topologies
- A **driver framework** with probe/remove lifecycle management
- A **product profile system** with 48 pre-defined hardware configurations
- A **build system** that orchestrates CMake, Ninja, Kbuild, Zephyr, FreeRTOS, and NuttX

EoS follows a single guiding philosophy:

> **Write once, compile for any hardware. Change the board, not the code.**

## 1.2 The Vision

The embedded industry suffers from fragmentation. Every vendor ships its own SDK, its own
HAL, its own build scripts. Moving a project from one microcontroller family to another
often means rewriting 60–80% of the platform code.

EoS eliminates this friction by placing a **stable, vendor-neutral API boundary** between
application code and hardware. The same `eos_gpio_write()` call works whether the target
is an nRF52840, an STM32H743, a Raspberry Pi 4, or a host PC running Linux.

```
    ┌──────────────────────────────────────────────────────┐
    │                Your Application Code                 │
    │     (unchanged across all targets)                   │
    ├──────────────────────────────────────────────────────┤
    │              EoS Public API                          │
    │    hal.h │ kernel.h │ multicore.h │ driver.h         │
    ├──────────────────────────────────────────────────────┤
    │           Backend Dispatch Layer                     │
    │   (vtable pointer → platform-specific impl)         │
    ├──────┬──────┬──────┬──────┬──────┬────────┬─────────┤
    │ Host │ STM32│ nRF52│ RPi4 │ ESP32│ RISC-V │  ...    │
    │ Linux│  HAL │  HAL │  HAL │  HAL │  HAL   │         │
    └──────┴──────┴──────┴──────┴──────┴────────┴─────────┘
```

## 1.3 The 18-Repo Ecosystem

EoS is organized as a family of repositories under the `embeddedos-org` GitHub organization.
Each repo is independently versioned and CI-tested.

| # | Repository | Purpose |
|---|-----------|---------|
| 1 | **eos** | Core OS: HAL, kernel, multicore, drivers, services, products |
| 2 | **eboot** | Secure bootloader with verified boot chain |
| 3 | **ebuild** | Build orchestration CLI and package manager |
| 4 | **ecloud** | Cloud connectivity (MQTT, HTTP, OTA) |
| 5 | **edebug** | Debug tools: trace, profiling, crash dump analysis |
| 6 | **edriver** | Vendor-specific driver packs (STM32, nRF, ESP, TI) |
| 7 | **eflash** | Flash programming utilities and algorithms |
| 8 | **efota** | Firmware-Over-The-Air update framework |
| 9 | **egui** | LVGL-based graphical UI framework |
| 10 | **eiot** | IoT protocols: CoAP, LwM2M, Matter |
| 11 | **eml** | Edge ML inference (TFLite Micro, CMSIS-NN) |
| 12 | **enet** | Lightweight TCP/IP stack (lwIP integration) |
| 13 | **epower** | Power management: sleep modes, DVFS, battery |
| 14 | **esafe** | Functional safety (IEC 61508, ISO 26262) |
| 15 | **esec** | Security services: TLS, crypto, secure storage |
| 16 | **eshell** | Interactive command shell for debugging |
| 17 | **esim** | Hardware simulation and virtual device framework |
| 18 | **etest** | Testing framework: unit, integration, HIL |

### Dependency Graph

```
                           eos (core)
                          / | | | \
                         /  | | |  \
                    eboot  enet esec epower
                      |     |    |     |
                   eflash ecloud esafe edriver
                             |
                           efota
                           eiot
```

All repos depend on `eos` for the core HAL and kernel APIs. Higher-level repos like
`efota` and `eiot` depend on networking (`enet`) and security (`esec`).

## 1.4 Architecture at a Glance

EoS uses a **layered architecture** with clean separation of concerns:

```
┌─────────────────────────────────────────────────────┐
│                   Applications                      │
│           (your firmware / product code)            │
├─────────────────────────────────────────────────────┤
│                    Services                         │
│    networking │ OTA │ UI │ power │ security          │
├─────────────────────────────────────────────────────┤
│                  RTOS Kernel                        │
│   tasks │ mutex │ semaphore │ queues │ timers        │
├─────────────────────────────────────────────────────┤
│           Hardware Abstraction Layer                 │
│  GPIO │ UART │ SPI │ I2C │ Timer │ ADC │ ... (×33)  │
├─────────────────────────────────────────────────────┤
│             Platform Backends                       │
│   STM32 │ nRF52 │ RPi │ Host-Linux │ ESP32 │ ...    │
└─────────────────────────────────────────────────────┘
```

Each layer communicates only with its immediate neighbors. Application code never
touches hardware registers directly — it always goes through the HAL.

## 1.5 Who Should Read This Book

This book is written for:

- **Embedded engineers** who want a portable OS framework
- **Systems architects** evaluating RTOS options for multi-product lines
- **Application developers** building IoT, automotive, medical, or industrial firmware
- **Students** learning embedded systems with a production-quality codebase

### Prerequisites

| Skill | Level Required |
|-------|---------------|
| C programming | Intermediate (pointers, structs, function pointers) |
| Embedded concepts | Basic (GPIO, UART, interrupts) |
| Build tools | Basic familiarity with CMake |
| RTOS concepts | Helpful but not required |

## 1.6 The 48 Product Profiles

EoS ships with 48 pre-defined product profiles in the `products/` directory. Each profile
is a C header that enables the appropriate set of HAL peripherals, kernel features, and
services for a specific product category.

| Category | Profiles |
|----------|----------|
| **Automotive** | `automotive`, `cockpit`, `ev`, `autonomous`, `infotainment` |
| **Industrial** | `industrial`, `plc`, `robot`, `vacuum`, `printer` |
| **Medical** | `medical`, `diagnostic`, `telemedicine`, `fitness`, `wearable` |
| **Aerospace** | `aerospace`, `satellite`, `ground_control`, `space_comm`, `drone` |
| **Consumer** | `mobile`, `watch`, `smart_speaker`, `gaming`, `xr_headset` |
| **Smart Home** | `smart_home`, `thermostat`, `home_camera`, `voice`, `smart_tv` |
| **Networking** | `router`, `gateway`, `telecom`, `iot`, `iptv_stb` |
| **Computing** | `server`, `desktop`, `computer`, `cast_device` |
| **Financial** | `banking`, `pos`, `crypto_hw` |
| **HMI/Display** | `hmi`, `security_cam` |
| **Testing** | `vbox_test`, `adapter` |

Selecting a profile at build time is a single CMake flag:

```bash
cmake -B build -DEOS_PRODUCT=robot
```

This sets the correct `EOS_ENABLE_*` flags to compile only the peripherals your
product actually needs, keeping binary size minimal.

## 1.7 Book Organization

This book is organized in four parts:

| Part | Chapters | Topics |
|------|----------|--------|
| **Part I: Foundations** | 1–3 | Introduction, Getting Started, Architecture |
| **Part II: Kernel & HAL** | 4–8 | HAL, Extended HAL, Kernel, Multicore, Drivers |
| **Part III: Services** | 9–12 | Networking, Security, OTA, Power Management |
| **Part IV: Products** | 13–16 | Product Profiles, Testing, Deployment, Contributing |

Each chapter includes:

- Conceptual explanation with diagrams
- Complete API reference tables
- Working code examples you can compile and run
- Platform-specific notes where behavior differs

## 1.8 Conventions Used in This Book

| Convention | Meaning |
|-----------|---------|
| `monospace` | Code, file paths, command-line input |
| **bold** | First use of an important term |
| `eos_` prefix | All EoS public API functions |
| `EOS_` prefix | All EoS constants and macros |
| `eos_*_config_t` | Configuration struct passed to `*_init()` |
| `eos_*_handle_t` | Opaque handle returned by `*_create()` |

### Return Code Convention

All EoS functions that can fail return `int`:

| Value | Meaning |
|-------|---------|
| `0` | Success |
| `-1` (`EOS_KERN_ERR`) | Generic error |
| `-2` (`EOS_KERN_TIMEOUT`) | Operation timed out |
| `-3` (`EOS_KERN_FULL`) | Resource full (queue, pool) |
| `-4` (`EOS_KERN_EMPTY`) | Resource empty |
| `-5` (`EOS_KERN_INVALID`) | Invalid parameter |
| `-6` (`EOS_KERN_NO_MEMORY`) | Out of memory |

## 1.9 How to Build and Run Examples

Every example in this book can be compiled with:

```bash
cd eos/examples/<example-name>
cmake -B build -DEOS_PRODUCT=iot
cmake --build build
./build/<example-name>
```

For cross-compilation to a specific board:

```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=../../toolchains/arm-none-eabi.cmake \
  -DEOS_PRODUCT=industrial
cmake --build build
```

## 1.10 Getting Help

| Resource | Location |
|----------|----------|
| GitHub Issues | `github.com/embeddedos-org/eos/issues` |
| Documentation | `docs/` directory in each repository |
| API Reference | `docs/api-reference.md` |
| Architecture Guide | `docs/architecture.md` |
| Contributing Guide | `CONTRIBUTING.md` |

---

*Next: [Chapter 2 — Getting Started](ch02-getting-started.md)*
