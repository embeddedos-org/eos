# Changelog

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
