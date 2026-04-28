# EmbeddedOS Deployment Guide

## Overview

The EmbeddedOS ecosystem consists of three repositories:

| Repo | Purpose | Commit |
|------|---------|--------|
| **eos** | RTOS kernel, HAL, drivers, services | `a9e1b11` |
| **eBoot** | Bootloader (stage0/stage1, crypto, MPU) | `772f4e2` |
| **ebuild** | Build system (CLI, package mgmt, HW analysis) | `59a9435` |

---

## Prerequisites

### Toolchains
```bash
# ARM Cortex-M/A cross-compiler
sudo apt install gcc-arm-none-eabi

# AArch64 cross-compiler (for Cortex-A targets)
sudo apt install gcc-aarch64-linux-gnu

# RISC-V cross-compiler
sudo apt install gcc-riscv64-linux-gnu

# Host build tools
sudo apt install cmake gcc g++ cppcheck

# QEMU for simulation
sudo apt install qemu-system-arm qemu-system-aarch64 qemu-system-x86 qemu-system-misc

# Python (for ebuild)
sudo apt install python3 python3-pip
```

### Flash Tools (for real hardware)
```bash
# OpenOCD (STM32, nRF, etc.)
sudo apt install openocd

# Or ST-Link tools
sudo apt install stlink-tools

# Or pyOCD
pip install pyocd
```

---

## Quick Start: Build & Test on Host

```bash
# Clone all repos
git clone https://github.com/embeddedos-org/eos.git
git clone https://github.com/embeddedos-org/eBoot.git
git clone https://github.com/embeddedos-org/ebuild.git

# Build eos (host)
cd eos
cmake -B build -DEOS_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure

# Run end-to-end integration test
gcc -std=c11 -Wall -O0 -Ikernel/include -Iinclude \
  -o build/test_e2e tests/test_e2e.c kernel/src/task.c \
  kernel/src/sync.c kernel/src/ipc.c kernel/src/mem/heap.c
./build/test_e2e
# Expected: ALL 88 CHECKS PASSED
```

---

## Cross-Compile for STM32F4 (ARM Cortex-M4)

```bash
cd eos

# Using the build script
bash scripts/build_arm.sh
# Output: _build_arm/eos_system_test.elf (8.4 KB)
# Output: _build_arm/eos_system_test.bin (flashable binary)

# Or using CMake
cmake -B build-arm \
  -DCMAKE_TOOLCHAIN_FILE=toolchains/arm-cortex-m4.cmake \
  -DEOS_BUILD_TESTS=OFF
cmake --build build-arm
```

---

## QEMU Simulation

### Run System Test on QEMU ARM
```bash
cd eos
bash scripts/build_qemu.sh
# Builds and runs on qemu-system-arm (lm3s6965evb)
# Expected output:
#   EoS kernel started
#   ALL SYSTEM TESTS PASSED
```

### Individual QEMU Launch Scripts
```bash
# Cortex-M
bash scripts/qemu/run_cortex_m.sh _build_arm/eos_system_test.elf

# Cortex-A (AArch64)
bash scripts/qemu/run_cortex_a.sh build-aarch64/eos.elf

# x86_64
bash scripts/qemu/run_x86_64.sh build-x86/eos.elf

# RISC-V
bash scripts/qemu/run_riscv.sh build-riscv/eos.elf

# Automated boot test (all architectures)
bash scripts/qemu/boot_test.sh _build_arm
```

---

## Flash to Hardware

### STM32F4 Discovery (OpenOCD + ST-Link)
```bash
# Flash eBoot first
cd eBoot
cmake -B build -DEBLDR_BOARD=stm32f4 \
  -DCMAKE_TOOLCHAIN_FILE=../eos/toolchains/arm-cortex-m4.cmake
cmake --build build
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
  -c "program build/ebldr_stage0.bin 0x08000000 verify reset exit"

# Flash eos kernel
cd ../eos
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
  -c "program _build_arm/eos_system_test.elf verify reset exit"

# Or use ebuild flash
cd ../ebuild
python -m ebuild flash _build_arm/eos_system_test.bin \
  --tool openocd --target stm32f4 --reset-after
```

### nRF52 (nrfjprog)
```bash
nrfjprog --program build/eos.hex --sectorerase --verify
nrfjprog --reset
```

### ESP32 (esptool)
```bash
esptool.py --chip esp32 write_flash 0x10000 build/eos.bin
```

---

## Build eBoot Separately

```bash
cd eBoot

# Host build (for tests)
cmake -B build -DEBLDR_BOARD=none -DEBLDR_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build

# Cross-compile for STM32F4
cmake -B build-arm -DEBLDR_BOARD=stm32f4 \
  -DCMAKE_SYSTEM_NAME=Generic \
  -DCMAKE_C_COMPILER=arm-none-eabi-gcc \
  -DCMAKE_C_FLAGS="-mcpu=cortex-m4 -mthumb -specs=nosys.specs"
cmake --build build-arm
```

---

## Using ebuild (Build System)

```bash
cd ebuild
pip install -e .

# Project info
ebuild info

# Full pipeline for a target board
ebuild pipeline --board stm32f4

# Hardware analysis
ebuild analyze "STM32H7 with CAN and Ethernet"

# Generate project from hardware description
ebuild generate-project --text "nRF52 BLE sensor" --output my-sensor

# Flash firmware
ebuild flash firmware.bin --tool openocd --target stm32f4

# List available packages
ebuild list-packages

# Add a package
ebuild add lwip

# Build with packages
ebuild build
```

---

## End-to-End Boot Chain

```
┌─────────────┐     ┌──────────────┐     ┌─────────────┐
│  eBoot      │     │  eos kernel  │     │  User App   │
│  Stage 0    │────▶│  Init        │────▶│  Tasks      │
│  Stage 1    │     │  Scheduler   │     │  IPC/Sync   │
│  Crypto     │     │  HAL/Drivers │     │  Services   │
│  MPU Setup  │     │  Network     │     │  Filesystem │
└─────────────┘     └──────────────┘     └─────────────┘

Boot sequence:
1. eBoot stage0 → hardware init, clock config
2. eBoot stage1 → verify firmware (AES-GCM), configure MPU
3. eBoot → pass boot params to shared memory (eos_boot_params_t)
4. eBoot → jump to eos kernel entry point
5. eos kernel_init() → create idle task, init scheduler
6. eos kernel_start() → SysTick + PendSV preemptive scheduling
7. User tasks run with mutex/semaphore/queue synchronization
```

---

## Supported Architectures

| Architecture | Port Files | Status |
|-------------|-----------|--------|
| ARM Cortex-M (Thumb) | `kernel/src/arch/arm_cm/` | Production-ready, QEMU-tested |
| ARM Cortex-A (AArch64) | `kernel/src/arch/arm_ca/` | Scaffold — needs hardware testing |
| x86_64 | `kernel/src/arch/x86_64/` | Scaffold — needs hardware testing |
| RISC-V (RV64) | `kernel/src/arch/riscv/` | Scaffold — needs hardware testing |

---

## Supported Hardware

| Board | MCU | eBoot | eos | ebuild |
|-------|-----|-------|-----|--------|
| STM32F4 Discovery | STM32F407VG | ✅ | ✅ | ✅ |
| STM32H7 Nucleo | STM32H743 | ✅ | ✅ | ✅ |
| nRF52840-DK | nRF52840 | ✅ | ✅ | ✅ |
| Raspberry Pi 4 | BCM2711 | ✅ | Scaffold | ✅ |
| QEMU lm3s6965evb | Cortex-M3 | N/A | ✅ Tested | N/A |
| QEMU virt (AArch64) | Cortex-A57 | N/A | Scaffold | N/A |

---

## CI/CD

GitHub Actions workflows are configured for all 3 repos:

- **eos**: Host build + ARM/AArch64/RISC-V cross-compile + cppcheck + clang-tidy
- **eBoot**: Host tests + STM32F4 cross-compile + cppcheck
- **ebuild**: Python 3.10/3.11/3.12 + flake8 lint + pytest + CLI smoke test

Triggered on push to `main` and pull requests.
