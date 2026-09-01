# EoS — Feature Maturity

§25 of the platform architecture document requires every significant feature to carry a
maturity state with the evidence that state demands. This file records them for `eos`.

Measured on **29 Aug 2026** against this branch. Every row below cites a command that was
run, or says plainly that it was not.

## States and the evidence each requires (§25)

| State | Definition | Required evidence |
|---|---|---|
| Planned | Design intent; not yet available | Roadmap / issue / design spec |
| Experimental | Working prototype; API/behaviour may change | Tests + known limitations |
| Implemented | Feature exists and is usable | Code + functional tests |
| Validated | Verified on named targets/use cases | Test reports / CI / hardware matrix / benchmarks |
| Certified | Formally certified to a named standard | Certificate / audit reference |

`Unknown` appears below where evidence was not collected. It is **not** a §25 state — it
is an admission of an unmeasured claim, and should be driven to zero.

## Repository-level

| | |
|---|---|
| **Overall** | **Experimental** |
| Host evidence | CMake 4.2.3 / GCC 15.2.0; `ctest` **28/28 pass** |
| Cross evidence | `toolchains/arm-cortex-m4.cmake` builds 23 static libraries; artifacts report `architecture: armv7e-m`. Footprint **15.8 KB flash, 26 KB RAM** (`arm-none-eabi-size -t`) |
| Blocking Implemented | No board-level or on-hardware evidence. Nothing has been observed to boot. |

## Subsystem detail

| Subsystem | Builds as | State | Evidence / gap |
|---|---|---|---|
| Kernel (task, sync, ipc, multicore, heap) | `libeos_kernel.a` | Experimental | Builds host and Cortex-M4; `test_kernel` passes. Mutex priority inheritance is transitive and recomputed from an invariant. No preemption or latency measurement. |
| HAL — dispatch layer | `libeos_hal.a` | Experimental | `hal_common.c` dispatches to a registered backend; `test_drivers` passes. |
| HAL — host backend | (in `libeos_hal.a`) | **Planned** | `hal_linux.c` implements a Linux backend, but `eos_hal_linux_register()` is **never called and is declared in no public header**. On a host build no backend registers, `eos_hal_init()` returns `-1`, and every HAL call silently no-ops. |
| HAL — extended peripherals (ADC, PWM, I²C, SPI, …) | (in `libeos_hal.a`) | **Planned** | `hal_extended_stubs.c` is 740 of the HAL's 3,185 lines and contains 144 `(void)param;` casts. The API exists; the peripherals do not. |
| Architecture — ARM Cortex-M4 / R5 | 23 `.a` targets | Experimental | Cross-compiles clean via `arm-cortex-m4`, `arm-none-eabi-stm32f4` and `arm-none-eabi-r5`; artifacts verified `armv7e-m` / `armv7` with `objdump -f`. **Never executed** — no board, no QEMU. |
| Architecture — RISC-V, Cortex-A/AArch64 | — | **Unknown** | Toolchain files exist (`riscv64-linux-gnu.cmake`, `aarch64-linux-gnu.cmake`). Neither toolchain was installed, so neither was built. **NOT RUN.** |
| Boards | `boards/*.yaml` | **Planned** | 84 descriptors, of which **71 are named `generic-*`**. A descriptor is not a port; none has been validated on hardware. |
| Drivers + devicetree | `libeos_drivers.a`, `libeos_devicetree.a` | Experimental | Build; `test_drivers` and `test_devicetree` pass. The DTB parser is bounds-checked against untrusted input and has a fuzz target. Per-driver status unassessed. |
| Networking (eNet) | `libeos_net.a` | **Planned** | Per **ADR-014**. `net/src/net.c` is 190 lines carrying 20 `(void)param;` casts; `eos_net_connect()` is `return -1`. There is no protocol implementation of any kind. `test_net` passes because it tests the facade's argument validation, not a connection. §12 names TCP/IP, UDP, DHCP, DNS, MQTT, CoAP, HTTP and WebSocket — **none exist**. |
| Security (eSec) — keystore, ACL | `libeos_security.a` | Experimental | Builds; `test_security` passes. |
| Security (eSec) — RSA / ECC signatures | (in `libeos_crypto.a`) | **Planned** | Per **ADR-011 / ADR-012**. The RSA and ECC routines are stubs: `eos_ecc_verify()` accepted any 64-byte signature and `eos_rsa_verify_sha256()` compared only the trailing 32 bytes. They now refuse to run unless a build defines `EOS_ALLOW_STUB_CRYPTO`, and `test_crypto_failclosed` asserts that refusal. **No signature verification exists.** |
| Security (eSec) — AES, SHA-256, SHA-512 | (in `libeos_crypto.a`) | Experimental | Build; `test_crypto`, `test_crypto_aes`, `test_crypto_sha512` and `test_crypto_vectors` pass, including NIST vectors. Hand-written; **not independently audited**, which §4 discourages. ADR-012 selects Mbed TLS to replace them. |
| Power management | `libeos_power.a` | Experimental | Builds; `test_power` passes. Peripheral IDs are bounds-checked against negative values. |
| Filesystem | `libeos_filesystem.a` | Experimental | Builds; `test_filesystem` passes. No power-loss or wear-levelling evidence — the properties that matter most, and the ones unit tests cannot establish. |
| OTA | `libeos_ota.a` | Experimental | Builds; `test_ota` passes. `eos_ota_apply()` refuses without a registered authenticator, so an image's own declared hash is no longer treated as proof of origin. **Not validated end-to-end against eBoot.** |
| Packaging | `libeos_pkg.a`, `libeos_systems.a` | Experimental | Build; `test_package` and `test_config` pass. The config parser is bounds-checked against a package count exceeding `EOS_MAX_PACKAGES` (AddressSanitizer clean on the input that previously overflowed). |
| Debug (GDB stub, core dump) | `libeos_debug.a` | Experimental | Builds; `test_debug` passes. `M`-packet payloads are length- and charset-validated. `eos_gdb_read_mem`/`write_mem` still cast a `uint32_t` to a pointer, so the accept path is untested on 64-bit hosts. |
| POSIX compat adapter | `libeos_compat.a` | Experimental | Builds; `test_os_adapter` passes. |
| Examples (13 apps) | not in the root build | Experimental | `examples/blink-gpio` builds standalone and runs on the host. The remaining twelve are **unverified**. |

## Claims that must not be made until evidence exists

- That any board or architecture is *supported* beyond a `boards/` descriptor and a clean
  cross-compile. Nothing here has been observed to boot.
- Any latency, throughput, power or boot-time figure. §25 requires a link to a reproducible
  benchmark; no benchmark methodology is published. The footprint numbers above are the one
  exception — `arm-none-eabi-size -t` is reproducible and the command is given.
- Any comparison against another OS.
- Real-time or deterministic guarantees — no scheduling latency measurement exists.
- That EoS has networking or signature verification. Both are `Planned`.
