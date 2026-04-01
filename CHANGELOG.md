# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-03-31

### Added
- **HAL:** 33 peripheral interfaces (GPIO, UART, SPI, I2C, Timer, ADC, DAC, PWM, CAN, USB, Ethernet, WiFi, BLE, Cellular, NFC, IR, Camera, Audio, Display, HDMI, GPU, GNSS, IMU, Radar, Motor, Haptics, Flash, SDIO, RTC, DMA, Watchdog, Touch, PCIe)
- **Kernel:** Task management, mutex, semaphore, message queue, software timers, multicore SMP/AMP
- **Driver framework:** Probe/remove lifecycle, power management hooks
- **Services:** Crypto (SHA-256/512, AES, RSA, ECC, CRC), security (keystore, ACL, secure boot), OS services (watchdog, audit, secure storage, integrity), OTA updates, filesystem, sensor framework, motor control with PID, datacenter (virtualization, BMC/IPMI, RAID, thermal, load balancer, routing, QoS, failover)
- **Compatibility layers:** POSIX threads/sync/signals/IO, VxWorks tasks/semaphores/watchdog/message queues, Linux IPC
- **41 product profiles** covering automotive, medical, aerospace, consumer, industrial, networking, financial, server, and HMI categories
- **Platform backends:** Linux (sysfs/ioctl) and RTOS (register-level)
- **Firmware build pipeline:** End-to-end firmware assembly from source to deployable image
- **Build scheduler:** Parallel build orchestration with dependency-aware caching
- **UI module:** Optional LVGL-based UI service for display-equipped products (`EOS_ENABLE_UI`)
- **Multicore SMP/AMP:** Enhanced multicore scheduling with per-core load balancing
- **Cross-compilation toolchains:** CMake toolchain files for AArch64, ARM hard-float, and RISC-V 64
- Complete CI/CD pipeline with nightly, weekly, EoSim sanity, and simulation test runs
- Full cross-platform support (Linux, Windows, macOS)
- ISO/IEC standards compliance documentation
- MIT license

[0.1.0]: https://github.com/embeddedos-org/eos/releases/tag/v0.1.0
