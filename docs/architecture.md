# 🏗 EoS — Architecture

## Overview

EoS is a **developer-first embedded build system** implemented in C with CMake. It unifies application builds, package management, embedded Linux image generation, RTOS firmware builds, and hybrid multi-core systems through a single CLI and config file.

## System Architecture

```
                   ┌─────────────────────────┐
                   │   CLI (cmd/eos/)         │
                   │  build | system |        │
                   │  firmware | hybrid | info │
                   └────────────┬────────────┘
                                │
                   ┌────────────▼────────────┐
                   │   Core Engine (core/)    │
                   │  Config → Graph →        │
                   │  Scheduler → Cache       │
                   └────────────┬────────────┘
                                │
     ┌──────────────────────────┼──────────────────────────┐
     │                          │                          │
┌────▼────────────┐  ┌─────────▼─────────┐  ┌─────────────▼──────────┐
│ Build Backends  │  │   Package Mgr     │  │   System Builder       │
│ (backends/)     │  │   (pkg/)          │  │   (systems/)           │
│ cmake|ninja|make│  │   resolve         │  │   Linux: kernel→rootfs │
│ kbuild          │  │   fetch           │  │     →image             │
│ zephyr|freertos │  │                   │  │   RTOS: firmware       │
│ nuttx           │  │                   │  │   Hybrid: both         │
└─────────────────┘  └───────────────────┘  └────────────────────────┘
     │                          │                          │
┌────▼────────────┐             │               ┌──────────▼──────────┐
│ Toolchains      │             │               │ Layers & Boards     │
│ (toolchains/)   │             │               │ (layers/boards/)    │
│ aarch64|x86     │             │               │ BSP|distro|product  │
│ arm-none-eabi   │             │               │ vendor|rtos         │
│ riscv           │             │               └─────────────────────┘
└─────────────────┘             │
                       ┌────────▼────────┐
                       │   eos.yaml      │
                       │   Configuration │
                       └─────────────────┘
```

## System Kinds

EoS supports three distinct system kinds, each with its own build pipeline:

| Kind | Pipeline | CLI Command |
|------|----------|-------------|
| Linux | build → package → rootfs → kernel → image | `eos system` |
| RTOS | build → package → firmware | `eos firmware` |
| Hybrid | Linux pipeline + RTOS firmware(s) | `eos hybrid` |

## Module Breakdown

### Core Engine (`core/`)

The heart of EoS — handles config parsing, dependency resolution, and build scheduling.

| File | Purpose |
|------|---------|
| `types.h` | Common types (EosArch, EosBuildType, EosSystemKind, EosRtosProvider, etc.) |
| `error.h` | Error codes and macros |
| `log.h/c` | Leveled logging with color output |
| `config.h/c` | YAML config parser (lightweight, no external deps) |
| `graph.h/c` | DAG with topological sort (Kahn's algorithm) |
| `scheduler.h/c` | Build execution across the graph |
| `cache.h/c` | Hash-based rebuild detection |

### Build Backends (`backends/`)

Thin adapters that invoke native build systems. EoS never rewrites build logic — it orchestrates.

| Backend | Use Case |
|---------|----------|
| CMake | First-party C/C++ code |
| Ninja | Direct ninja invocation |
| Make | Legacy Unix packages |
| Kbuild | Linux kernel, U-Boot |
| Zephyr | Zephyr RTOS apps (west/CMake) |
| FreeRTOS | FreeRTOS apps (CMake/Make/vendor SDK) |
| NuttX | Apache NuttX (native config/build) |

### Package Manager (`pkg/`)

Handles package metadata, dependency resolution, and source fetching.

- **Resolution**: Validates all deps exist before building
- **Fetch**: Supports git clone, tarball download, checksum verification
- **Graph**: Builds a DAG from resolved packages

### Toolchains (`toolchains/`)

YAML-based cross-compilation definitions. Each file specifies CC, CXX, AR, sysroot, flags.

| Toolchain | Target |
|-----------|--------|
| `aarch64-linux-gnu` | ARM64 Linux |
| `x86_64-linux-gnu` | x86_64 Linux |
| `riscv64-linux-gnu` | RISC-V 64 Linux |
| `arm-none-eabi` | ARM Cortex-M/R bare-metal (RTOS) |

### System Builder (`systems/`)

Orchestrates the full build pipeline for each system kind:

**Linux pipeline:**
1. **Kernel**: Build using Kbuild or delegate to provider
2. **Rootfs**: FHS skeleton, install packages, configure init, create users
3. **Image**: Partition table, format filesystems, populate, generate bootable image

**RTOS firmware pipeline:**
1. **Configure**: Set up RTOS SDK for target board
2. **Build**: Cross-compile firmware using provider-specific backend
3. **Output**: Generate firmware binary (.bin, .elf, .hex, .uf2)

**Hybrid pipeline:**
1. **Phase 1**: Build Linux system (kernel + rootfs + image)
2. **Phase 2**: Build each RTOS firmware target
3. **Phase 3**: Combine outputs for multi-core deployment

### Layer System (`layers/`)

Six stable layer types for configuration:

| Layer | Purpose |
|-------|---------|
| Core | Default build rules, package schemas |
| BSP | Board-specific: kernel, bootloader, device tree |
| Distro | Package selection, init system, users |
| Vendor | SDK integrations, proprietary components |
| Product | Final config, branding, release images |
| RTOS | RTOS kernel config, middleware, scheduler policies |

## Three Independent Graphs

EoS internally maintains three graph types:

1. **Source Graph** — Where code comes from (git, tarballs, local)
2. **Build Graph** — How things are built (which backend, what order)
3. **Output Graph** — How outputs become deployable:
   - **Linux**: kernel + rootfs + disk image
   - **RTOS**: firmware binary + flash layout
   - **Hybrid**: Linux image + RTOS firmware(s)

## Config Schema

### Linux System
```yaml
project:
  name: <string>
  version: <semver>

workspace:
  backend: <cmake|ninja|make>
  build_dir: <path>
  cache_dir: <path>

toolchain:
  target: <triple>  # e.g., aarch64-linux-gnu

layers:
  - <path>

packages:
  - name: <string>
    version: <string>
    source: <url>
    hash: <sha256>
    build:
      type: <cmake|ninja|make|kbuild>
    deps:
      - <package-name>

system:
  kind: linux
  linux:
    kernel:
      provider: <kbuild|buildroot>
      defconfig: <string>
    rootfs:
      provider: <eos|buildroot>
      init: <busybox|sysvinit|systemd>
      hostname: <string>
  image_format: <raw|qcow2|tar>
  output: <path>
```

### RTOS Firmware
```yaml
system:
  kind: rtos
  rtos:
    provider: <freertos|zephyr|nuttx|threadx>
    board: <board-name>
    entry: <app-path>
    output: <firmware-path>
    format: <bin|elf|hex|uf2>

toolchain:
  target: arm-none-eabi
```

### Hybrid System
```yaml
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
```

## Design Principles

- **Simplicity over completeness** — easy to learn, easy to extend
- **Reuse existing tools** — never rewrite build systems, wrap them
- **Build incrementally** — cache aggressively, rebuild only what changed
- **Developer experience first** — clear errors, helpful logs, fast feedback
- **Separate system kinds** — Linux and RTOS are peers, not subsets
- **RTOS is a first-class citizen** — not squeezed into Linux abstractions
