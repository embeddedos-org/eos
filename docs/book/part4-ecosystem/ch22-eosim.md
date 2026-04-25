# Chapter 22: EoSim — Multi-Architecture Simulation Platform

*Author: Srikanth Patchava & EmbeddedOS Contributors*

---

## 22.1 Introduction

Hardware availability is often the bottleneck in embedded development. Boards
are expensive, shared among teams, and sometimes not yet manufactured.
**EoSim** removes this constraint by providing simulation and validation for
**52+ platforms** across **12 architectures** — enabling development, testing,
and CI before hardware is ready.

---

## 22.2 Supported Platforms

EoSim supports an extensive range of platforms:

### ARM Cortex-M (MCU)

| Platform        | SoC          | Description                        |
|-----------------|--------------|------------------------------------|
| `stm32f4`       | STM32F407    | Cortex-M4F, 168 MHz, FPU          |
| `stm32h7`       | STM32H743    | Cortex-M7, 480 MHz, Dual-core     |
| `nrf52`         | nRF52840     | Cortex-M4F, BLE 5.0               |
| `nrf5340`       | nRF5340      | Cortex-M33, Dual-core, BLE 5.3    |
| `rp2040`        | RP2040       | Dual Cortex-M0+, PIO              |
| `s32k344`       | S32K344      | Cortex-M7, Automotive CAN-FD      |
| `samd51`        | ATSAMD51     | Cortex-M4F, Arduino/Adafruit      |

### ARM Cortex-A (Application Processors)

| Platform        | SoC          | Description                        |
|-----------------|--------------|------------------------------------|
| `raspi4`        | BCM2711      | Cortex-A72, 4-core, 8 GB          |
| `raspi5`        | BCM2712      | Cortex-A76, 4-core                 |
| `jetson-nano`   | Tegra X1     | Cortex-A57, 128 CUDA              |
| `jetson-orin`   | Orin         | Cortex-A78AE, 40 TOPS             |
| `imx8m`         | i.MX8M       | Cortex-A53, NPU, 4K               |

### RISC-V

| Platform        | SoC          | Description                        |
|-----------------|--------------|------------------------------------|
| `riscv64`       | Generic      | RISC-V 64-bit Linux                |
| `sifive_u`      | FU740        | RISC-V U74, Linux-capable          |
| `esp32c3`       | ESP32-C3     | RISC-V, Wi-Fi/BLE                  |

### Other Architectures

| Platform        | Arch         | Description                        |
|-----------------|--------------|------------------------------------|
| `x86_64`        | x86_64       | Generic Linux on QEMU q35          |
| `esp32`         | Xtensa       | Xtensa LX6, Wi-Fi/BLE             |
| `aurix-tc3xx`   | TriCore      | Automotive safety                  |
| `mipsel`        | MIPS         | MIPS little-endian                 |

---

## 22.3 Simulation Engines

EoSim supports four simulation backends:

| Engine            | Type               | Speed  | Fidelity | Best For              |
|-------------------|--------------------|--------|----------|-----------------------|
| **EoSim native**  | Python simulation  | Fast   | Medium   | Rapid prototyping, CI |
| **Renode**        | Deterministic sim  | Medium | High     | Peripheral-accurate   |
| **QEMU**          | Binary emulation   | Medium | High     | Full firmware testing |
| **HIL**           | Hardware-in-loop   | Real   | Exact    | Final validation      |

### Engine Selection

EoSim automatically selects the best engine based on the platform definition,
but you can override it:

```bash
eosim run stm32f4 --engine renode     # Use Renode for peripheral accuracy
eosim run raspi4 --engine qemu        # Use QEMU for full Linux emulation
eosim run nrf52 --engine native       # Use EoSim native for speed
```

---

## 22.4 CLI Reference

```bash
# List all available platforms
eosim list

# Filter by architecture
eosim list --arch arm-cortex-m

# Show platform details
eosim info stm32f4

# Run a simulation
eosim run arm64-linux

# Run with custom firmware
eosim run stm32f4 --firmware build/firmware.bin

# Run validation tests
eosim test raspi4

# Validate a platform config file
eosim validate platforms/stm32f4/config.yaml

# Check environment health
eosim doctor

# Search platforms
eosim search "bluetooth"

# Platform statistics
eosim stats
```

---

## 22.5 Platform Definition Format

Each platform is defined by a YAML configuration file:

```yaml
name: stm32f4
display_name: STM32F4 Discovery
arch: arm-cortex-m4
engine: renode

simulation:
  machine: STM32F4Discovery
  cpu: cortex-m4
  frequency_mhz: 168

peripherals:
  gpio: [GPIOA, GPIOB, GPIOC, GPIOD]
  uart: [USART1, USART2]
  spi: [SPI1, SPI2]
  i2c: [I2C1]
  timer: [TIM1, TIM2, TIM3, TIM4]
  adc: [ADC1]

memory:
  flash_kb: 1024
  sram_kb: 192

boot:
  entry_point: 0x08000000
  stack_pointer: 0x20030000

runtime:
  headless: true
  uart: sysbus.usart2
```

Platform definitions also include Renode `.repl` (platform description) and
`.resc` (script) files for high-fidelity simulation.

---

## 22.6 GUI Dashboard

EoSim includes an optional Tkinter-based GUI for interactive simulation:

```bash
eosim gui stm32f4
```

### GUI Panels

| Panel                | Description                                  |
|----------------------|----------------------------------------------|
| **GPIO Panel**       | Pin visualizer with click-to-toggle          |
| **UART Terminal**    | Real-time serial output                      |
| **CPU State**        | Registers, PC, SP, flags, clock              |
| **Memory View**      | Hex dump of RAM/Flash regions                |
| **Peripheral Regs**  | TIM, ADC, SPI, I2C configuration registers   |
| **3D Renderers**     | Drone, robot, vehicle, aircraft visualization|

The GUI provides a visual development experience similar to debugging on
real hardware, but without the hardware.

---

## 22.7 CI Integration

EoSim is designed to run in CI pipelines for automated validation:

```yaml
# .github/workflows/firmware-test.yml
name: Firmware Validation
on: [push, pull_request]

jobs:
  simulate:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        platform: [stm32f4, nrf52, raspi4, esp32, riscv64]
    steps:
      - uses: actions/checkout@v4
      - name: Install EoSim
        run: pip install eosim
      - name: Run simulation tests
        run: eosim test ${{ matrix.platform }} --timeout 300
```

---

## 22.8 Cluster Simulation

For multi-device systems, EoSim supports cluster simulation — running
multiple interconnected device instances simultaneously:

```bash
eosim cluster run \
  --nodes "sensor1:nrf52,sensor2:nrf52,gateway:raspi4" \
  --network ble-mesh \
  --duration 60
```

This is invaluable for testing IoT mesh networks, distributed sensor
systems, and multi-device protocols.

---

## 22.9 Specialty Simulators

Beyond embedded hardware, EoSim includes domain-specific simulators:

| Simulator           | Domain                | Description                    |
|---------------------|-----------------------|--------------------------------|
| `aerodynamics-sim`  | Aerospace             | Aerodynamics simulation        |
| `physiology-sim`    | Medical               | Physiological modeling         |
| `weather-sim`       | Environmental         | Weather modeling               |
| `finance-sim`       | Financial             | Financial market simulation    |
| `gaming-sim`        | Gaming                | Game engine simulation         |

These simulators enable domain-specific validation of EoS-based systems.

---

## 22.10 Architecture

```
eosim/
├── eosim/
│   ├── cli/              # Click-based CLI entry point
│   ├── core/             # Core simulation logic
│   ├── engine/           # Backend engines (native, Renode)
│   ├── gui/              # Tkinter GUI dashboard
│   ├── integrations/     # External tool integrations
│   ├── artifacts/        # Simulation artifact management
│   └── platforms/        # Platform abstraction layer
├── platforms/            # 52+ platform definitions (YAML + Renode)
├── tests/                # Unit + integration + scenario tests
└── examples/             # Demo scenarios
```

---

## 22.11 Summary

EoSim enables embedded development without hardware constraints. With 52+
platform definitions, four simulation engines, and CI-native design, it
ensures that firmware is validated early and often.

**Key takeaways:**

- 52+ platforms across 12 architectures
- Four simulation engines: native Python, Renode, QEMU, HIL
- Interactive GUI with GPIO, UART, CPU, and memory panels
- CI-native: runs in GitHub Actions, GitLab CI, Jenkins
- Cluster simulation for multi-device systems
- Domain-specific specialty simulators

---

*Next: Chapter 23 — EoStudio Design Suite*
