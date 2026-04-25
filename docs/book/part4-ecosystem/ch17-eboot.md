# Chapter 17: eBoot — The EoS Bootloader

*Author: Srikanth Patchava & EmbeddedOS Contributors*

---

## 17.1 Introduction

Every embedded system begins its life in the bootloader. Before the RTOS kernel
initializes, before drivers enumerate peripherals, and before applications run,
the bootloader establishes the foundation of trust and control that makes
everything else possible.

**eBoot** is the production-grade boot platform for EoS. It supports **83 board
ports** across **73 architecture families**, with clean separation between boot
logic, hardware abstraction, and firmware management. eBoot handles everything
from the first instruction after reset to handing control to the operating
system — including secure boot verification, firmware updates, and automatic
rollback on failure.

---

## 17.2 Architecture Overview

eBoot uses a two-stage boot architecture:

```
┌──────────────────────────────────────────────────────┐
│  Reset Vector                                         │
│  ┌────────────┐    ┌────────────┐    ┌─────────────┐ │
│  │  Stage-0   │───►│  Stage-1   │───►│  EoS Kernel │ │
│  │  Minimal   │    │  Full boot │    │  Application│ │
│  │  HW init   │    │  logic     │    │  Runtime    │ │
│  └────────────┘    └────────────┘    └─────────────┘ │
└──────────────────────────────────────────────────────┘
```

### Stage-0: Minimal Hardware Initialization

Stage-0 runs directly from the reset vector. Its responsibilities are minimal
and architecture-specific:

- CPU clock configuration
- Stack pointer initialization
- Minimal memory controller setup (if external RAM is needed)
- Verification and jump to Stage-1

Stage-0 is intentionally small (typically under 4 KB) to minimize the attack
surface and to fit in on-chip ROM or protected flash regions.

### Stage-1: Full Boot Logic

Stage-1 contains the bulk of eBoot functionality:

- A/B slot management and boot policy evaluation
- Secure boot chain verification (SHA-256, CRC-32, Ed25519 stubs)
- Firmware update pipeline (stream-based)
- Boot configuration storage (in flash or EEPROM)
- Handoff to the operating system

---

## 17.3 Key Features

| Category            | Capabilities                                                      |
|---------------------|-------------------------------------------------------------------|
| **Boot Management** | Staged boot (stage-0 + stage-1), A/B slots with automatic rollback, boot policy engine |
| **Secure Boot**     | Self-contained SHA-256, CRC-32, Ed25519 signature stubs, anti-rollback counters |
| **Firmware Update** | Stream-based update pipeline, delta updates, integrity verification |
| **Board Support**   | 83 board ports across ARM Cortex-M/A/R, RISC-V, Xtensa, x86, and more |
| **Integration**     | Seamless integration with EoS kernel and ebuild build system |
| **Diagnostics**     | Boot log, failure counters, watchdog integration |

---

## 17.4 Building eBoot

### Native Build (Host Testing)

```bash
git clone https://github.com/embeddedos-org/eboot.git
cd eboot

cmake -B build -DEBLDR_BUILD_TESTS=ON
cmake --build build
cd build && ctest
```

### Cross-Compilation for STM32F4

```bash
cmake -B build-arm -DEBLDR_BOARD=stm32f4 \
  -DCMAKE_TOOLCHAIN_FILE=toolchains/arm-none-eabi.cmake
cmake --build build-arm
```

### Flashing with eFlash

eBoot includes a unified flashing tool written in Python:

```bash
python tools/eflash.py flash \
  --board stm32f4 \
  --image build-arm/eboot_firmware.bin \
  --verify --reset
```

Alternatively, use the CMake flash target:

```bash
cmake --build build-arm --target flash
```

---

## 17.5 A/B Slot Management

eBoot implements A/B (dual-slot) firmware management, a pattern proven in
Android and ChromeOS. The system maintains two complete firmware images and
tracks which slot is active.

### Slot State Machine

```
        ┌───────────────────────────────────────┐
        │           Boot Policy Engine          │
        │                                       │
        │  ┌─────────┐       ┌─────────┐       │
        │  │ Slot A   │       │ Slot B   │       │
        │  │ Active   │◄─────►│ Standby  │       │
        │  │ Verified │       │ Updated  │       │
        │  └─────────┘       └─────────┘       │
        │       ▲                   │           │
        │       │   Rollback on     │           │
        │       └───boot failure────┘           │
        └───────────────────────────────────────┘
```

### Boot Policy Rules

1. **Try active slot first** — if verification passes, boot it
2. **Fall back to standby** — if active fails integrity or crash counter exceeds threshold
3. **Automatic rollback** — if a newly updated slot fails three consecutive boots, revert
4. **Anti-rollback** — monotonic counter prevents downgrade attacks

### Configuration Storage

Boot metadata is stored in a reserved flash sector:

```c
typedef struct {
    uint8_t  active_slot;       // 0 = A, 1 = B
    uint8_t  boot_attempts[2];  // per-slot failure counters
    uint32_t rollback_counter;  // anti-rollback monotonic counter
    uint32_t crc32;             // metadata integrity check
} eboot_metadata_t;
```

---

## 17.6 Secure Boot Chain

eBoot implements a progressive trust chain:

1. **Stage-0 verifies Stage-1** — CRC-32 integrity check (fast, minimal code)
2. **Stage-1 verifies firmware** — SHA-256 hash comparison against signed manifest
3. **Ed25519 signature stubs** — pluggable public-key verification for production deployments

### Hash Verification Example

```c
#include "eboot/crypto.h"

int verify_firmware(const uint8_t *image, size_t len,
                    const uint8_t expected_hash[32]) {
    uint8_t computed[32];
    eboot_sha256(image, len, computed);
    return eboot_secure_compare(computed, expected_hash, 32);
}
```

The cryptographic primitives are self-contained — eBoot does not depend on
external libraries. This is critical for the boot environment where the OS
and its libraries are not yet available.

---

## 17.7 Firmware Update Pipeline

eBoot supports stream-based firmware updates, enabling over-the-air (OTA)
updates even on memory-constrained devices.

### Update Flow

```
┌──────────┐    ┌───────────┐    ┌──────────┐    ┌──────────┐
│ Download  │───►│ Verify    │───►│ Write to │───►│ Update   │
│ stream    │    │ integrity │    │ standby  │    │ metadata │
│ (chunks)  │    │ per-chunk │    │ slot     │    │ & reboot │
└──────────┘    └───────────┘    └──────────┘    └──────────┘
```

Key design decisions:

- **Chunk-based processing** — firmware is received and verified in chunks,
  never requiring the full image in RAM
- **Power-fail safe** — interrupted updates leave the active slot untouched
- **Verify-before-commit** — the standby slot is fully written and verified
  before updating the boot metadata

---

## 17.8 Board Port Structure

Each board port provides a minimal set of functions that eBoot needs:

```
boards/
├── stm32f4/
│   ├── board.c          # Clock, GPIO, flash driver
│   ├── board.h          # Pin definitions, memory map
│   └── linker.ld        # Linker script
├── nrf52840/
│   ├── board.c
│   ├── board.h
│   └── linker.ld
├── rpi4/
│   ├── board.c
│   ├── board.h
│   └── linker.ld
└── ...                  # 80+ more board ports
```

A board port must implement the `eboot_board_ops_t` interface:

```c
typedef struct {
    int  (*init)(void);
    void (*deinit)(void);
    int  (*flash_read)(uint32_t addr, uint8_t *buf, size_t len);
    int  (*flash_write)(uint32_t addr, const uint8_t *buf, size_t len);
    int  (*flash_erase)(uint32_t addr, size_t len);
    void (*reset)(void);
    void (*uart_putc)(char c);
} eboot_board_ops_t;
```

---

## 17.9 Integration with EoS and ebuild

eBoot is designed to work as part of the EoS toolchain:

```
ebuild ──► eBoot (bootloader) ──► EoS (kernel + apps)
  │              │                       │
  └── build ─────┴── flash ──────────────┘
      configs         binary              firmware
```

When using `ebuild`, the bootloader is automatically included in the firmware
image. The build system handles:

- Selecting the correct eBoot board port based on the target hardware
- Linking the bootloader and application firmware into a single flashable image
- Generating the boot metadata with correct slot information

---

## 17.10 Debugging and Diagnostics

### Boot Log

eBoot outputs diagnostic information over UART during boot:

```
[eBoot v1.0] Stage-0 init OK
[eBoot] Flash: 1MB @ 0x08000000
[eBoot] Slot A: valid (SHA-256 OK), attempts=0
[eBoot] Slot B: valid (SHA-256 OK), attempts=0
[eBoot] Policy: boot Slot A (active)
[eBoot] Jumping to 0x08040000...
```

### Failure Recovery

eBoot maintains boot attempt counters per slot. If a slot fails to boot
successfully three times in a row (detected via watchdog timeout or explicit
failure flag), eBoot automatically falls back to the other slot:

```
[eBoot] Slot A: boot failed (attempt 3/3)
[eBoot] Rollback: switching active to Slot B
[eBoot] Jumping to 0x08080000...
```

---

## 17.11 Testing

eBoot includes comprehensive tests that run on the host machine:

```bash
cmake -B build -DEBLDR_BUILD_TESTS=ON
cmake --build build
cd build && ctest --output-on-failure
```

Test categories include:

| Category        | Coverage                                     |
|-----------------|----------------------------------------------|
| Crypto          | SHA-256, CRC-32, secure compare              |
| Slot management | A/B switching, rollback, anti-rollback        |
| Flash driver    | Read, write, erase, boundary conditions       |
| Update pipeline | Chunked update, power-fail simulation         |
| Boot policy     | Policy evaluation, failure counters           |

---

## 17.12 Summary

eBoot provides the critical foundation layer for EoS-based systems. Its
two-stage architecture, A/B slot management, and secure boot chain ensure that
devices boot reliably and securely — even in the face of failed updates or
compromised firmware. With 83 board ports and seamless integration with ebuild,
eBoot scales from tiny microcontrollers to application processors.

**Key takeaways:**

- Two-stage boot architecture minimizes the trusted code base
- A/B slots with automatic rollback ensure update safety
- Stream-based firmware updates work on memory-constrained devices
- Self-contained crypto — no external dependencies in the boot path
- 83 board ports across 73 architecture families

---

*Next: Chapter 18 — ebuild Build System*
