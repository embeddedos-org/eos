# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.0.0] — 2024-12-01

### Added

#### eos (Embedded OS)
- **HAL:** 33 peripheral interfaces (GPIO, UART, SPI, I2C, Timer, ADC, DAC, PWM, CAN, USB, Ethernet, WiFi, BLE, Cellular, NFC, IR, Camera, Audio, Display, HDMI, GPU, GNSS, IMU, Radar, Motor, Haptics, Flash, SDIO, RTC, DMA, Watchdog, Touch, PCIe)
- **Kernel:** Task management, mutex, semaphore, message queue, software timers
- **Multicore:** SMP/AMP support, spinlocks, IPI, shared memory, core affinity
- **Services:** Crypto (SHA-256/512, AES, RSA, ECC, CRC), OTA updates, filesystem, sensor framework, motor control, security (keystore, ACL), OS services (watchdog, audit)
- **Compatibility layers:** POSIX, VxWorks, Linux IPC
- **41 product profiles** covering automotive, medical, aerospace, consumer, industrial, networking, financial, server, and HMI categories
- **7 example applications** with full source code
- **Platform backends:** Linux (sysfs/ioctl) and RTOS (register-level)
- **Toolchain support:** ARM, ARM64, RISC-V, Xtensa, x86, SPARC

#### eboot (Bootloader)
- Multi-stage boot: Stage-0 → Stage-1
- Secure boot with RSA/ECC signature verification
- A/B slot management with rollback
- 23 board support packages
- Configurable flash layout via YAML

#### ebuild (Build System)
- CLI: build, clean, configure, info, install, add, list-packages, firmware, system, analyze, generate-project, generate-boot, new
- Build backends: Ninja, CMake, Make, Meson, Cargo, Kbuild
- Package management with recipe-based dependency resolution
- EoS AI hardware analyzer (no API key needed)
- Project scaffolding with `ebuild new` and 5 templates

#### Documentation
- Unified README.md with quick start and decision tree
- GETTING_STARTED.md with 3 hardware paths
- Board-specific quickstart guides (nRF52, STM32, RPi4, host)
- Integration guide for eos + eboot + ebuild workflow
- API reference, product profile guide, troubleshooting FAQ
