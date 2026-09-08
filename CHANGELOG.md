# Changelog

## [Unreleased]

### Fixed
- **`linux_security` shell commands:** `is_path_safe()` was a denylist of shell metacharacters that missed the backtick and the newline, was applied at 2 of the 9 call sites that build a command, and returned "safe" for `NULL`. A path of the form `/tmp/<backtick>id<backtick>` reached `system()` and ran `id`. The predicate now rejects control characters, backslash, backtick and newline, refuses `NULL`, and guards every shell-building entry point. `eos_busybox_configure()` validated `source_dir` *before* filling in its default, so the default path -- the one a caller gets by not setting the field -- validated the empty string and handed `.eos/build/src/busybox-<version>` to the shell unchecked; the fill now happens first, and `eos_busybox_set_version()` refuses a version that is not one shell word.
- **One shell word is an allowlist.** `defconfig` and `cross_compile` are interpolated unquoted. The predicate guarding them was a second denylist (space, tab, `*`, `?`, `~`) that missed `[`/`]` and `{`/`}`, and accepted a leading `-`. It is now `isalnum()` plus `._/+=:-` and nothing else.
- **Security steps no longer report success without having run.** `eos_ima_sign_file()`, `eos_selinux_install_to_rootfs()`, `eos_selinux_label_rootfs()`, `eos_ima_install_to_rootfs()` and `eos_busybox_install_to_rootfs()` ended their commands with `|| true` or `|| echo …` and discarded `system()`'s result, so a missing `evmctl` or `setfiles`, a failed policy or key copy, and a `make install` that never ran all returned 0. Each now distinguishes a step that completed from one that could not, logs through `EOS_ERROR` and returns -1. **Contract change:** these five functions previously always returned 0; no caller in this tree relies on that.
- **`eos_busybox_configure()` / `eos_busybox_build()`:** `offset += snprintf(...)` accumulated the would-be length, so a truncating segment would have put the write pointer past the end of the buffer and underflowed the remaining size. Unreachable at the current field widths; now checked rather than inferred.
- **`eos_pkg` trust anchor:** Package signatures were checked against `eos_pkg_public_key[32] = {0}`. #99 stopped that all-zero key accepting every package, which left it rejecting every package -- including correctly signed ones -- while reporting `signature verification failed`, blaming the package for a key that was never provisioned. The key now comes from `eos_pkg_set_trust_anchor()` or from `EOS_PKG_TRUST_ANCHOR_HEX` at build time; with neither, verification refuses and says so, unless the build defines `EOS_ALLOW_UNSIGNED_PKG`. Closes the second half of #98.
- **`services/pkg` is compiled.** `services/pkg/eos_pkg.c` was in no `CMakeLists.txt`, so no target built it and no test could reach it -- which is how the all-zero anchor survived. It is now the `eos_eapp` library, with `tests/test_pkg_trust_anchor.c` covering it.
- **`ed25519_public_key_is_usable()`:** #99's subgroup and identity checks are exported, so a stored trust anchor can be rejected when it is configured rather than once per package as an apparent signature failure.
- **`eos_queue_send` / `eos_queue_receive`:** A full send/recv waiter table now returns `EOS_KERN_NO_MEMORY` instead of blocking a task that can never be woken, matching mutex and semaphore behavior.
- **`eos_queue_create`:** Size check now uses division so `item_size * capacity` cannot wrap `size_t` on 32-bit targets and overflow the 1024-byte queue store.
- **`eos_sem_create`:** Reject `initial > max` and `max` values that do not fit in `int32_t`, so the counting-semaphore invariant cannot be created already broken.
- **`eos_mutex_lock`:** Recursive lock returns `EOS_KERN_FULL` at `uint8_t` saturation instead of wrapping `rec_count` to 0 and leaving the mutex stuck.
- **`eos_mutex_lock`:** A waiter that times out no longer leaves the mutex owner permanently boosted. The owner's effective priority is recomputed from its base priority and the remaining waiters, so the boost propagates transitively along the blocking chain and is withdrawn when a waiter leaves. A full waiter table returns `EOS_KERN_NO_MEMORY` without applying a boost.
- **`eos_dt_parse`:** Bounds-check the flattened device tree blob before dereferencing it. A malformed DTB could previously read outside the buffer four different ways: `off_struct` past the end of the blob (the `size - off_struct` bound wrapped), a node name with no terminator (unbounded `strlen`), a property name offset outside the strings block, and nesting deeper than the 32-entry node stack. All are now rejected.
- **`eos_dt_find_compatible`:** Search within `prop->len` instead of calling `strstr()` on a property value that is not guaranteed to be NUL-terminated.
- **`eos_dt_get_irq`:** Reject a negative index, and one large enough that `index * 4` overflows, before it is used as an offset.

### Added
- `tests/test_linux_security_paths.c` — the inputs `linux_security` must refuse: 11 shell payloads per entry point, 12 strings that are not one shell word (with 16 real values that must still be accepted), one hostile `rootfs_dir` per `*_install_to_rootfs`, and the absent-tool and failed-copy cases for every security step. Refusal is asserted by side effect -- a sentinel file a successful injection would create -- rather than by return code, because these functions have several ways to return -1 and only one of them means the guard caught it.
- `tests/test_devicetree.c` — device tree parser suite: happy path, malformed-blob rejection, a sweep over every truncation of a valid blob, and a single-byte corruption sweep.
- `tests/fuzz/` is now wired into the build. The directory was never added by any `add_subdirectory`, so no fuzz target could be built. `fuzz_devicetree` is enabled and calls the real `eos_dt_parse()` API — it previously declared a non-existent `eos_dtb_parse()` and was commented out.

## [3.0.1] - 2026-05-16

### Production Release — Unified EmbeddedOS-org v3.0.1

This is the synchronized production release across all 18 EmbeddedOS-org repos.

- Refreshed governance: LICENSE, NOTICE, CITATION.cff, SECURITY.md
- CI/CD pipelines hardened: release.yml, book-build.yml, video-build.yml, deploy-pages.yml
- Release artifacts produced for: Linux x64/arm64, macOS x64/arm64, Windows x64, Docker, plus per-repo embedded/mobile/extension targets
- mdBook documentation built and deployed to GitHub Pages
- Promo video rendered and attached as a release asset

## [3.0.0] - 2026-05-13

### Production Release — Unified EmbeddedOS-org v3.0.0

This is the synchronized production release across all 18 EmbeddedOS-org repos.

- Refreshed governance: LICENSE, NOTICE, CITATION.cff, SECURITY.md
- CI/CD pipelines hardened: release.yml, book-build.yml, video-build.yml, deploy-pages.yml
- Release artifacts produced for: Linux x64/arm64, macOS x64/arm64, Windows x64, Docker, plus per-repo embedded/mobile/extension targets
- mdBook documentation built and deployed to GitHub Pages
- Promo video rendered and attached as a release asset

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.5.0] — 2026-03-27

### Added
- **Firmware build pipeline:** End-to-end firmware assembly from source to deployable image
- **Build scheduler:** Parallel build orchestration with dependency-aware caching
- **`backend.h`:** Unified platform backend abstraction header for Linux and RTOS targets
- **`package.h`:** Package metadata and dependency declaration header for modular builds
- **UI module:** Optional LVGL-based UI service for display-equipped products (`EOS_ENABLE_UI`)
- **CI tests enabled:** Unit test suites now run automatically in CI across all 3 platforms
- **Multicore SMP/AMP:** Enhanced multicore scheduling with per-core load balancing
- **41 product profiles:** Full coverage across automotive, medical, aerospace, consumer, industrial, networking, financial, server, and HMI
- **33 HAL peripherals:** Complete hardware abstraction layer with conditional compilation
- **Cross-compilation toolchains:** CMake toolchain files for AArch64 Linux (`aarch64-linux-gnu`), ARM hard-float (`arm-linux-gnueabihf`), and RISC-V 64 (`riscv64-linux-gnu`)
- **Multi-arch release workflow:** Automated cross-compiled binary releases for 4 architectures (x86_64, AArch64, ARM hard-float, RISC-V)
- **CI/CD pipeline:** GitHub Actions workflows for CI (ubuntu, windows, macos + 6 product builds) and release automation

### Fixed
- **`datacenter.h/c`:** Replaced GCC-only `__builtin_popcount()` with portable `eos_popcount32()` — was breaking all MSVC/Windows builds
- **`os_services.c`:** Fixed hardcoded `/tmp/` path with `_WIN32` guard using `%TEMP%` — OTA downloads were failing on Windows
- **`hal_extended.h`:** Fixed wrong type `eos_imu_data_t` → `eos_imu_vec3_t` in `eos_hal_ext_backend_t` — was breaking all product builds
- **`motor_ctrl.h`:** Added missing `#include <stddef.h>` for `size_t` — was failing on ubuntu, macos, and product builds (robot, automotive)

## [0.1.0] — 2026-03-26

### Added
- **HAL:** 33 peripheral interfaces (GPIO, UART, SPI, I2C, Timer, ADC, DAC, PWM, CAN, USB, Ethernet, WiFi, BLE, Cellular, NFC, IR, Camera, Audio, Display, HDMI, GPU, GNSS, IMU, Radar, Motor, Haptics, Flash, SDIO, RTC, DMA, Watchdog, Touch, PCIe)
- **Kernel:** Task management, mutex, semaphore, message queue, software timers, multicore SMP/AMP
- **Driver framework:** Probe/remove lifecycle, power management hooks
- **Services:** Crypto (SHA-256/512, AES, RSA, ECC, CRC), security (keystore, ACL, secure boot), OS services (watchdog, audit, secure storage, integrity), OTA updates, filesystem, sensor framework, motor control with PID, datacenter (virtualization, BMC/IPMI, RAID, thermal, load balancer, routing, QoS, failover)
- **Compatibility layers:** POSIX threads/sync/signals/IO, VxWorks tasks/semaphores/watchdog/message queues, Linux IPC (SysV shared memory, semaphores, message queues)
- **41 product profiles** covering automotive, medical, aerospace, consumer, industrial, networking, financial, server, and HMI categories
- **Platform backends:** Linux (sysfs/ioctl) and RTOS (register-level)
- **Power management:** Sleep/deep-sleep/standby state machine
- **Networking:** Socket abstraction layer
- **Systems:** Firmware assembly, rootfs generation, system image builder
- **Toolchain management:** YAML-based toolchain definitions with runtime parser
