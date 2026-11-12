# 🏗 EoS — Architecture

> **How to read this document.** Part I (§1–§9) is **present state**:
> only what was verified from the code and documents named in the
> Verification notes appendix. Part II (§10) is **planned state**:
> every item there carries a `[PLANNED — source: …]` marker naming
> the eos document it comes from. Nothing planned is stated as fact.
> Maturity words (`Planned`, `Experimental`, `Implemented`,
> `Validated`) are used in the §25 sense defined in `STATUS.md`.
>
> Snapshot this document was written against: eos `master` at the
> 2026-11-01 foundation-review snapshot (see appendix). The document
> is *living*: the daily loop updates it by PR whenever an
> architecture-level change lands.

---

# Part I — Present state (verified)

## 1. What EoS is today

EoS (CMake project `EoS`, version **0.5.0**, C11 only —
`project(EoS VERSION 0.5.0 … LANGUAGES C)` in the root
`CMakeLists.txt`) is **two cooperating halves in one repository**.
Any architecture description that names only one half is wrong.

| Half | What it is | Where it lives |
|---|---|---|
| **A. RTOS framework** | A lightweight RTOS kernel, HAL, driver framework, networking/power abstractions and runtime services, built as static libraries (`libeos_*.a`) for host (Linux/POSIX) and bare-metal targets | `kernel/`, `hal/`, `drivers/`, `net/`, `power/`, most of `services/`, `debug/`, `boards/`, `products/` |
| **B. Build & system tool** | A build orchestrator: parses a project config, resolves a package/layer graph, drives external build backends (CMake, Ninja, Make, Kbuild, Zephyr, FreeRTOS, NuttX, Buildroot, Cargo, Meson, Autotools), and assembles Linux images, RTOS firmware, and hybrid systems through a CLI | `core/`, `backends/`, `cmd/eos/`, `pkg/`, `systems/`, `toolchains/`, `layers/` |

The halves meet at two seams, verified in the root
`CMakeLists.txt` and in code — and configuration is *not* a third
shared seam:

1. The RTOS libraries are ordinary CMake targets that the build
   tool's CMake backend can build like any other package.
2. The halves are configured **separately** (§6): half A selects a
   product profile via the `EOS_PRODUCT` CMake definition (root
   `CMakeLists.txt`: `add_compile_definitions(EOS_PRODUCT_...)`),
   which chooses a profile header from `products/`; half B takes
   target/board config through its own path (`EosConfig` /
   `EosRtosConfig` board string) and loads no `boards/*.yaml` —
   code search on master finds 0 `EOS_PRODUCT` hits in `core/`,
   `backends/`, `cmd/`, `systems/`, `pkg/`, and 0 `boards` hits in
   `core/`. One product/board selection spanning both halves would
   be a planned unification, not present state.
3. The build tool's config types (`core/include/eos/config.h`,
   the `EosConfig` family) describe *projects that may contain*
   RTOS firmware targets (`EosRtosConfig`: provider, board, core,
   entry, output, format) alongside Linux system definitions —
   i.e. half B models half A as one of its output kinds
   (`linux` / `rtos` / `hybrid`, `core/include/eos/types.h`).

**Repository maturity (from `STATUS.md`, §25): Experimental
overall.** Nothing in eos has been observed to boot on hardware;
no latency, throughput, or power figure is published. Host
evidence recorded in `STATUS.md` (measured 2026-08-29): `ctest`
28/28 pass; Cortex-M4 cross-build produces 23 static libraries,
15.8 KB flash / 26.0 KB RAM by `arm-none-eabi-size -t`. Those are
`STATUS.md`'s recorded measurements, not re-measurements by this
draft.

## 2. System map (present state)

```
                        DEVELOPER / AGENT
                               │
              ┌────────────────┴─────────────────┐
              │  Half B: build & system tool     │   Half A: RTOS framework
              │                                  │   (libraries, linked into
              │  cmd/eos (CLI, host-only)        │    the application image)
              │     │                            │
              │  core/  Config → Graph →         │   ┌───────────────────────┐
              │         Scheduler → Cache        │   │ Application / examples │
              │     │            │               │   └──────────┬────────────┘
              │  backends/   pkg/  systems/      │   services/ (runtime)   │
              │  (11 backend  (resolve,          │   kernel/  (tasks, sync,│
              │   adapters)    fetch)            │             IPC, heap)  │
              │     │            │               │   drivers/ + devicetree │
              │  toolchains/  layers/  boards/   │   net/  power/  debug/  │
              └─────┼────────────┼───────────────┘   hal/ (dispatch +        │
                    ▼            ▼                        backends)          │
              external build  packages,              └──────────┬────────────┘
              systems         rootfs/image/                   ▼
                              firmware assembly      Host OS (POSIX) or
                                                     bare metal (descriptors
                                                     only — §6, §9)
```

There is no single process that "is" EoS at runtime. Half B runs
on the developer's machine. Half A is a set of static libraries
linked into a firmware/host binary. The diagram in the previous
`docs/architecture.md` showed only Half B; the table in
`README.md` ("What's inside") shows mostly Half A. This document
replaces both partial maps.

## 3. Half A — RTOS framework: layers and libraries

Build targets below are as declared in the root `CMakeLists.txt`
(and per-directory `CMakeLists.txt` files it pulls in via
`add_subdirectory()`). Include roots are the
`target_include_directories(... PUBLIC ...)` of each target —
that, not the directory tree alone, is what a consumer can
actually `#include`.

| Layer (top → bottom) | Target(s) | Public headers (include root) | What it does today |
|---|---|---|---|
| Application | — (consumer's own) | — | Links any subset of the libraries below; `examples/` holds 13 example directories (§9) |
| Runtime services | one target per service under `services/` (`services/CMakeLists.txt` + per-service files) | `<svc>/include/eos/*.h` per service | See §3.1 |
| Networking | `eos_net` (`net/src/net.c`, `net/src/mqtt_codec.c`, + `net/src/net_posix.c` on non-cross UNIX) | `net/include/eos/net.h` | Socket-shaped facade; POSIX mapping on host; freestanding fallback returns `-1` (§7) |
| Power | `eos_power` (`power/src/power.c`) | `power/include/eos/power.h` | Power-management abstraction; links `eos_hal` |
| Drivers | `eos_drivers` (`drivers/src/driver_framework.c` only, in the root build) + `eos_devicetree` (`drivers/devicetree/`) | `drivers/include/eos/driver.h`, `drivers/include/eos/drivers.h` | Driver registration framework (§9, gap G-4) + bounds-checked flattened-devicetree parser with a fuzz target (`tests/fuzz/fuzz_devicetree.c`) |
| Kernel | `eos_kernel` (`kernel/src/task.c`, `sync.c`, `ipc.c`, `multicore.c`, `mem/heap.c`) | `kernel/include/eos/kernel.h` (+ `arch.h`, `mem.h`, `multicore.h`, `boot_params.h`) | Tasks, mutex/semaphore sync, IPC, multicore, heap. Links `eos_hal` **PUBLIC** (see gap G-5) |
| HAL | `eos_hal` (`hal/src/hal_common.c`, `hal/src/hal_extended_stubs.c`, + exactly one platform backend TU selected by CMake) | `hal/include/eos/hal.h`, `hal/include/eos/hal_extended.h` | Dispatch layer over a registered backend vtable; §4 |
| Platform backend | (inside `eos_hal`) | — | `hal_linux.c` (host/POSIX), `hal_win32.c` (Windows), `hal_rtos.c` (bare-metal), `hal/src/hal_stm32f4/*` (STM32F4 register-level files), `hal/src/backends/*` (Linux desktop / SDL2 display/input backends) |

### 3.1 Runtime services (`services/`)

Eighteen subdirectories exist in the tree; each carries its own
`CMakeLists.txt` and (with the exceptions in gap G-6) an
`include/eos/*.h` + `src/` layout:

`compat` (POSIX↔EoS OS adapter, `libeos_compat.a`), `crypto`
(AES, SHA-256/512, CRC, Ed25519; RSA/ECC are fail-closed stubs,
§7), `datacenter`, `filesystem`, `gps` (NMEA parsing), `init`
(service manager / daemon lifecycle), `linux` (Linux security
surface), `linux_ipc` (incl. SysV IPC), `motor` (motor control),
`os` (OS services), `ota`, `pkg` (host-side `.eapp` package
manager — host-only except its trust-anchor TU), `posix`
(POSIX I/O/IPC/signals/sync/threads/time compatibility headers),
`rtos` (RTOS security surface), `security` (keystore/ACL;
secure-boot verify path), `sensor`, `ui`, `vxworks`
(VxWorks-style task/sem/msgq/watchdog compatibility surface).

Two structural facts a consumer must know:

- **Module code is feature-gated.** Most service/kernel TUs are
  wrapped in `#if EOS_ENABLE_<MODULE>`, and
  `include/eos/eos_config.h` defaults every flag to `0`. With no
  product profile selected, those TUs compile to *empty
  translation units*. The root `CMakeLists.txt` works around
  this for its own test build by defining a fixed set of flags
  before the first `add_library()` when `EOS_BUILD_TESTS=ON`;
  `README.md` documents that tests require
  `EOS_PRODUCT=vbox_test`. Selecting a product (§6) is
  therefore not optional decoration — it is what turns services
  on.
- **`systems/` straddles the halves.** It builds
  `libeos_systems.a` (rootfs/image/firmware assembly — Half B
  output machinery) and is listed with the RTOS libraries in
  `STATUS.md`'s packaging row. Treat it as Half B machinery that
  ships as a library.

### 3.2 The second kernel-adjacent surface: compat layers

`services/compat`, `services/posix`, `services/linux`,
`services/linux_ipc`, `services/vxworks`, and `services/rtos`
together form an *interoperability surface*: POSIX-, Linux-,
SysV-, VxWorks- and generic-RTOS-shaped APIs implemented over /
alongside the native `eos_*` APIs. This is the code embodiment
of the platform's modular-adoption and interop intent (other
OSes' programming models hosted on EoS). Its timeout semantics
are not uniform with the kernel's — the kernel convention as
read in `docs/safety/robot-control-contract.md` §2 is that
`EOS_WAIT_FOREVER` is `0xFFFFFFFFU` and `EOS_NO_WAIT` is `0U`
(`kernel/include/eos/kernel.h`) — and one compat-semantics
question (eos #202) is explicitly recorded in that contract
document as a maintainer decision, **not** settled by this
document.

## 4. HAL: dispatch and backends (present state)

- `hal/src/hal_common.c` dispatches every public HAL call
  (`hal/include/eos/hal.h`: GPIO, UART, SPI, I2C, timer,
  interrupt, delay/tick) to a registered `eos_hal_backend_t`
  backend (`eos_hal_register_backend()`).
- If the application has not registered a backend,
  `eos_hal_init()` registers the platform default.
  `hal_common.c` selects the registrar from the
  `EOS_HAL_BACKEND_*` compile definition that the root
  `CMakeLists.txt` sets *at the same place* it chooses the
  backend source file (`hal_linux.c` / `hal_win32.c` /
  `hal_rtos.c`), so the compiled backend and the default
  registrar cannot disagree. `hal.h` declares all three
  registrars (`eos_hal_linux_register`,
  `eos_hal_win32_register`, `eos_hal_rtos_register`)
  unconditionally.
- `EOS_HAL_HOSTED` (defined in `hal.h`) is 1 for hosted POSIX
  builds and 0 for bare-metal/Windows; `hal_linux.c` compiles
  to an empty TU when it is 0, which is how the ARM cross-build
  (where `EOS_PLATFORM` still defaults to `linux`) stays safe.
- **Extended peripherals are an API, not an implementation.**
  `hal/include/eos/hal_extended.h` (~1,000 lines) declares
  ADC/DAC/PWM/CAN/USB/Ethernet/WiFi/BLE/camera/audio/display/
  motor/GNSS/IMU/touch/RTC/DMA/flash/watchdog/NFC/IR/cellular/
  radar/GPU/HDMI/PCIe/SDIO/haptics surfaces. In the root build,
  `eos_hal` compiles `hal/src/hal_extended_stubs.c` (740 lines),
  whose functions return `-1`. A partial real implementation
  file, `hal/src/hal_extended.c`, exists in the tree but is not
  compiled by the root build (gap G-3). Maturity: **Planned**
  (`STATUS.md`).

## 5. Half B — build & system tool: components (present state)

| Component | Location | Role (as documented in code + previous `docs/architecture.md`, and consistent with the tree) |
|---|---|---|
| CLI | `cmd/eos/` (`main.c`, `exec_argv.c`) | The `eos` command (`build | system | firmware | hybrid | info` per the previous architecture doc). Host-only: the root build adds it only `if(NOT CMAKE_CROSSCOMPILING)` |
| Core engine | `core/` → config, graph, scheduler, cache, layer, log, lockfile, shell_cmd | Parses the project config into `EosConfig` (`core/include/eos/config.h`), builds dependency graphs (`graph.h`), schedules builds (`scheduler.h`), caches by hash (`cache.h`) |
| Config types | `core/include/eos/types.h`, `error.h` | `EosArch`, `EosBuildType`, `EosSystemKind` (linux/rtos/hybrid), `EosRtosProvider`, `EosResult` — the `Eos*` PascalCase type world (see §8.3) |
| Backends | `backends/src/` | One adapter TU per external build system: `cmake`, `ninja`, `make`, `kbuild`, `autotools`, `buildroot`, `cargo`, `meson`, `freertos`, `nuttx`, `zephyr`; registry in `backends/src/backend.c` — registry API declared in `include/eos/backend.h` (`EosBackend` vtable: configure → build → install → clean) |
| Packages | `pkg/` (+ `services/pkg/` for the on-device `.eapp` manager) | Metadata, dependency resolution, source fetching (git/tarball, checksum) — declared in the `eos/package.h` header pair (gap G-1). Root entry point for a project: `build.yaml` (name `eos`, version `0.5.0`, backend `cmake`) |
| Systems | `systems/src/` (`system.c`, `rootfs.c`, `image.c`, `firmware.c`, `docs.c`) | Assembly pipelines per system kind — **scaffold/partial, not working end to end**: Linux (kernel → rootfs → image) is a scaffold: `eos_system_build_kernel()` warns "Kernel build requires kernel source tree — skipping in scaffold" / "Buildroot integration not yet implemented" and still returns `EOS_OK`; the RAW image case creates only an empty zero-filled file plus a partition table (no kernel or rootfs is written into it); `rootfs.c` builds only an FHS skeleton + hostname/fstab, no packages. RTOS (configure → build → firmware bin/elf/hex/uf2) is the real phase. Hybrid runs the Linux phase + one firmware phase per RTOS target and returns — there is no combine step. A working Linux/hybrid image pipeline is planned state (§10). |
| Toolchains | `toolchains/` | Cross-compile definitions: six CMake toolchain files (`arm-cortex-m4`, `arm-none-eabi-stm32f4`, `arm-none-eabi-r5` exercised per `README.md`; RISC-V and AArch64 files present, unverified per `STATUS.md`) plus YAML toolchain descriptors |
| Layers | `layers/` | Configuration layering — see §6.2 |
| Debug | `debug/` → `eos_debug` | GDB stub + core dump (`debug/include/eos/gdb_stub.h`, `coredump.h`) |

**Build graph facts.** The root `CMakeLists.txt` is the wiring
of record. Order and conditions that matter architecturally:
`eos_hal` → `eos_kernel` (PUBLIC link) → `eos_drivers`;
`add_subdirectory` for `core`, `toolchains`, `backends`,
`cmd/eos` (host-only), `systems`, `services`, `services/pkg`,
`pkg`, `debug`, `drivers/devicetree`, `services/init`;
standalone `eos_power`, `eos_net`; `tests/` only under
`EOS_BUILD_TESTS=ON`. Global include directories are
`include/` and the repo root itself (the latter is what makes
`products/*.h` includable as `"products/<name>.h"` from
`eos_config.h`).

## 6. Configuration: boards, products, layers (present state)

Configuration is where the two halves share one vocabulary —
and where the counts in the docs have drifted from the tree
(gap G-7). This section states the *tree* counts.

### 6.1 Boards — descriptors, not ports

- `boards/` holds **86 top-level `*.yaml` descriptors** (72 of
  them named `generic-*`; 14 named boards: am64x, esp32,
  hiletgo-esp-wroom-32, imx8m, nrf52840, qemu-arm64,
  raspberrypi4, rp2350, samd51, sifive_u, stm32f4, stm32h743,
  stm32mp1, tms570), plus linker scripts and one C board file
  (`boards/stm32f407_discovery/`).
- A descriptor selects memory/peripheral/toolchain facts at
  configure time. **It is not a port and not hardware
  validation**: no board has been observed to boot
  (`README.md`, `STATUS.md`). Maturity: **Planned**.

### 6.2 Products — feature selection

- `products/` holds **48 profile headers**. Selecting
  `EOS_PRODUCT=<name>` defines `EOS_PRODUCT_<NAME>`, and
  `include/eos/eos_config.h` then `#include`s
  `"products/<name>.h"`, whose `EOS_ENABLE_*` definitions turn
  modules on (§3.1).
- Only **42 of the 48** profiles are reachable this way today,
  and the root `CMakeLists.txt` cache-string help lists **41**
  names (gap G-7). Reachability, not the file count, is the
  operative fact for users.
- `products/vbox_test.h` is the host-test profile required for
  the full C test suite (§3.1).

### 6.3 Layers — mostly data, one code layer

- Layer *types* in the tree: `ai`, `bsp`, `core`, `distro`,
  `product`, `rtos` (under `layers/<type>/…`), each defined by
  a `layer.yaml`. BSP instances exist for generic-x86,
  qemu-arm64, raspberrypi4, stm32h7, stm32mp1; distro has
  `minimal`; product has `demo`; rtos has `freertos` and
  `zephyr`.
- Only `layers/ai` contains code: `layers/ai/src/eos_ai.c`
  implementing `layers/ai/include/eos_ai.h` (§8.4). The
  build-tool half resolves layers through `core/`'s layer
  machinery (`core/include/eos/layer.h`) and the root
  `EosConfig.layers[]` list.

## 7. The API layer — eos's public C contracts (present state)

eos is the org's API-layer anchor: the stable, versioned
programmatic surface is its **public C headers**. Reality
today, stated plainly:

### 7.1 What the contract surface is

There is **no single contract directory and no published
stability policy in the repo**. The de-facto contract surface
is the union of:

- `include/eos/*.h` — `eos_config.h` (feature/product config),
  `backend.h`, `package.h`, `quality.h`, `eos_windows.h`;
- per-module `<module>/include/eos/*.h` — kernel, HAL (+
  extended), drivers, net, power;
- per-service `services/<svc>/include/eos/*.h`;
- `systems/include/eos/system.h`,
  `debug/include/eos/*.h`, `toolchains/include/eos/toolchain.h`.

Conventions of that surface, as observed: C11; `eos_` prefix
for functions in the RTOS half; include guards `EOS_<NAME>_H`;
`extern "C"` guards in most (not all — §9) headers; returns are
`0`/`-1` in the RTOS half and `EosResult` in the build-tool
half. Doxygen comments in headers feed the `Doxyfile` build
(`INPUT = hal kernel core services drivers power net systems
include`).

### 7.2 Contract truth table (per `STATUS.md`, cross-read against code)

| Surface (header) | State | What is actually behind it |
|---|---|---|
| Kernel (`kernel.h`) | Experimental | Implemented; `test_kernel` passes per `STATUS.md`. Limits in-header (`EOS_MAX_TASKS` 16, …); `EOS_WAIT_FOREVER 0xFFFFFFFFU`, `EOS_NO_WAIT 0U` |
| HAL core (`hal.h`) | Experimental | Dispatch + host/RTOS/Win32 backends implemented (§4) |
| HAL extended (`hal_extended.h`) | **Planned** | Stub TU only in the root build; functions `return -1` (§4) |
| Sockets (`net.h`, socket section) | Experimental on host / **Planned** on bare metal | Real POSIX mapping in `net_posix.c` (host, non-cross UNIX); freestanding fallback validates args and returns `-1` for connect/send/recv/resolve |
| MQTT (`net.h`, MQTT section) | Experimental (partial) | Codec (`net/src/mqtt_codec.c`) + connect/disconnect implemented in `net.c` over the `eos_net` socket API; **publish / subscribe / unsubscribe / loop still `return -1`** (in-code `TODO(#199 slice 3)`) |
| HTTP (`net.h`) | **Planned** | `eos_http_get/post` `return -1` in *all* builds |
| mDNS (`net.h`) | **Planned** | All entry points `return -1` / no-op in all builds |
| Crypto (`services/crypto/include/eos/crypto.h`) | Mixed | AES, SHA-256, SHA-512, CRC, Ed25519 implemented (NIST vectors pass per `STATUS.md`); **RSA/ECC sign/verify are stubs that refuse to run** unless built with `EOS_ALLOW_STUB_CRYPTO` — fail-closed by design (`services/crypto/src/rsa_ecc_sha512.c`) |
| Security (`services/security/include/eos/security.h`) | Experimental | Keystore/ACL implemented per `STATUS.md`; secure-boot verify path exists in `services/security/src/security.c` and is only as strong as the crypto row above |
| OTA (`services/ota/include/eos/ota.h`) | Experimental | Implemented; `eos_ota_apply()` refuses without a registered authenticator. **Not validated end-to-end against eBoot** (`STATUS.md`) |
| Package manager / #162 envelope (`services/pkg/eos_pkg.h`) | Experimental | The v2 envelope signs capabilities + binary + resources, and its trust anchor fails closed. `eos_pkg_run()` enforces the signed capabilities fail-closed: it refuses any package whose capabilities fall outside the enforceable set — currently empty, because this loader has no confinement mechanism (no seccomp, landlock, namespace, uid separation, or chroot) — including unknown capability bits, so only capability-free packages run, and a refused package stays installed. `verify()` and `install()` refuse a declared binary + resources payload over 64 MiB (`EOS_PKG_MAX_PAYLOAD_BYTES`, overridable with `-DEOS_PKG_MAX_PAYLOAD_BYTES=…`) before allocating anything and before signature verification. Foundation PR-EOS-3 (S-03/S-04); issues #245, #246. |
| Filesystem, power, sensor, motor, GPS, UI, init, compat/posix/linux/vxworks surfaces | Experimental (per `STATUS.md` where listed) | Implemented with unit tests named in `STATUS.md`; per-driver/per-service depth varies and is not uniform — see `STATUS.md` for the per-row evidence, which this document does not restate as stronger than it is |
| Build-tool API (`core/include/eos/*.h`, `include/eos/backend.h`, `package.h`) | Experimental | The `Eos*`/`EosResult` C API of Half B (§5). Version macro drift exists: `include/eos/quality.h` carries `EOS_API_VERSION` **0.1.0** against project version 0.5.0 (gap G-8) |

**Envelope enforcement, honestly:** signing is not enforcement. The loader can enforce no capability at all today, so refusal *is* the enforcement: a package that asks for a device capability cannot run until the loader can confine it to that capability. A platform that gains a real confinement mechanism adds its bits to `eos_pkg_enforceable_capabilities()` and the matching child-side enforcement in `eos_pkg_run()` in the same change, so the mask and the mechanism can never drift apart.

**Versioning, honestly:** the project version is 0.5.0
(`CMakeLists.txt`, `Doxyfile`, `build.yaml`, `sbom.json`).
There is no API-versioning policy, no ABI promise, and no
deprecation process documented in the repo. Consumers today
pin by commit/repository state, not by a contract version.

### 7.3 Known contract defects (present state — not planned fixes)

These are verified in the foundation coherency review and are
part of the present architecture until a PR removes them:

- **G-1 Guard collisions.** `EOS_CONFIG_H` is the guard of
  *both* `include/eos/eos_config.h` and
  `core/include/eos/config.h`; `EOS_PACKAGE_H` guards two
  *diverged* `package.h` files (`include/eos/` vs
  `pkg/include/eos/`); `EOS_GPS_NMEA_H` guards two
  incompatible GPS headers. First inclusion silently suppresses
  the second header.
- **G-2 Duplicate GPS API.** `drivers/include/eos/gps_nmea.h`
  (15 lines, `eos_gps_parse_gga`, `int fix_valid`,
  `sat_count`) vs `services/gps/include/eos/gps_nmea.h`
  (48 lines, `eos_gps_parse_gprmc`, `bool fix_valid`,
  `satellites`, `speed_knots`) — same type name
  `eos_gps_data_t`, different layouts; implementations exist
  in both `drivers/src/gps_nmea.c` and
  `services/gps/src/gps_nmea.c`.
- **G-3 Orphaned HAL implementation.** `hal/src/hal_extended.c`
  is compiled by no target in the root build.
- **G-4 Two driver frameworks.** `drivers/include/eos/driver.h`
  (PascalCase `EosDrv*` types) is implemented by
  `drivers/src/driver.c`, which the root build does not
  compile; `eos_drivers` builds only
  `drivers/src/driver_framework.c` (for `drivers.h`).
- **G-5 Kernel→HAL link.** `eos_kernel` links `eos_hal`
  PUBLIC, so HAL is part of the kernel's link interface.
- **G-6 Header-placement exceptions.** No `eos/` prefix:
  `layers/ai/include/eos_ai.h`; not under `include/eos/`:
  `drivers/devicetree/devicetree.h`,
  `services/pkg/eos_pkg.h`,
  `services/crypto/include/ed25519.h`.
- **G-7 Count drift.** Boards documented as 84, tree has 86;
  products 48 on disk, 42 reachable via `eos_config.h`, 41 in
  the CMake help string; examples documented as 13 apps, 3 of
  the 13 directories contain only an `eos.yaml`.
- **G-8 Version drift.** `EOS_API_VERSION` 0.1.0 in
  `include/eos/quality.h` vs project 0.5.0 everywhere else.

## 8. How the satellite repos attach (eos-side evidence only)

This section describes only what eos's own files establish.
Satellite internals were not read for this draft; where a claim
rests on an eos-side *document* rather than eos *code*, it is
marked `(eos doc)`.

### 8.1 eBoot — boot / image contract

- The contract, from eos's side, is **flash layout + signed
  image + OTA slots**: `docs/integration-guide.md` (eos doc)
  §2 documents ebuild generating `eboot_flash_layout.h` and
  `eboot_linker.ld` (Stage-0 / Stage-1 / boot-control /
  Slot A / Slot B / storage layout), the eos app linking
  against the generated slot linker script
  (`-DEOS_LINKER_SCRIPT=…`), and signing with eBoot's
  `tools/sign_image.py` (eos doc).
- In eos code, the live seam is OTA + crypto:
  `services/ota` (authenticator-gated apply, §7.2) and the
  secure-boot verify path in `services/security`. End-to-end
  validation of that seam against eBoot **has not happened**
  (`STATUS.md`: OTA "Not validated end-to-end against eBoot").
- `DEPLOYMENT.md` (eos doc) pins the trio eos `a9e1b11` /
  eBoot `772f4e2` / ebuild `59a9435` — those SHAs predate the
  current tree and are a record of a past joint state, not a
  current compatibility claim.

### 8.2 ebuild — builds eos

- eos carries an ebuild entry point: root `build.yaml`
  ("Build with: `ebuild build`. ebuild auto-detects
  `CMakeLists.txt` and uses the cmake backend", backend
  `cmake`, defines `EOS_BUILD_TESTS=ON`, `EOS_PLATFORM=linux`).
- `docs/integration-guide.md` (eos doc) §3–§4 documents the
  reverse direction: ebuild *generates eos's inputs* —
  `board.yaml`, `boot.yaml`, `eos_product_config.h`
  (`EOS_ENABLE_*` flags), `eboot_flash_layout.h` — from a YAML
  hardware description or a text prompt, i.e. ebuild is the
  authoring tool for the very configuration surfaces in §6.
- eos's own ADR index records the intended dependency
  direction: ADR-019, "ebuild consumes eos and eBoot as pinned
  dependencies" (`docs/adr/README.md`; status **Proposed** —
  see §10).

### 8.3 eIPC — IPC over eos contracts

- eos's kernel IPC (`kernel/src/ipc.c`, declared in
  `kernel.h`) is the in-OS primitive. The org-level eIPC
  attachment visible *from eos* is the AI bridge (§8.4) — but
  only as **API scaffolding**: `layers/ai/include/eos_ai.h`
  frames eIPC as the intended transport between ENI (input)
  and EAI (agent), with the bridge config carrying an
  `eipc_address` and `eipc_hmac_key`, while the implementation
  sends and receives nothing (§8.4). The working connection
  is planned state (§10).
- A design cross-link, not code: `docs/track1/
  accelerator-hal-profiles.md` cites an eIPC design document
  ("zero-copy tensor IPC") as a companion to the accelerator
  HAL sketch. That is a **planned-state** reference (§10); no
  zero-copy tensor API exists in eos headers today.

### 8.4 eAI / eNI — the AI layer bridge

- The only AI code in eos is `layers/ai/`:
  `eos_ai.h` declares a lifecycle API (`eos_ai_init/start/
  stop/state`), an intent bridge (`eos_ai_send_intent`,
  `eos_ai_send_tool_request`, `eos_ai_receive_result`), and
  config naming the counterpart services (`eni_service_id`,
  `eai_service_id`, `eipc_address`). `layers/ai/src/eos_ai.c`
  is its implementation TU — but the bridge functions are
  success-returning stubs: `eos_ai_send_intent()` returns
  `EOS_AI_OK` having sent nothing (comment: "In full build:
  eni_eipc_bridge_send_intent(...); This stub ..."),
  `eos_ai_send_tool_request()` likewise returns `EOS_AI_OK`
  with its arguments discarded, and `eos_ai_receive_result()`
  writes `buf[0] = '\0'` and returns `EOS_AI_OK`. The bridge
  is therefore an API scaffold, not a live eIPC attachment;
  the working connection is planned state (§10). Header
  placement breaks the `eos/` convention (gap G-6), and
  `eos_ai.h` sits outside every `EOS_ENABLE_*` gate.
- Everything beyond that bridge — inference itself — is
  design material in eos (`docs/track1/
  accelerator-hal-profiles.md`, a self-described "design
  sketch, not yet an ADR") and therefore planned state (§10).

### 8.5 EoSim — simulation

- eos-side evidence is CI-shaped, not code-shaped: eos's
  `.github/` contains a reusable action
  `.github/actions/install-eosim/` and workflows
  `eosim-sanity.yml`, `eos-simulation.yml`,
  `simulation-test.yml` — i.e. eos CI installs and runs
  against EoSim. `sim/` *inside* eos holds target entry stubs
  and simulation harnesses (qemu_arm64, qemu_cortex_m3/m33,
  rpi4, stm32mp1), which are eos's own sim targets, distinct
  from the EoSim repo.
- The one functional contract written down on the eos side is
  `docs/safety/robot-control-contract.md`: it defines the
  on-device robot control + safety contract (kernel scheduler
  facts read from `kernel/src/task.c`) and names its companion
  on the EoSim side (a ROS 2 rover adapter there) as the place
  the contract is first exercised. The contract document
  itself is labelled "contract slice, not code".

### 8.6 eNet, EoStudio, and the rest

- **eNet** is, from eos's evidence, a *name for eos's own
  networking subsystem* (`STATUS.md` lists "Networking (eNet)"
  = `libeos_net.a`) **and** the name of a sibling repo in the
  org (per the org plan, not eos files). Inside eos, eNet ==
  §7.2's net rows. No code in eos imports an external eNet.
- **EoStudio**: no reference establishing a code contract with
  EoStudio was found in the eos files read for this draft.
  Marked **UNVERIFIED / out of scope for this draft** rather
  than described.
- Other org repos (eSec, eDB, eApps, eBrowser, eOffice, …):
  eos's security/filesystem/etc. *services* carry the
  corresponding subsystem names (`STATUS.md` rows are labelled
  e.g. "Security (eSec)"), but no eos file read for this draft
  establishes a code-level contract with those repos. They are
  recorded here as naming correspondence only.

## 9. Testing, examples, and docs (present state)

- **Tests.** `tests/` mixes C unit tests (one `test_*.c` per
  module, run via `ctest` under `EOS_BUILD_TESTS=ON` +
  `EOS_PRODUCT=vbox_test`), fuzz targets (`tests/fuzz/`),
  and Python suites (`tests/unit`, `functional`, `performance`,
  `simulation`, driven by root `run_all_tests.py` / pytest).
  CI is 18 workflows under `.github/workflows/` (20 `.yml` files in master, minus `auto-assign.yml` and `linked-issue.yml` — issue automation, not CI) (build, CI,
  CodeQL, scorecard, release, simulation, cross-repo dispatch,
  docs/book and video builds, nightly/weekly).
- **Examples.** 13 directories; 10 contain `main.c` +
  `CMakeLists.txt` + `README.md` + `eos.yaml`; 3
  (`demo-linux`, `industrial-gateway`, `motor-controller`)
  contain only `eos.yaml`. `STATUS.md` records one example
  verified building/running on host; the other twelve
  unverified.
- **Docs.** `docs/` is the documentation tree (MkDocs,
  `mkdocs.yml` at root; Doxygen via `Doxyfile`). Agent-facing
  entry points: root `llms.txt` + `llms-full.txt` (the latter a
  *hand-maintained* concatenation — its header says to update
  it in the same commit as any source change). `docs/book/`
  holds a book-length manuscript tree and `docs/coverage/`
  holds committed generated coverage HTML — both are part of
  the present docs architecture, for better or worse.
  `mkdocs.yml` itself is stale in places (`repo_url` points
  at `github.com/example/eos`; its nav covers only the
  build-tool docs) — recorded in the coherency review.

---

# Part II — Planned state (nothing below is implemented unless stated)

Every item names its source document in eos. Inclusion here
means "the repo records this intent", never "this works".

## 10. Planned / proposed architecture

- **[PLANNED — source: `docs/adr/ADR-014-tcpip-via-lwip.md`,
  status Proposed]** Adopt **lwIP** as the TCP/IP stack for
  the Nano/Edge (bare-metal) profiles, behind the existing
  `net.h` API; the Compute (host) profile keeps the POSIX
  mapping (already implemented, §7.2). eNet maturity stays
  `Planned` until this lands. Open questions recorded in the
  ADR: lwIP integration mode (bare-metal `NO_SYS` vs RTOS
  mode) and zero-copy buffer ownership.
- **[PLANNED — source: `docs/eos-mqtt-client-design.md`
  (design doc, tracker #199) + in-code markers in
  `net/src/net.c`]** Complete the MQTT client on the codec
  already landed: publish / subscribe / unsubscribe / loop
  ("slice 3") written against the `eos_net` socket API so the
  same code runs on POSIX today and lwIP later. HTTP and CoAP
  are explicitly *separate* adoption decisions in the same
  documents and have no design in the tree yet.
- **[PLANNED — source: `systems/src/image.c`,
  `systems/src/rootfs.c`, `systems/src/firmware.c` scaffold
  admissions (§5)]** A working Linux / hybrid image pipeline:
  kernel build from a kernel source tree (or a real Buildroot
  integration) instead of the scaffold skip, rootfs package
  installation beyond the FHS skeleton, writing kernel and
  rootfs into the image instead of an empty file plus partition
  table, and a hybrid combine step joining the Linux image with
  the per-target firmware outputs. Until then §5's Linux and
  hybrid pipelines are scaffold/partial.
- **[PLANNED — source: `docs/adr/` index, all status
  Proposed]** The ADR programme itself: ADR-012 cryptographic
  provider selection (the recorded direction for replacing the
  hand-written crypto and the RSA/ECC stubs — `STATUS.md`
  names Mbed TLS under ADR-012), ADR-013 devicetree via
  libfdt, ADR-015 vendor HAL adapter policy, ADR-016 board/
  feature description via devicetree + Kconfig, ADR-017
  third-party/SBOM policy, ADR-018 test framework, ADR-019
  ebuild pinned dependencies (§8.2). Note: ADR-001…011 are
  recorded in the index as existing only on an unmerged
  branch; the numbering gap is part of the present state.
- **[PLANNED — source: `docs/track1/
  accelerator-hal-profiles.md`, self-labelled "design sketch,
  not yet an ADR"]** A tiered accelerator HAL (`tiny` /
  `large` profiles, a 3-operation backend vtable sketch:
  dispatch, scratch sizing, quant format) for on-device
  inference, designed jointly with companion documents in
  eAI, eosllm, eIPC, and ebuild (inference service, zero-copy
  tensor IPC, quantize step). No accelerator API exists in
  eos headers today.
- **[PLANNED — source: `layers/ai/src/eos_ai.c` stub comments
  ("In full build: ...") + `layers/ai/include/eos_ai.h`]**
  The working eIPC AI bridge: wire `eos_ai_send_intent()` /
  `eos_ai_send_tool_request()` / `eos_ai_receive_result()` to
  a real eIPC transport between ENI (input) and EAI (agent)
  (`eni_eipc_bridge_*`), so intents and results actually cross
  between the services instead of returning success with
  nothing sent. Until then the bridge is API scaffolding
  (§8.4).
- **[PLANNED — source: `docs/safety/
  robot-control-contract.md`, PROPOSED sections]** The robot
  control + safety contract's proposed values and mechanisms,
  beyond the READ-verified scheduler facts its §2 records.
  The document takes no position on the open compat timeout
  question (eos #202); neither does this one.
- **[PLANNED — source: org direction recorded outside eos;
  NO eos implementation]** The org-wide **MCP layer** (agents
  reaching platform capabilities as tools, per-repo servers
  converging on one gateway) has **no presence in the eos
  tree**: no MCP server, no `server.json`, no MCP code path
  was found in the full tree listing. eos's agent-facing
  surface today is documentary only: `llms.txt` /
  `llms-full.txt`. When an eos MCP server exists, this
  section is where it gets documented — as present state,
  with its tools listed against the §7 API contracts it wraps
  (the standing rule: the API layer is the contract, MCP
  wraps it).
- **[PLANNED — source: `README.md` / `STATUS.md` evidence
  gaps]** Hardware validation of any board, reproducible
  benchmarks (latency/throughput/power), RISC-V and AArch64
  toolchain verification, and end-to-end OTA validation
  against eBoot. All are recorded as missing evidence, not as
  scheduled work with dates.

---

# Appendix — Verification notes

Claims in Part I rest on the files below, read in full or in
the parts cited, via the GitHub API (`contents/` + recursive
tree) at the 2026-11-01 foundation-review snapshot of `master`
(brief head `d5723899`; head moved during the review due to a
concurrent sweep — see the coherency review for the record).

| Claim area | Files read |
|---|---|
| Two halves, build wiring, targets, links, counts | root `CMakeLists.txt`; recursive tree listing (1,058 entries — all counts computed from it); `build.yaml`; `sbom.json`; code search for `EOS_PRODUCT` / `boards` (§1 — 0 hits in half-B dirs / `core/`) |
| RTOS overview, board/product/example counts as *documented* | `README.md`; `AGENTS.md` |
| Maturity states, evidence, stub admissions | `STATUS.md` (measured 2026-08-29 per its header — its HAL-backend row is stale vs current `hal.h`/`hal_common.c`, §4 follows the code) |
| Kernel surface, wait constants | `kernel/include/eos/kernel.h`; `docs/safety/robot-control-contract.md` §1–§2 (for tick/priority facts read there from `kernel/src/task.c`) |
| HAL dispatch, backends, stubs | `hal/include/eos/hal.h`, `hal_extended.h`; `hal/src/hal_common.c`, `hal_linux.c`, `hal_extended_stubs.c`; tree listing for `hal_extended.c` non-inclusion + root `CMakeLists.txt` target sources |
| Net / MQTT / HTTP / mDNS truth table | `net/include/eos/net.h`; `net/src/net.c` (incl. `TODO(#199 slice 3)`); `net/src/net_posix.c`; `docs/eos-mqtt-client-design.md`; `docs/adr/ADR-014-tcpip-via-lwip.md` |
| Config collision, product reachability | `include/eos/eos_config.h` (42 profile branches) vs `products/` tree count (48) vs root `CMakeLists.txt` cache string (41 names) |
| Guard collisions, duplicate headers | `core/include/eos/config.h`; `include/eos/package.h` vs `pkg/include/eos/package.h` (`diff` — diverged); `drivers/include/eos/gps_nmea.h` vs `services/gps/include/eos/gps_nmea.h` (`diff` — incompatible) |
| Driver frameworks | `drivers/include/eos/driver.h` vs `drivers.h`; root `CMakeLists.txt` `eos_drivers` sources |
| Crypto stub posture | `services/crypto/include/eos/crypto.h`; `services/crypto/src/rsa_ecc_sha512.c` (fail-closed guard) |
| Build-tool half | `core/include/eos/types.h`, `error.h`, `config.h`; `include/eos/backend.h`, `quality.h` (API version 0.1.0); previous `docs/architecture.md` (Half-B diagram/tables — carried forward where consistent with the tree); `systems/src/image.c`, `rootfs.c`, `firmware.c` (pipeline scaffold admissions, §5) |
| eBoot / ebuild attachment | `docs/integration-guide.md`; `GETTING_STARTED.md`; `DEPLOYMENT.md` (pinned SHAs noted as historical); `build.yaml`; `docs/adr/README.md` (ADR-019 title/status) |
| eIPC / eAI attachment | `layers/ai/include/eos_ai.h`; `layers/ai/src/eos_ai.c` read in full (bridge functions are success-returning stubs, §8.4); `docs/track1/accelerator-hal-profiles.md` (cross-links) |
| EoSim attachment | Tree listing: `.github/actions/install-eosim/`, `.github/workflows/eosim-sanity.yml`, `eos-simulation.yml`; `docs/safety/robot-control-contract.md` (companion reference) |
| Docs conventions followed by this draft | `docs/architecture.md` (heading style, tables), `docs/getting-started.md`, `docs/api-reference.md`, `docs/integration-guide.md` (title/section/`---` style), `mkdocs.yml` (nav slot) |
| MCP absence in eos | Full recursive tree listing — no `*mcp*` path, no `server.json` in eos |

**Explicitly not verified by this draft:** satellite-repo
internals (eBoot, eIPC, ebuild, EoSim, eAI, eNI, EoStudio —
described only via eos-side evidence, §8); any CI result;
any build or test run; files in `docs/book/` beyond the tree
listing; the content of ADRs 012–019 beyond ADR-014 (read in
full) and the index table in `docs/adr/README.md`.

**Companion document:** the foundation coherency review of
eos (same date, same snapshot) lists the defects behind
gaps G-1…G-8 with severity ratings and the root-structure
assessment. This draft describes the architecture *including*
those gaps; fixing them updates this document by PR.
