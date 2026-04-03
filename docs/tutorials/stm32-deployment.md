# STM32F407 Discovery — End-to-End Deployment Tutorial

Deploy EoS to an STM32F407 Discovery board from scratch: toolchain installation,
project build, flash, verification, and troubleshooting.

---

## Prerequisites

### Hardware

| Item | Description |
|------|-------------|
| **Board** | STM32F407VG Discovery (STM32F4DISCOVERY) |
| **Debugger** | On-board ST-Link/V2 (built into Discovery board) |
| **USB cables** | Mini-USB (ST-Link) + Micro-USB (optional, for USB OTG) |
| **USB-UART adapter** | 3.3 V FTDI or CP2102 module for serial output |
| **Jumper wires** | Female-to-female for UART and sensor connections |

### Software — ARM GCC Toolchain

Install the ARM bare-metal GCC toolchain (`arm-none-eabi-gcc`).

**Ubuntu / Debian:**

```bash
sudo apt update
sudo apt install -y gcc-arm-none-eabi binutils-arm-none-eabi \
                    libnewlib-arm-none-eabi cmake ninja-build
```

**macOS (Homebrew):**

```bash
brew install --cask gcc-arm-embedded
brew install cmake ninja
```

**Windows (MSYS2):**

```bash
pacman -S mingw-w64-x86_64-arm-none-eabi-gcc cmake ninja
```

Verify the installation:

```bash
arm-none-eabi-gcc --version
# arm-none-eabi-gcc (GNU Arm Embedded Toolchain 13.2) 13.2.1 ...

cmake --version
# cmake version 3.28.1
```

### Software — OpenOCD

OpenOCD provides the debug/flash bridge between ST-Link and the MCU.

**Ubuntu / Debian:**

```bash
sudo apt install -y openocd
```

**macOS:**

```bash
brew install openocd
```

**Windows:**

Download the latest release from [openocd.org](https://openocd.org/) and add it
to your `PATH`.

Verify:

```bash
openocd --version
# Open On-Chip Debugger 0.12.0
```

### Software — ST-Link Driver (Windows Only)

On Linux and macOS, ST-Link works out of the box via libusb. On Windows, install
the [STSW-LINK009](https://www.st.com/en/development-tools/stsw-link009.html)
USB driver from STMicroelectronics.

After installation, confirm the device appears in Device Manager under
**Universal Serial Bus devices → STMicroelectronics STLink Virtual COM Port**.

### udev Rules (Linux Only)

Create a udev rule so OpenOCD can access ST-Link without root:

```bash
sudo tee /etc/udev/rules.d/99-stlink.rules << 'EOF'
# ST-Link/V2
ATTRS{idVendor}=="0483", ATTRS{idProduct}=="3748", MODE="0666", GROUP="plugdev"
# ST-Link/V2-1
ATTRS{idVendor}=="0483", ATTRS{idProduct}=="374b", MODE="0666", GROUP="plugdev"
# ST-Link/V3
ATTRS{idVendor}=="0483", ATTRS{idProduct}=="374f", MODE="0666", GROUP="plugdev"
EOF

sudo udevadm control --reload-rules
sudo udevadm trigger
```

---

## Step 1 — Clone the Repository

```bash
git clone https://github.com/anthropic/EoS.git
cd EoS/eos
```

---

## Step 2 — Select Board Configuration

EoS board definitions live in `boards/`. The STM32F407 Discovery board uses
`boards/stm32f4.yaml`:

```yaml
board:
  name: stm32f4
  mcu: STM32F407VG
  family: STM32F4
  arch: arm
  core: cortex-m4
  vendor: ST Microelectronics
  clock_hz: 168000000
  memory:
    flash: 1048576   # 1 MB
    ram: 196608       # 192 KB
  peripherals:
    - {name: USART1, type: uart, bus: apb2}
    - {name: USART2, type: uart, bus: apb1}
    - {name: SPI1,   type: spi,  bus: apb2}
    - {name: I2C1,   type: i2c,  bus: apb1}
    - {name: ADC1,   type: adc,  bus: apb2, channels: 16}
    - {name: CAN1,   type: can,  bus: apb1}
    - {name: ETH,    type: ethernet, bus: ahb}
    - {name: DMA1,   type: dma,  bus: ahb}
    - {name: WDT,    type: watchdog, bus: apb1}
    - {name: RTC,    type: rtc,  bus: apb1}
  features: [fpu, dsp, mpu, ethernet, can, usb, dma]
  toolchain: arm-none-eabi
```

The board YAML is referenced automatically by the build system when you pass
`-DEOS_BOARD=stm32f4`.

---

## Step 3 — Choose a Product Profile

Product profiles control which EoS subsystems are compiled in. For a sensor node
project, use the `iot` profile:

```bash
# See all available profiles:
ls products/
# adapter.h  automotive.h  gateway.h  industrial.h  infotainment.h
# iot.h  medical.h  plc.h  robot.h  ...
```

The `iot` profile (`products/iot.h`) enables:

| Feature | Enabled |
|---------|---------|
| GPIO, UART, SPI, I2C, Timer | ✅ |
| ADC, WiFi, BLE, Cellular | ✅ |
| Flash, RTC, Watchdog | ✅ |
| Crypto, OTA, Filesystem | ✅ |
| Power management, Networking, Sensor | ✅ |
| Display, Audio, Camera, GPU | ❌ |

Pass `-DEOS_PRODUCT=iot` to cmake to select this profile.

---

## Step 4 — Build

```bash
cmake -B build-stm32 \
  -DEOS_BOARD=stm32f4 \
  -DEOS_PRODUCT=iot \
  -DCMAKE_TOOLCHAIN_FILE=toolchains/arm-none-eabi-stm32f4.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -G Ninja

cmake --build build-stm32
```

Expected output:

```
[1/42] Building C object core/CMakeFiles/eos_core.dir/src/kernel.c.obj
[2/42] Building C object hal/CMakeFiles/eos_hal.dir/src/gpio.c.obj
...
[41/42] Linking C executable eos-firmware.elf
[42/42] Generating eos-firmware.bin
```

The build produces:

| File | Description |
|------|-------------|
| `build-stm32/eos-firmware.elf` | ELF with debug symbols (for GDB) |
| `build-stm32/eos-firmware.bin` | Raw binary (for flashing) |
| `build-stm32/eos-firmware.hex` | Intel HEX (alternative flash format) |
| `build-stm32/eos-firmware.map` | Linker map (memory usage analysis) |

Check firmware size:

```bash
arm-none-eabi-size build-stm32/eos-firmware.elf
#    text    data     bss     dec     hex filename
#   48312    1024    8192   57528    e0b8 build-stm32/eos-firmware.elf
```

---

## Step 5 — Flash via OpenOCD

Connect the Discovery board to your PC via the Mini-USB (ST-Link) connector.

### One-Shot Flash Command

```bash
openocd \
  -f interface/stlink-v2.cfg \
  -f target/stm32f4x.cfg \
  -c "program build-stm32/eos-firmware.bin 0x08000000 verify reset exit"
```

Expected output:

```
Open On-Chip Debugger 0.12.0
Info : STLINK V2J37S7 (API v2) VID:PID 0483:3748
Info : Target voltage: 2.886034
Info : stm32f4x.cpu: Cortex-M4 r0p1 processor detected
Info : stm32f4x.cpu: target state: halted
** Programming Started **
** Programming Finished **
** Verify Started **
** Verified OK **
** Resetting Target **
```

### Alternative: STM32CubeProgrammer CLI

```bash
STM32_Programmer_CLI \
  -c port=SWD freq=4000 \
  -w build-stm32/eos-firmware.bin 0x08000000 \
  -v -rst
```

### Alternative: Flash via GDB

```bash
# Terminal 1 — start OpenOCD server
openocd -f interface/stlink-v2.cfg -f target/stm32f4x.cfg

# Terminal 2 — connect with GDB and flash
arm-none-eabi-gdb build-stm32/eos-firmware.elf
(gdb) target remote :3333
(gdb) monitor reset halt
(gdb) load
(gdb) monitor reset run
(gdb) quit
```

---

## Step 6 — Verify

### UART Wiring

Connect a USB-UART adapter to the board's USART2 pins:

| Board Pin | Signal | UART Adapter |
|-----------|--------|--------------|
| PA2 | USART2_TX | RX |
| PA3 | USART2_RX | TX |
| GND | Ground | GND |

> ⚠️ **Do not connect VCC** — the board is powered via USB. Connecting VCC from
> the UART adapter can damage the board.

### Serial Terminal

Open a terminal at **115200 baud, 8N1**:

```bash
# Linux
screen /dev/ttyUSB0 115200

# macOS
screen /dev/tty.usbserial-* 115200

# Windows (PowerShell)
# Use PuTTY or TeraTerm — select the COM port at 115200 baud
```

### Expected Boot Output

```
========================================
  EoS v0.1.0 — Embedded Operating System
  Board : STM32F407VG (Cortex-M4 @ 168 MHz)
  Product: iot
  Flash : 1024 KB | RAM: 192 KB
========================================
[  0.000] BOOT  : Clock configured — HCLK=168 MHz, APB1=42 MHz, APB2=84 MHz
[  0.001] BOOT  : MPU configured — 8 regions
[  0.001] BOOT  : FPU enabled (single-precision)
[  0.002] HAL   : GPIO driver initialized (114 pins)
[  0.003] HAL   : USART2 initialized @ 115200 baud
[  0.004] HAL   : SPI1 initialized @ 10.5 MHz
[  0.005] HAL   : I2C1 initialized @ 400 kHz
[  0.006] HAL   : ADC1 initialized (16 channels, 12-bit)
[  0.008] DRV   : Driver registry: 6 drivers bound
[  0.010] NET   : Network stack initialized
[  0.012] KERN  : Scheduler started — 3 tasks ready
[  0.500] APP   : LED blink task running (LD3=orange, 500 ms period)
[  1.000] APP   : Sensor read — temp=23.4°C, humidity=48.2%
[  1.001] APP   : ADC ch0 = 2048 (1.65 V)
```

### LED Verification

On the STM32F407 Discovery board, the default EoS blink example toggles LD3
(orange LED, PD13). You should see:

- **LD3 (orange)** — blinks at ~1 Hz (application heartbeat)
- **LD4 (green)** — solid on (system OK indicator)
- **LD5 (red)** — off (error indicator, lights on hard fault)

### Sensor Read Verification

If you have a BME280 sensor connected to I2C1 (PB8=SCL, PB9=SDA), the periodic
sensor task will output readings every second:

```
[  5.000] SENSOR: BME280 — temp=23.6°C, pressure=1013.2 hPa, humidity=47.8%
[ 10.000] SENSOR: BME280 — temp=23.5°C, pressure=1013.3 hPa, humidity=48.1%
```

---

## Troubleshooting

### ST-Link Not Detected

| Symptom | Cause | Fix |
|---------|-------|-----|
| `Error: open failed` | USB not connected / wrong port | Use the **Mini-USB** port (CN1), not Micro-USB |
| `Error: LIBUSB_ERROR_ACCESS` | Permission denied (Linux) | Add udev rules (see Prerequisites) or run with `sudo` |
| `STLINK firmware too old` | ST-Link firmware outdated | Update via [STSW-LINK007](https://www.st.com/en/development-tools/stsw-link007.html) |
| `NACK on address 0x80` | ST-Link V2 clone incompatibility | Try `-f interface/stlink.cfg` instead of `stlink-v2.cfg` |

### Flash Write-Protect Error

```
Error: stm32f4x device protected
Error: failed erasing sectors 0 to 7
```

**Fix:** Remove read/write protection via OpenOCD:

```bash
openocd -f interface/stlink-v2.cfg -f target/stm32f4x.cfg \
  -c "init" \
  -c "reset halt" \
  -c "stm32f4x unlock 0" \
  -c "reset halt" \
  -c "exit"
```

Then power-cycle the board and flash again.

### UART Shows Garbage or No Output

| Symptom | Fix |
|---------|-----|
| Garbage characters | Check baud rate — must be **115200** |
| Nothing at all | Verify TX→RX / RX→TX crossover (board TX to adapter RX) |
| Intermittent data | Check GND connection between board and adapter |
| Characters dropped | Disable hardware flow control in your terminal |

### Build Errors

| Error | Fix |
|-------|-----|
| `arm-none-eabi-gcc: not found` | Toolchain not in PATH — reinstall or `export PATH` |
| `Cannot find -lnosys` | Install `libnewlib-arm-none-eabi` |
| `No rule to make target 'stm32f4'` | Ensure `-DEOS_BOARD=stm32f4` matches a file in `boards/` |
| `Linker region 'FLASH' overflow` | Reduce features — switch to a smaller product profile |

### Hard Fault on Boot

If LD5 (red) lights immediately:

1. Connect GDB (see [Debugging Guide](debugging-guide.md))
2. Read the fault registers:
   ```
   (gdb) monitor reg
   (gdb) x/x 0xE000ED28   # CFSR
   (gdb) x/x 0xE000ED2C   # HFSR
   ```
3. Common causes:
   - **Unaligned access** — check struct packing
   - **Stack overflow** — increase stack size in linker script
   - **Missing interrupt handler** — verify your vector table

---

## Next Steps

- [Debugging Guide](debugging-guide.md) — GDB + OpenOCD setup, hard fault analysis
- [Wireless Stacks](wireless-stacks.md) — BLE and WiFi configuration
- [Networking Protocols](networking-protocols.md) — MQTT, HTTP, mDNS
- [Configuration Examples](configuration-examples.md) — Product profiles and board customization
