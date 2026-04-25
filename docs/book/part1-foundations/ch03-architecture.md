# Chapter 3: Architecture

*Srikanth Patchava & EmbeddedOS Contributors*

---

## 3.1 Design Principles


![Figure: EoS 6-Layer System Architecture — from hardware through kernel to applications](images/eos-architecture.png)

EoS is built on six foundational principles:

1. **Simplicity over completeness** -- Easy to learn, easy to extend
2. **Reuse existing tools** -- Never rewrite build systems; wrap them
3. **Build incrementally** -- Cache aggressively, rebuild only what changed
4. **Developer experience first** -- Clear errors, helpful logs, fast feedback
5. **Separate system kinds** -- Linux and RTOS are peers, not subsets
6. **RTOS is a first-class citizen** -- Not squeezed into Linux abstractions

## 3.2 Layered Architecture

EoS uses a strict layered architecture. Each layer communicates only with its immediate
neighbors through well-defined C interfaces.

`
+-------------------------------------------------------------+
|                      Applications                           |
|              (your firmware / product code)                  |
+-------------------------------------------------------------+
|                        Services                             |
|      networking | OTA | UI | power mgmt | security          |
+-------------------------------------------------------------+
|                      RTOS Kernel                            |
|    tasks | mutex | semaphore | msg queues | sw timers        |
+-------------------------------------------------------------+
|              Hardware Abstraction Layer                      |
|    GPIO | UART | SPI | I2C | Timer | IRQ | ADC | ... (x33)  |
+-------------------------------------------------------------+
|                   Platform Backends                         |
|    STM32 | nRF52 | RPi | Host-Linux | ESP32 | RISC-V | ... |
+-------------------------------------------------------------+
`

### Layer Responsibilities

| Layer | Header(s) | Responsibility |
|-------|----------|---------------|
| **Application** | User code | Product-specific business logic |
| **Services** | services/*.h | Cross-cutting concerns (OTA, networking, UI) |
| **Kernel** | kernel.h, multicore.h | Task scheduling, synchronization, IPC |
| **HAL** | hal.h, hal_extended.h | Vendor-neutral peripheral access |
| **Backend** | backend.h | Platform-specific hardware drivers |

### Information Flow

`
Application
    |
    v  calls eos_gpio_write(pin, true)
   HAL  --> dispatches via backend vtable
    |
    v  backend->gpio_write(pin, true)
 Backend  --> writes to hardware register (or printf on host)
`

The key insight: the HAL API is a **stable contract**. Backends can change (new chip
support, bug fixes) without affecting application code.

## 3.3 The Build System

EoS uses CMake as its meta-build system, but it orchestrates multiple native build
systems depending on the target.

`
               +-------------------------+
               |   CLI (cmd/eos/)         |
               |  build | system |        |
               |  firmware | hybrid | info|
               +------------+------------+
                            |
               +------------v------------+
               |   Core Engine (core/)    |
               |  Config -> Graph ->      |
               |  Scheduler -> Cache      |
               +------------+------------+
                            |
     +----------------------+----------------------+
     |                      |                      |
+----v--------------+  +----v--------+  +----------v----------+
|Build Backends     |  | Package Mgr |  | System Builder      |
| cmake|ninja       |  | resolve     |  | Linux: kernel->     |
| make|kbuild       |  | fetch       |  |   rootfs->image     |
| zephyr|freertos   |  |             |  | RTOS: firmware      |
| nuttx             |  |             |  | Hybrid: both        |
+-------------------+  +-------------+  +---------------------+
`

### Core Engine (core/)

The core engine handles configuration parsing, dependency resolution, and build
scheduling:

| Module | File | Purpose |
|--------|------|---------|
| Types | 	ypes.h | EosArch, EosBuildType, EosSystemKind, EosRtosProvider |
| Error | error.h | Error codes and macros |
| Logger | log.h/c | Leveled logging with color output |
| Config | config.h/c | YAML config parser (no external deps) |
| Graph | graph.h/c | DAG with topological sort |
| Scheduler | scheduler.h/c | Build execution across the dependency graph |
| Cache | cache.h/c | Hash-based rebuild detection |

### Build Backends

EoS never rewrites build logic -- it orchestrates existing build systems through thin
adapter modules:

| Backend | Use Case | Invocation |
|---------|----------|-----------|
| CMake | First-party C/C++ code | cmake -B build && cmake --build build |
| Ninja | Direct ninja invocation | 
inja -C build |
| Make | Legacy Unix packages | make -j |
| Kbuild | Linux kernel, U-Boot | make defconfig && make |
| Zephyr | Zephyr RTOS apps | west build |
| FreeRTOS | FreeRTOS apps | CMake/Make + vendor SDK |
| NuttX | Apache NuttX | make menuconfig && make |

## 3.4 System Kinds

EoS supports three distinct system kinds, each with its own build pipeline:

| Kind | Pipeline | CLI Command |
|------|----------|-------------|
| **Linux** | build -> package -> rootfs -> kernel -> image | os system |
| **RTOS** | build -> package -> firmware | os firmware |
| **Hybrid** | Linux pipeline + RTOS firmware(s) | os hybrid |

### Linux Pipeline

`
Source -> Build -> Package -> Rootfs -> Kernel -> Image
                                        |
                                FHS skeleton
                                install packages
                                configure init
                                create users
`

### RTOS Firmware Pipeline

`
Source -> Configure SDK -> Cross-compile -> Firmware Binary
                                             |
                                       .bin / .elf / .hex / .uf2
`

### Hybrid Pipeline

For multi-core SoCs (e.g., STM32MP1 with Cortex-A + Cortex-M):

`
Phase 1: Build Linux system (kernel + rootfs + image)
Phase 2: Build each RTOS firmware target
Phase 3: Combine outputs for multi-core deployment
`

### Configuration Examples

**RTOS Firmware (eos.yaml):**

`yaml
system:
  kind: rtos
  rtos:
    provider: freertos
    board: stm32f4
    entry: apps/realtime-io
    output: firmware.bin
    format: bin

toolchain:
  target: arm-none-eabi
`

**Hybrid System (eos.yaml):**

`yaml
system:
  kind: hybrid
  linux:
    provider: buildroot
  rtos:
    - provider: freertos
      core: m4
      board: stm32mp1-m4
      entry: apps/realtime-io
      output: firmware-m4.bin

toolchain:
  linux:
    target: aarch64-linux-gnu
  rtos:
    target: arm-none-eabi
`

## 3.5 The Product Profile System

EoS ships with 48 product profiles in the products/ directory. Each profile is a C
header that defines EOS_ENABLE_* preprocessor flags to select which HAL peripherals,
kernel features, and services are compiled into the firmware.

### How Profiles Work

When you pass -DEOS_PRODUCT=robot to CMake, the build system includes
products/robot.h. This header might contain:

`c
// products/robot.h
#define EOS_ENABLE_GPIO     1
#define EOS_ENABLE_UART     1
#define EOS_ENABLE_SPI      1
#define EOS_ENABLE_I2C      1
#define EOS_ENABLE_PWM      1
#define EOS_ENABLE_ADC      1
#define EOS_ENABLE_MOTOR    1
#define EOS_ENABLE_IMU      1
#define EOS_ENABLE_CAMERA   1
#define EOS_ENABLE_WIFI     1
#define EOS_ENABLE_MULTICORE 1
`

### Profile Categories

| Category | Count | Profiles |
|----------|-------|----------|
| Automotive and Transport | 5 | automotive, cockpit, ev, autonomous, infotainment |
| Industrial and Manufacturing | 5 | industrial, plc, robot, vacuum, printer |
| Medical and Health | 5 | medical, diagnostic, telemedicine, fitness, wearable |
| Space and Aerospace | 5 | aerospace, satellite, ground_control, space_comm, drone |
| Consumer Electronics | 5 | mobile, watch, smart_speaker, gaming, xr_headset |
| Smart Home and IoT | 5 | smart_home, thermostat, home_camera, voice, smart_tv |
| Networking and Telecom | 5 | router, gateway, telecom, iot, iptv_stb |
| Computing and Server | 4 | server, desktop, computer, cast_device |
| Financial | 3 | banking, pos, crypto_hw |
| HMI and Security | 2 | hmi, security_cam |
| Testing and Development | 4 | vbox_test, adapter, ai_edge, tv_os |

### Custom Profiles

Create your own profile by adding a header to products/:

`c
// products/my_device.h
#ifndef EOS_PRODUCT_MY_DEVICE_H
#define EOS_PRODUCT_MY_DEVICE_H

#define EOS_ENABLE_GPIO     1
#define EOS_ENABLE_UART     1
#define EOS_ENABLE_BLE      1
#define EOS_ENABLE_DISPLAY  1
#define EOS_ENABLE_TOUCH    1
#define EOS_ENABLE_HAPTICS  1

#endif
`

Then build with:

```bash
cmake -B build -DEOS_PRODUCT=my_device
`

## 3.6 The Layer System

EoS uses six stable layer types for configuration management:

| Layer | Purpose | Example |
|-------|---------|---------|
| **Core** | Default build rules, package schemas | Always included |
| **BSP** | Board-specific: kernel, bootloader, device tree | boards/stm32h7/ |
| **Distro** | Package selection, init system, users | layers/minimal/ |
| **Vendor** | SDK integrations, proprietary components | layers/nordic/ |
| **Product** | Final config, branding, release images | products/robot.h |
| **RTOS** | RTOS kernel config, middleware, scheduler | layers/freertos/ |

Layers are stacked in eos.yaml:

`yaml
layers:
  - layers/core
  - layers/bsp/stm32h7
  - layers/distro/minimal
  - layers/vendor/nordic
`

## 3.7 Three Independent Graphs

EoS internally maintains three graph types to separate concerns:

### 1. Source Graph -- Where Code Comes From

`
git clone -> tarball download -> local path
    +-- checksum verification
    +-- cache lookup
`

### 2. Build Graph -- How Things Are Built

`
zlib --> openssl --> curl --> application
 (make)   (cmake)  (cmake)   (cmake)
`

A DAG (Directed Acyclic Graph) with topological sort determines build order.

### 3. Output Graph -- How Outputs Become Deployable

`
Linux:  kernel + rootfs -> disk image (.raw, .qcow2)
RTOS:   firmware binary -> flash layout (.bin, .hex)
Hybrid: Linux image + RTOS firmware(s) -> combined package
`

## 3.8 Toolchain Definitions

Toolchains are YAML-based definitions specifying CC, CXX, AR, sysroot, and flags:

| Toolchain File | Target Architecture |
|---------------|-------------------|
| aarch64-linux-gnu.cmake | ARM64 Linux |
| x86_64-linux-gnu.cmake | x86_64 Linux |
| iscv64-linux-gnu.cmake | RISC-V 64 Linux |
| arm-none-eabi.cmake | ARM Cortex-M/R bare-metal |

Usage:

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=toolchains/arm-none-eabi.cmake
`

## 3.9 Configuration Schema Summary

The eos.yaml file drives everything. Key sections:

`yaml
project:
  name: <string>         # Project name
  version: <semver>      # Semantic version

workspace:
  backend: <cmake|ninja|make>
  build_dir: <path>
  cache_dir: <path>

toolchain:
  target: <triple>       # e.g., aarch64-linux-gnu

packages:
  - name: <string>
    version: <string>
    source: <url>
    build:
      type: <cmake|ninja|make|kbuild>
    deps:
      - <package-name>

system:
  kind: <linux|rtos|hybrid>
`

## 3.10 Summary

| Concept | Key Takeaway |
|---------|-------------|
| Layered architecture | HAL -> Kernel -> Services -> Apps |
| Backend dispatch | vtable pointer swaps platform at link time |
| System kinds | Linux, RTOS, and Hybrid are equal peers |
| Product profiles | 48 pre-built, or create your own |
| Build orchestration | CMake wraps Ninja, Make, Kbuild, Zephyr, etc. |
| Three graphs | Source, Build, Output -- independently managed |

---

*Next: [Chapter 4 -- Hardware Abstraction Layer](../part2-kernel-hal/ch04-hal.md)*
