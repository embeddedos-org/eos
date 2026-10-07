# Changelog

## [Unreleased]

### Security
- **Build backends no longer hand configuration to a shell unchecked, and never run a truncated command.** All eleven backends under `backends/` built their `system()` lines with `snprintf` from the project's `src_dir`, `build_dir`, `install_dir`, toolchain and option values, quoted but unfiltered: a value carrying `"; touch pwned; "` ran as shell (reproduced against the CMake backend's `clean`), and `freertos_install`, `cargo_install` and `nuttx_install` formatted three `EOS_MAX_PATH` paths into a 1024-byte buffer, so a long path ran a cut-off line. Commands are now assembled through `EosShellCmd` (`core/include/eos/shell_cmd.h`): program text written in the tree is appended as text, every configuration value goes through `eos_shell_cmd_arg()` (double-quoted, refused if it carries a shell metacharacter or control character) or `eos_shell_cmd_word()` (unquoted, an allowlist of one word), and a line that does not fit or carries a refused value is refused with `EOS_ERR_INVALID` before anything runs. Option values are quoted, so a value with a space is one argument. The two rules are the ones `linux_security` reviewed and shipped, moved to `core` and used by both; `linux_security`'s own predicates delegate to them. `eos_shell_cmd_run()` reports a shell that could not be started, or a command the shell could not run at all (exit 127), as `EOS_ERR_SYSTEM` rather than as the `EOS_ERR_BUILD` a genuine build failure produces. `tests/test_shell_cmd.c` covers the rules, the builder, and every backend against a hostile directory.

### Changed
- **`eos_task_delete(0)` and `eos_task_suspend(0)` return `EOS_KERN_INVALID`.** Handle 0 is the idle task and it is permanent: it is the task the scheduler falls back to when nothing else is runnable. This restores the scheduler contract #131 established and #130's replay rolled back -- `kernel/src/task.c` on master was #130's copy, in which the idle task could be deleted or suspended, `eos_task_create()` published a TCB to the scheduler before its stack existed, and `eos_schedule()` left `g_current_sp` pointing at the outgoing task. The file is #131's again, plus #130's `wake_armed` hunk.

### Fixed
- **GDB stub: a packet of the advertised size is accepted, and a refused packet does not end the session.** `qSupported` announced `PacketSize=400` (1024 bytes) over a 512-byte receive buffer, so GDB was told it could send packets the stub then NACKed every time; and `recv_packet()` stopped reading at the buffer's end and took the next two body bytes for the checksum, leaving the stream out of step. The size is now derived from the buffer (`PacketSize=1ff`), a body that does not fit is drained to its `#` and refused with `-`, and the dispatch loop distinguishes a refused packet (GDB resends it) from a dead transport -- it used to exit on the first corrupt or oversized packet. The drain is bounded: a body that never reaches its `#` is abandoned after another `PacketSize` bytes with a `-`, where the first version of this change read it forever. The `m`/`M` memory cap is derived from the packet size too (`GDB_MEM_MAX`, 248 bytes) instead of a literal 128, so a bulk transfer GDB sizes from `PacketSize` is not answered `E01`. `tests/test_debug.c` covers a packet of exactly the advertised size, one 40 bytes longer, one with a bad checksum, a body that never ends, and an `M` one byte past the cap, each followed by a `D` that must still be answered; `tests/unit/test_gdb_stub_limits.py` pins the derivations in the source.
- **Master did not compile after the 09-08 batch merge.** Nine PRs were merged within minutes, each on the base it was written against. #119 and #132 each added an `#else` to the same `#ifndef _WIN32` in `eos_busybox_install_to_rootfs()`, so `linux_security.c` read "#else after #else" and every C job, CodeQL and the simulation's ARM64 kernel failed to compile -- before the Python guards that would have named the rest could run. The duplicate arm is removed. Behind it: `test_linux_security_paths` (#119) and `test_pkg_fetch` (#115), whose `add_executable()` blocks later replays had overwritten, are registered again (41 to 43 suites); four `test_kernel` tests from #130 and #131 that were defined but never called are wired into `main()`; `windows-test` (#132) is in the CI gate's `needs`, so the gate cannot be green with the MSVC leg red; and `eosim-sanity.yml` and `simulation-test.yml` (#129), whose `pull_request` trigger is path-filtered and so cannot be a required check, are classified in `test_ci_gate.py`'s `NOT_REQUIRED`.
- **`eos_busybox_install_to_rootfs()` refuses `NULL`:** it read `bb->source_dir` to hand it to `is_path_safe()`, so a `NULL` `bb` was a crash rather than a -1. Both arguments are now refused before either is dereferenced.
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
- **`.eapp` header name fields are path components, not paths.** `eos_pkg_install()` wrote the binary to `apps_dir/<package_id>/<name>` with both fields taken raw from the header, which the signature does not cover (it signs the binary alone): a genuinely signed package with `package_id` of `../escape` installed a 0755 binary outside `apps_dir`, a 64-byte field with no terminator was printed and joined into paths past the end of the header struct, and since `eos_pkg_remove()` hands `install_path` to `rm -rf "..."` through `system()`, a quote in `package_id` reached a shell. `verify`, `install` and `update` now refuse a `name` or `package_id` that is not a terminated, non-empty string of `[A-Za-z0-9._-]` other than `.` and `..`; in `verify` the check runs after the signature so an unsigned package is still reported as unsigned; in `install` it runs before anything is printed; `update` validates the header it read itself and verifies before it looks the id up. The package database is held to the same rules on load: a record whose names are not names, or whose `install_path` is not the directory `install` builds, is dropped with a message rather than acted on, so a record an earlier version wrote from an unvalidated header cannot reach `remove` or `stop`. And nothing in `eos_pkg.c` reaches a shell any more: removal is a directory walk in C on directory descriptors (`unlinkat`/`openat` with `O_NOFOLLOW`, so nothing is checked and then trusted, and a symlink is removed rather than followed) that refuses a path other than the package's own install directory, and `stop` hands the name to `pkill`/`taskkill` as an argv element. Pinned by `tests/test_pkg_install_paths.c`.

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
