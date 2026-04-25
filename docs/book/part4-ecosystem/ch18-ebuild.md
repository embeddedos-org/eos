# Chapter 18: ebuild — The EoS Build System

*Author: Srikanth Patchava & EmbeddedOS Contributors*

---

## 18.1 Introduction

Building firmware for embedded systems has traditionally been a fragmented,
error-prone process. Each target architecture requires its own toolchain,
linker scripts, and build configuration. Multiply this by dozens of supported
boards and the complexity becomes overwhelming.

**ebuild** solves this with a single command: from hardware schematic to
deployable firmware. It reads KiCad/Eagle schematics or board descriptions,
auto-generates all build configurations, and compiles firmware for 73+
embedded targets.

> **Note:** ebuild is the EmbeddedOS Build Tool — not related to Gentoo's
> ebuild package format.

---

## 18.2 Architecture

ebuild operates as a pipeline that transforms hardware descriptions into
firmware binaries. The pipeline has four stages:

1. **Hardware Description** — KiCad schematic, YAML, CSV, or plain text input
2. **Analyze & Parse** — extract and classify components
3. **Generate Configs** — produce 9 configuration files
4. **Build Firmware** — compile the final binary

### Supported Input Formats

| Format         | Description                                  | Parser       |
|----------------|----------------------------------------------|--------------|
| `.kicad_sch`   | KiCad schematic (S-expression format)        | Real parser  |
| `.yaml`        | Board description file                       | YAML parser  |
| `.csv`         | Bill of Materials (Reference, Value columns) | CSV parser   |
| `.txt`         | Plain text description                       | NLP parser   |
| `.eagle`       | Eagle schematic                              | XML parser   |

### Generated Configuration Files

When ebuild analyzes a hardware description, it generates **9 configuration
files** that fully describe the build:

1. `eos_config.h` — Feature flags and peripheral enables
2. `hal_config.h` — HAL peripheral configuration
3. `board.h` — Pin assignments and memory map
4. `linker.ld` — Linker script for the target MCU
5. `CMakeLists.txt` — Build system configuration
6. `openocd.cfg` — Debug probe configuration
7. `flash.sh` — Flashing script
8. `product_profile.h` — EoS product profile header
9. `README.md` — Auto-generated board documentation

---

## 18.3 Installation

```bash
# Linux / macOS
git clone https://github.com/embeddedos-org/ebuild.git
cd ebuild
./install.sh         # or: pip install -e .
ebuild --version     # ebuild, version 0.1.0

# Windows
git clone https://github.com/embeddedos-org/ebuild.git
cd ebuild
install.bat
```

---

## 18.4 Hands-On Examples

### Hello World

```bash
cd examples/hello_world
ebuild info && ebuild build && ./_build/hello
# → "Hello from EoS Build System!"
```

### Multi-Target Build

```bash
cd examples/multi_target
ebuild build && ./_build/myapp
# → add=17, subtract=7, multiply=60, factorial=120
```

### Hardware Analysis

The most powerful feature of ebuild is automatic hardware analysis:

```bash
# From a KiCad schematic (real S-expression parser)
ebuild analyze \
  --file hardware/board/sample_stm32f4_sensor.kicad_sch \
  --output-dir /tmp/configs

# From a YAML board description
ebuild analyze \
  --file hardware/board/sample_iot_gateway.yaml \
  --output-dir /tmp/iot

# From a CSV BOM
ebuild analyze --file /tmp/bom.csv --output-dir /tmp/bom_out

# From plain text
ebuild analyze --file desc.txt --output-dir /tmp/text_out
```

---

## 18.5 The Analyze Pipeline

When ebuild processes a hardware description, it follows a multi-step
pipeline:

### Step 1: Component Extraction

The parser identifies components from the input format and extracts:
- Part numbers (e.g., STM32F407VGT6, BME280, W25Q128)
- Pin connections and nets
- Power rails and decoupling

### Step 2: Component Classification

Each component is classified into categories:

| Category       | Examples                                |
|----------------|-----------------------------------------|
| MCU            | STM32F407, nRF52840, ESP32              |
| Sensor         | BME280, MPU6050, MAX30102               |
| Memory         | W25Q128 (SPI Flash), IS42S16400J (SDRAM)|
| Communication  | SX1276 (LoRa), ENC28J60 (Ethernet)     |
| Power          | TPS63020 (Buck-Boost), MCP1700 (LDO)   |
| Display        | SSD1306 (OLED), ILI9341 (TFT)          |
| Actuator       | DRV8833 (Motor Driver), Servo           |

### Step 3: Configuration Generation

Based on the classified components, ebuild generates all 9 configuration
files with correct peripheral enables, pin assignments, memory layout,
and appropriate EoS product profile selection.

---

## 18.6 Build System Configuration

ebuild uses a `ebuild.toml` project file:

```toml
[project]
name = "my-sensor-node"
version = "1.0.0"

[target]
board = "stm32f407_discovery"
profile = "iot"

[dependencies]
eos = { version = ">=0.5.0", features = ["hal", "rtos", "net"] }
eboot = { version = ">=1.0.0" }

[build]
optimization = "size"
lto = true
strip = true
```

### Build Commands

| Command             | Description                             |
|---------------------|-----------------------------------------|
| `ebuild init`       | Create a new project from template      |
| `ebuild info`       | Display project and toolchain info      |
| `ebuild build`      | Compile the project                     |
| `ebuild clean`      | Remove build artifacts                  |
| `ebuild flash`      | Flash firmware to target                |
| `ebuild debug`      | Start GDB debug session                 |
| `ebuild analyze`    | Analyze hardware and generate configs   |
| `ebuild test`       | Run unit tests                          |
| `ebuild package`    | Create distributable firmware package   |

---

## 18.7 Cross-Compilation Support

ebuild manages cross-compilation toolchains for all EoS-supported
architectures:

| Architecture      | Toolchain                | Targets                    |
|-------------------|--------------------------|----------------------------|
| ARM Cortex-M      | `arm-none-eabi-gcc`      | STM32, nRF52, SAM, RP2040 |
| ARM Cortex-A      | `aarch64-linux-gnu-gcc`  | RPi 4/5, Jetson, i.MX8    |
| ARM Cortex-R      | `arm-none-eabi-gcc`      | TMS570, Cortex-R5/R52     |
| RISC-V            | `riscv64-linux-gnu-gcc`  | SiFive, ESP32-C3, K210    |
| Xtensa            | `xtensa-esp32-elf-gcc`   | ESP32, ESP32-S3           |
| x86_64            | `gcc` / `clang`          | QEMU, native host         |

ebuild automatically selects the correct toolchain based on the target board
and verifies that it is installed before building.

---

## 18.8 Integration with EoS Ecosystem

ebuild is the glue that binds the EoS ecosystem together. It combines
hardware analysis, EoS kernel, eBoot bootloader, and application code
into a single firmware binary.

### Typical Workflow

```bash
# 1. Analyze your hardware
ebuild analyze --file my_board.kicad_sch --output-dir config/

# 2. Build EoS + eBoot + your application
ebuild build --target all

# 3. Flash the complete firmware
ebuild flash --verify --reset

# 4. Debug over SWD/JTAG
ebuild debug
```

---

## 18.9 SDK Generation

ebuild can generate a complete SDK package for distribution:

```bash
ebuild package --sdk --output my-board-sdk.tar.gz
```

The SDK includes:

- Pre-built EoS libraries for the target
- Header files for the HAL and kernel APIs
- Toolchain configuration files
- Example projects
- Documentation

This enables third-party developers to build applications for a specific
board without needing the full EoS source tree.

---

## 18.10 Continuous Integration

ebuild integrates with CI/CD pipelines. A typical GitHub Actions workflow
builds firmware across multiple boards in a matrix strategy, running
`ebuild setup`, `ebuild build`, and `ebuild test` for each target board
(e.g., stm32f4, nrf52840, rpi4, esp32).

---

## 18.11 Summary

ebuild transforms the traditionally painful process of embedded firmware
building into a streamlined, automated pipeline. Its ability to read hardware
schematics and auto-generate complete build configurations eliminates an
entire class of manual, error-prone work.

**Key takeaways:**

- One command from hardware schematic to firmware binary
- Supports 5 input formats (KiCad, YAML, CSV, text, Eagle)
- Generates 9 configuration files automatically
- Manages cross-compilation for 73+ targets
- Integrates eBoot, EoS kernel, and applications into a single build
- SDK generation for third-party distribution

---

*Next: Chapter 19 — eIPC Inter-Process Communication*
