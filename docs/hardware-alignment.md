# Hardware Support Alignment: eos + eboot

This document tracks which hardware is supported by each component and whether they are aligned for end-to-end usage.

---

## Alignment Status Summary

```
                    eos board    eboot board    Aligned?
                    definition   port           (can use together)
                    ─────────    ──────────     ──────────────────
ARM Cortex-M:
  nRF52840          ✅           ✅             ✅ ALIGNED
  STM32F4           ✅           ✅             ✅ ALIGNED
  STM32H7           ✅           ✅             ✅ ALIGNED
  SAMD51            ✅           ✅             ✅ ALIGNED

ARM Cortex-A:
  Raspberry Pi 4    ✅           ✅             ✅ ALIGNED
  NXP i.MX8M        ✅           ✅             ✅ ALIGNED
  TI AM64x          ✅           ✅             ✅ ALIGNED
  STM32MP1          ✅           ✅             ✅ ALIGNED
  QEMU ARM64        ✅           ✅             ✅ ALIGNED

RISC-V:
  RISC-V 64 (QEMU)  ✅           ✅             ✅ ALIGNED
  SiFive U74        ✅           ✅             ✅ ALIGNED

Xtensa:
  ESP32             ✅           ✅             ✅ ALIGNED

x86:
  x86 generic       ✅           ✅             ✅ ALIGNED
  x86_64 (UEFI)     ✅           ✅             ✅ ALIGNED

Legacy/Niche:
  MIPS32            ✅           ✅             ✅ ALIGNED
  PowerPC           ✅           ✅             ✅ ALIGNED
  SPARC LEON        ✅           ✅             ✅ ALIGNED
  M68K / ColdFire   ✅           ✅             ✅ ALIGNED
  SuperH SH-4       ✅           ✅             ✅ ALIGNED
  V850              ✅           ✅             ✅ ALIGNED
  FR-V              ✅           ✅             ✅ ALIGNED
  H8/300            ✅           ✅             ✅ ALIGNED
  MN103             ✅           ✅             ✅ ALIGNED
  StrongARM         ✅           ✅             ✅ ALIGNED
  XScale            ✅           ✅             ✅ ALIGNED
```

**Result: 25/25 boards aligned.** Every eboot board has a matching eos board definition, and every eos board has a matching eboot port.

---

## Detailed Board Inventory

### eos Board Definitions (`eos/boards/`)

| Board File | Architecture | Core | Vendor | Flash | RAM |
|-----------|-------------|------|--------|-------|-----|
| `nrf52840.yaml` | ARM | Cortex-M4F | Nordic | 1 MB | 256 KB |
| `stm32f4.yaml` | ARM | Cortex-M4 | ST | 1 MB | 192 KB |
| `stm32h743.yaml` | ARM | Cortex-M7 | ST | 2 MB | 1 MB |
| `samd51.yaml` | ARM | Cortex-M4F | Microchip | 512 KB | 192 KB |
| `raspberrypi4.yaml` | ARM64 | Cortex-A72 | Broadcom | SD | 1-8 GB |
| `imx8m.yaml` | ARM64 | Cortex-A53 | NXP | eMMC | 2 GB |
| `am64x.yaml` | ARM | A53+R5F | TI | 64 MB OSPI | 2 GB DDR |
| `stm32mp1.yaml` | Hybrid | A7+M4 | ST | eMMC | 512 MB |
| `qemu-arm64.yaml` | ARM64 | Cortex-A57 | QEMU | virtio | 512 MB |
| `generic-riscv64.yaml` | RISC-V | RV64GC | QEMU | virtio | 256 MB |
| `sifive_u.yaml` | RISC-V | U74 | SiFive | SPI flash | 8 GB DDR |
| `esp32.yaml` | Xtensa | LX6 | Espressif | 4 MB | 520 KB |
| `generic-x86.yaml` | x86 | i686 | Generic | disk | 512 MB |
| `generic-x86_64.yaml` | x86_64 | x86_64 | Generic | NVMe | 4 GB |
| `generic-mips.yaml` | MIPS | MIPS32r2 | MIPS | 8 MB | 32 MB |
| `generic-powerpc.yaml` | PowerPC | PPC440 | IBM | 16 MB | 512 MB |
| `generic-sparc.yaml` | SPARC | LEON3 | Gaisler | 4 MB | 16 MB |
| `generic-m68k.yaml` | M68K | ColdFire V3 | NXP | 2 MB | 4 MB |
| `generic-sh.yaml` | SuperH | SH-4 | Renesas | 4 MB | 16 MB |
| `generic-v850.yaml` | V850 | V850E2M | Renesas | 2 MB | 512 KB |
| `generic-frv.yaml` | FR-V | FR450 | Fujitsu | 4 MB | 64 MB |
| `generic-h8300.yaml` | H8/300 | H8/3069 | Renesas | 512 KB | 16 MB |
| `generic-mn103.yaml` | MN103 | MN1030 | Panasonic | 2 MB | 8 MB |
| `generic-strongarm.yaml` | ARM | StrongARM | Intel | 32 MB | 32 MB |
| `generic-xscale.yaml` | ARM | XScale | Intel | 32 MB | 64 MB |

### eboot Board Ports (`eboot/boards/`)

| Board Directory | Architecture | Platform Enum | Core | Flash | RAM |
|----------------|-------------|---------------|------|-------|-----|
| `nrf52/` | ARM | `EOS_PLATFORM_ARM_CM4` | Cortex-M4F | 1 MB | 256 KB |
| `stm32f4/` | ARM | `EOS_PLATFORM_ARM_CM4` | Cortex-M4 | 1 MB | 192 KB |
| `stm32h7/` | ARM | `EOS_PLATFORM_ARM_CM7` | Cortex-M7 | 2 MB | 1 MB |
| `samd51/` | ARM | `EOS_PLATFORM_ARM_CM4` | Cortex-M4F | 512 KB | 192 KB |
| `rpi4/` | ARM64 | `EOS_PLATFORM_ARM_CA72` | Cortex-A72 | SD | 1-8 GB |
| `imx8m/` | ARM64 | `EOS_PLATFORM_ARM_CA53` | Cortex-A53 | eMMC | 2 GB |
| `am64x/` | ARM | `EOS_PLATFORM_ARM_CA53` | A53+R5F | 64 MB | 2 GB |
| `stm32mp1/` | ARM | `EOS_PLATFORM_ARM_CA72` | A7+M4 | eMMC | 512 MB |
| `qemu-arm64/` | ARM64 | `EOS_PLATFORM_ARM_CA53` | Cortex-A57 | virtio | 512 MB |
| `riscv64_virt/` | RISC-V | `EOS_PLATFORM_RISCV64` | RV64GC | virtio | 256 MB |
| `sifive_u/` | RISC-V | `EOS_PLATFORM_RISCV64` | U74 | SPI | 8 GB |
| `esp32/` | Xtensa | `EOS_PLATFORM_XTENSA` | LX6 | 4 MB | 520 KB |
| `x86/` | x86 | `EOS_PLATFORM_X86` | i686 | disk | 512 MB |
| `x86_64_efi/` | x86_64 | `EOS_PLATFORM_X86_64` | x86_64 | NVMe | 4 GB |
| `mips/` | MIPS | `EOS_PLATFORM_MIPS` | MIPS32 | 8 MB | 32 MB |
| `powerpc/` | PowerPC | `EOS_PLATFORM_POWERPC` | PPC440 | 16 MB | 512 MB |
| `sparc/` | SPARC | `EOS_PLATFORM_SPARC` | LEON3 | 4 MB | 16 MB |
| `m68k/` | M68K | `EOS_PLATFORM_M68K` | ColdFire | 2 MB | 4 MB |
| `sh4/` | SuperH | `EOS_PLATFORM_SH4` | SH-4 | 4 MB | 16 MB |
| `v850/` | V850 | `EOS_PLATFORM_V850` | V850E2M | 2 MB | 512 KB |
| `frv/` | FR-V | `EOS_PLATFORM_FRV` | FR450 | 4 MB | 64 MB |
| `h8300/` | H8/300 | `EOS_PLATFORM_H8300` | H8/3069 | 512 KB | 16 MB |
| `mn103/` | MN103 | `EOS_PLATFORM_MN103` | MN1030 | 2 MB | 8 MB |
| `strongarm/` | ARM | `EOS_PLATFORM_STRONGARM` | SA-110 | 32 MB | 32 MB |
| `xscale/` | ARM | `EOS_PLATFORM_XSCALE` | PXA270 | 32 MB | 64 MB |

---

## Architecture Alignment

### Architectures Supported by Both

| Architecture | eos Toolchain | eos Boards | eboot Boards | Platform Enum |
|-------------|--------------|-----------|-------------|---------------|
| **ARM Cortex-M** | `arm-none-eabi` | nRF52840, STM32F4, STM32H7, SAMD51 | nrf52, stm32f4, stm32h7, samd51 | `ARM_CM0`..`ARM_CM33` |
| **ARM Cortex-A** | `aarch64-linux-gnu` | RPi4, i.MX8M, AM64x, STM32MP1, QEMU | rpi4, imx8m, am64x, stm32mp1, qemu-arm64 | `ARM_CA53`, `ARM_CA72` |
| **RISC-V 64** | `riscv64-linux-gnu` | generic-riscv64, SiFive U | riscv64_virt, sifive_u | `RISCV64` |
| **Xtensa** | (vendor SDK) | ESP32 | esp32 | `XTENSA` |
| **x86 / x86_64** | `x86_64-linux-gnu` | generic-x86, generic-x86_64 | x86, x86_64_efi | `X86`, `X86_64` |
| **MIPS** | (vendor SDK) | generic-mips | mips | `MIPS` |
| **PowerPC** | (vendor SDK) | generic-powerpc | powerpc | `POWERPC` |
| **SPARC** | (vendor SDK) | generic-sparc | sparc | `SPARC` |
| **M68K** | (vendor SDK) | generic-m68k | m68k | `M68K` |
| **SuperH** | (vendor SDK) | generic-sh | sh4 | `SH4` |
| **V850** | (vendor SDK) | generic-v850 | v850 | `V850` |
| **FR-V** | (vendor SDK) | generic-frv | frv | `FRV` |
| **H8/300** | (vendor SDK) | generic-h8300 | h8300 | `H8300` |
| **MN103** | (vendor SDK) | generic-mn103 | mn103 | `MN103` |
| **StrongARM** | (vendor SDK) | generic-strongarm | strongarm | `STRONGARM` |
| **XScale** | (vendor SDK) | generic-xscale | xscale | `XSCALE` |

**16/16 architectures aligned across eos and eboot.**

---

## Peripheral / Feature Coverage by Board

### Key
- ✅ = Built-in hardware support (on-chip peripheral)
- 🔌 = Supported via external module (connected via SPI/I2C/UART/USB)
- ⚠️ = Board-specific (only some boards in this class)
- ❌ = Not applicable (no reasonable hardware path)

### Which eos HAL peripherals are available per board class

| Peripheral | Cortex-M (MCU) | Cortex-A (Linux) | RISC-V | Xtensa | x86 | Legacy |
|-----------|---------------|-----------------|--------|--------|-----|--------|
| GPIO | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| UART | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| SPI | ✅ | ✅ | ✅ | ✅ | ✅ PCH/LPC | ✅ |
| I2C | ✅ | ✅ | ✅ | ✅ | ✅ SMBus | ✅ |
| Timer | ✅ | ✅ | ✅ | ✅ | ✅ HPET | ✅ |
| ADC | ✅ | ✅ sysfs | 🔌 MCP3008 | ✅ | 🔌 USB DAQ | 🔌 SPI ADC |
| PWM | ✅ | ✅ | ✅ SiFive | ✅ LEDC | 🔌 PCIe | 🔌 software |
| CAN | ✅ STM32 | ⚠️ AM64x | 🔌 MCP2515 | ✅ ESP32 | 🔌 PCIe PCAN | ✅ SPARC |
| USB | ✅ | ✅ | ✅ SiFive | ✅ | ✅ | ⚠️ |
| Ethernet | ✅ STM32H7 | ✅ | ✅ SiFive | ✅ | ✅ | ✅ |
| WiFi | ⚠️ ESP8266 ext | ✅ RPi4 | 🔌 USB/SPI | ✅ ESP32 | 🔌 PCIe/USB | ❌ |
| BLE | ✅ nRF52 | ✅ RPi4 | 🔌 USB/UART | ✅ ESP32 | 🔌 USB dongle | ❌ |
| Camera | ⚠️ STM32H7 DCMI | ✅ MIPI CSI | 🔌 USB UVC | 🔌 ESP32-CAM | 🔌 USB UVC | ❌ |
| Display | ✅ SPI/I2C | ✅ HDMI/DSI | 🔌 SPI TFT | 🔌 SPI TFT | ✅ GPU/VGA | 🔌 SPI TFT |
| GNSS | 🔌 UART module | ✅ | 🔌 UART | 🔌 UART | 🔌 UART/USB | 🔌 UART |
| IMU | 🔌 I2C module | ✅ | 🔌 I2C | 🔌 I2C | 🔌 I2C/USB | 🔌 I2C |
| Motor | ✅ PWM+GPIO | 🔌 GPIO | 🔌 GPIO+PWM | 🔌 LEDC+GPIO | 🔌 GPIO | 🔌 GPIO |
| Audio | ⚠️ I2S | ✅ | ⚠️ I2S | ✅ I2S | ✅ HDA | ❌ |
| DMA | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Watchdog | ✅ | ✅ | ✅ | ✅ | ✅ iTCO/ACPI | ✅ |
| Multicore | ⚠️ STM32H7 | ✅ RPi4, i.MX8 | ✅ SiFive | ✅ ESP32 | ✅ SMP | ⚠️ PPC SMP |

**Notes:**
- 🔌 External modules work on ANY platform with the right bus (SPI/I2C/UART/USB)
- The EoS HAL API is identical regardless of whether the peripheral is built-in or external
- External peripherals are listed in `external_peripherals:` in each board's YAML

### Which eboot features are available per board

| Feature | Cortex-M | Cortex-A | RISC-V | Xtensa | x86 | Legacy |
|---------|----------|----------|--------|--------|-----|--------|
| A/B Slots | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Secure Boot | ✅ | ✅ | ✅ | ✅ | ✅ UEFI | ✅ SHA+CRC |
| Recovery | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Watchdog | ✅ | ✅ | ✅ | ✅ | ✅ iTCO | ✅ |
| UART Transport | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Multicore Boot | ⚠️ STM32H7 | ✅ PSCI | ✅ SBI HSM | ✅ REG | ✅ SIPI | ⚠️ PPC SMP |
| DRAM Init | ❌ (SRAM) | ✅ | ✅ | ⚠️ PSRAM | ✅ | ⚠️ PPC DDR |
| PCIe Enum | ❌ | ✅ | ✅ SiFive | ❌ | ✅ | ⚠️ PPC PCI |
| RTOS Boot | ✅ | ✅ | ✅ | ✅ | ✅ UEFI | ✅ |
| Boot Menu | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |

---

## Toolchain Alignment

| Architecture | eos Toolchain File | Cross-Compiler | Status |
|-------------|-------------------|----------------|--------|
| ARM Cortex-M | `arm-none-eabi.yaml` | `arm-none-eabi-gcc` | ✅ Ready |
| ARM Cortex-A / ARM64 | `aarch64-linux-gnu.yaml` | `aarch64-linux-gnu-gcc` | ✅ Ready |
| RISC-V 64 | `riscv64-linux-gnu.yaml` | `riscv64-linux-gnu-gcc` | ✅ Ready |
| x86_64 | `x86_64-linux-gnu.yaml` | `x86_64-linux-gnu-gcc` | ✅ Ready |
| Xtensa (ESP32) | — | `xtensa-esp32-elf-gcc` | ⚠️ Needs vendor SDK |
| MIPS | — | `mips-linux-gnu-gcc` | ⚠️ Needs toolchain file |
| PowerPC | — | `powerpc-linux-gnu-gcc` | ⚠️ Needs toolchain file |
| SPARC | — | `sparc-elf-gcc` | ⚠️ Needs toolchain file |
| M68K / ColdFire | — | `m68k-elf-gcc` | ⚠️ Needs toolchain file |
| SuperH | — | `sh4-linux-gnu-gcc` | ⚠️ Needs toolchain file |
| Other legacy | — | vendor-specific | ⚠️ Needs toolchain file |

**4 of 16 architectures have eos toolchain files.** The remaining 12 architectures have board definitions in both repos but require users to provide their own cross-compiler toolchain file or vendor SDK.

---

## Product Profile → Board Mapping

Which eos product profiles are typically used with which boards:

| Product Profile | Primary Boards | Architecture |
|----------------|---------------|-------------|
| `iot` | nRF52840, ESP32 | ARM Cortex-M4F, Xtensa |
| `watch` / `wearable` / `fitness` | nRF52840, SAMD51 | ARM Cortex-M4F |
| `automotive` | STM32H7, AM64x | ARM Cortex-M7, Cortex-A53 |
| `medical` / `diagnostic` | STM32F4, STM32H7 | ARM Cortex-M4/M7 |
| `drone` | STM32H7 | ARM Cortex-M7 |
| `robot` / `vacuum` | STM32H7, i.MX8M | ARM Cortex-M7, Cortex-A53 |
| `gateway` / `router` | RPi4, i.MX8M, STM32H7 | ARM Cortex-A72/A53/M7 |
| `server` / `ai_edge` | x86_64, RPi4 | x86_64, ARM64 |
| `gaming` / `xr_headset` | Custom SoC, i.MX8M | ARM64 |
| `satellite` / `aerospace` | SPARC LEON, STM32H7 | SPARC, ARM Cortex-M7 |
| `industrial` / `plc` | STM32F4, AM64x | ARM Cortex-M4, Cortex-A53 |
| `telecom` | x86_64 | x86_64 |
| `smart_home` / `voice` | ESP32, RPi4 | Xtensa, ARM64 |
| `printer` | STM32F4 | ARM Cortex-M4 |
| `pos` / `banking` | STM32H7 | ARM Cortex-M7 |

---

## End-to-End Verified Combinations

These board + profile combinations have been verified to work with both eos and eboot:

| Board | eos Profile | eboot Board | Build | Flash | Boot | OTA |
|-------|-----------|-------------|-------|-------|------|-----|
| nRF52840 | `iot` | `nrf52` | ✅ | ✅ J-Link | ✅ | ✅ |
| nRF52840 | `wearable` | `nrf52` | ✅ | ✅ J-Link | ✅ | ✅ |
| STM32F4 | `industrial` | `stm32f4` | ✅ | ✅ ST-Link | ✅ | ✅ |
| STM32H7 | `automotive` | `stm32h7` | ✅ | ✅ ST-Link | ✅ | ✅ |
| STM32H7 | `drone` | `stm32h7` | ✅ | ✅ ST-Link | ✅ | ✅ |
| RPi4 | `gateway` | `rpi4` | ✅ | ✅ SD card | ✅ | ✅ |
| ESP32 | `smart_home` | `esp32` | ✅ | ✅ esptool | ✅ | ✅ |
| RISC-V 64 | `iot` | `riscv64_virt` | ✅ | ✅ QEMU | ✅ | ✅ |
| x86_64 | `server` | `x86_64_efi` | ✅ | ✅ UEFI | ✅ | ✅ |
| SPARC | `satellite` | `sparc` | ✅ | ✅ GRMON | ✅ | ✅ |

---

## How to Verify Alignment for a New Board

When adding a new board to either eos or eboot, use this checklist:

### Checklist

- [ ] Board definition exists in `eos/boards/<board>.yaml`
- [ ] Board port exists in `eboot/boards/<board>/`
- [ ] Platform enum exists in `eboot/include/eos_hal.h` (`eos_platform_t`)
- [ ] Toolchain file exists in `eos/toolchains/` (or documented as vendor SDK)
- [ ] At least one product profile works with this board
- [ ] Flash layout in eboot header matches eos linker expectations
- [ ] Memory map (flash base, RAM base) is consistent between eos board YAML and eboot header
- [ ] Board is listed in this alignment document
- [ ] Quickstart guide exists in `eos/docs/quickstart-<board>.md` (for primary boards)

### Adding a board to maintain alignment

```bash
# 1. Create eos board definition
# eos/boards/my_board.yaml

# 2. Create eboot board port
# eboot/boards/my_board/board_my_board.h
# eboot/boards/my_board/board_my_board.c

# 3. Add platform enum if new architecture
# eboot/include/eos_hal.h → eos_platform_t

# 4. Verify flash layout consistency
# eos board YAML flash base == eboot header FLASH_BASE
# eos linker slot address == eboot header SLOT_A_ADDR
```
