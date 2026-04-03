# Configuration Examples

How EoS product profiles, board configs, and build variants work together to
produce firmware tailored for specific hardware and use cases.

---

## How Product Profiles Work

Product profiles are compile-time feature selectors. Each profile is a C header
in `products/` that defines a set of `EOS_ENABLE_*` macros. The build system
includes exactly one profile via `eos_config.h`, which gates every subsystem
behind `#if EOS_ENABLE_<MODULE>` guards.

### Configuration Flow

```
cmake -DEOS_PRODUCT=iot
    │
    ▼
CMakeLists.txt sets -DEOS_PRODUCT_IOT
    │
    ▼
eos_config.h includes products/iot.h
    │
    ▼
products/iot.h defines EOS_ENABLE_GPIO=1, EOS_ENABLE_WIFI=1, ...
    │
    ▼
eos_config.h defaults all undefined EOS_ENABLE_* to 0
    │
    ▼
Each module checks: #if EOS_ENABLE_<MODULE>
    │
    ▼
Only enabled modules are compiled into the firmware
```

### YAML Schema (Board Config)

Board configuration files in `boards/` use this schema:

```yaml
board:
  name: <string>              # Short identifier (used in -DEOS_BOARD=)
  mcu: <string>               # MCU part number
  family: <string>            # MCU family (e.g., STM32F4, nRF52)
  arch: <string>              # Architecture (arm, riscv, x86, ...)
  core: <string>              # CPU core (cortex-m4, cortex-a53, ...)
  vendor: <string>            # Silicon vendor name
  clock_hz: <integer>         # Default system clock in Hz
  memory:
    flash: <integer>           # Flash size in bytes
    ram: <integer>             # RAM size in bytes
  peripherals:                # List of on-chip peripherals
    - name: <string>           #   Peripheral instance (e.g., USART1)
      type: <string>           #   Class: uart, spi, i2c, gpio, adc, ...
      bus: <string>            #   Bus domain: apb1, apb2, ahb
      channels: <integer>     #   (optional) Number of channels
      speed: <string>          #   (optional) Speed class: full, high
  features: [<string>, ...]   # Hardware capabilities: fpu, dsp, mpu, ...
  toolchain: <string>         # Toolchain ID (maps to toolchains/)
```

### Feature Flag Mapping

Product profile headers map to compile-time `#define` flags:

| Product Header Macro | Module Affected | Build Artifact |
|---------------------|-----------------|----------------|
| `EOS_ENABLE_GPIO` | `hal/gpio.c` | GPIO driver |
| `EOS_ENABLE_UART` | `hal/uart.c` | UART driver |
| `EOS_ENABLE_WIFI` | `drivers/wifi/` | WiFi stack |
| `EOS_ENABLE_BLE` | `drivers/ble/` | BLE stack |
| `EOS_ENABLE_NET` | `net/src/net.c` | TCP/IP, MQTT, HTTP, mDNS |
| `EOS_ENABLE_CRYPTO` | `services/crypto/` | AES, SHA, RSA, ECC |
| `EOS_ENABLE_OTA` | `services/ota/` | Over-the-air update |
| `EOS_ENABLE_SENSOR` | `services/sensor/` | Sensor framework |
| `EOS_ENABLE_SAFETY` | `services/safety/` | Watchdog, redundancy |
| `EOS_ENABLE_UI` | `ui/` | Display/touch UI framework |

### Compile-Time vs Runtime Configuration

| Aspect | Compile-Time (Product Profile) | Runtime (Config API) |
|--------|-------------------------------|---------------------|
| **Mechanism** | `#define` / `#if` guards | Config registers, EEPROM |
| **Granularity** | Entire modules on/off | Individual parameters |
| **Binary size** | Excluded modules save flash | No binary size impact |
| **Change requires** | Rebuild + reflash | API call or reboot |
| **Examples** | Enable/disable WiFi stack | Set WiFi SSID, MQTT broker |

---

## Profile Walkthrough: IoT Sensor (Minimal)

**File:** `products/iot.h`

**Target hardware:** Battery-powered sensor node (e.g., STM32F4 + BME280 + ESP32 WiFi module)

```c
// products/iot.h
#define EOS_PRODUCT_NAME    "iot"

/* Core peripherals */
#define EOS_ENABLE_GPIO      1
#define EOS_ENABLE_UART      1
#define EOS_ENABLE_SPI       1
#define EOS_ENABLE_I2C       1
#define EOS_ENABLE_TIMER     1

/* Extended peripherals */
#define EOS_ENABLE_ADC       1
#define EOS_ENABLE_WIFI      1
#define EOS_ENABLE_BLE       1
#define EOS_ENABLE_CELLULAR  1
#define EOS_ENABLE_FLASH     1
#define EOS_ENABLE_RTC       1
#define EOS_ENABLE_WDT       1

/* Services */
#define EOS_ENABLE_CRYPTO    1
#define EOS_ENABLE_OTA       1
#define EOS_ENABLE_FILESYSTEM 1

/* Frameworks */
#define EOS_ENABLE_POWER     1
#define EOS_ENABLE_NET       1
#define EOS_ENABLE_SENSOR    1
```

**What's excluded:** Display, Audio, Camera, GPU, CAN, USB, Motor control,
Safety framework — these are unnecessary for a sensor node and would waste
flash.

**Resulting firmware size:** ~48 KB flash, ~8 KB RAM (on STM32F407)

**Build command:**

```bash
cmake -B build-iot \
  -DEOS_BOARD=stm32f4 \
  -DEOS_PRODUCT=iot \
  -DCMAKE_TOOLCHAIN_FILE=toolchains/arm-none-eabi-stm32f4.cmake \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build-iot
```

---

## Profile Walkthrough: Industrial PLC (Safety)

**File:** `products/plc.h`

**Target hardware:** Industrial controller (e.g., TMS570 safety MCU with CAN bus, Ethernet)

```c
// products/plc.h
#define EOS_PRODUCT_NAME    "plc"

/* Core peripherals */
#define EOS_ENABLE_GPIO      1
#define EOS_ENABLE_UART      1
#define EOS_ENABLE_SPI       1
#define EOS_ENABLE_I2C       1
#define EOS_ENABLE_TIMER     1

/* Extended peripherals */
#define EOS_ENABLE_ADC       1
#define EOS_ENABLE_DAC       1
#define EOS_ENABLE_PWM       1
#define EOS_ENABLE_CAN       1
#define EOS_ENABLE_ETHERNET  1
#define EOS_ENABLE_FLASH     1
#define EOS_ENABLE_RTC       1
#define EOS_ENABLE_WDT       1

/* Safety */
#define EOS_ENABLE_WATCHDOG  1
#define EOS_ENABLE_SAFETY    1

/* Frameworks */
#define EOS_ENABLE_NET       1
#define EOS_ENABLE_SENSOR    1
#define EOS_ENABLE_MOTOR_CTRL 1
#define EOS_ENABLE_FILESYSTEM 1
```

**Key differences from IoT:**
- **Safety features enabled** — watchdog supervisor, redundancy checks
- **CAN bus** — industrial field bus for PLCs
- **DAC + PWM + Motor control** — actuator/motor output
- **No wireless** — wired Ethernet only (reliability requirement)
- **No OTA** — firmware updates via controlled maintenance windows

**Build command:**

```bash
cmake -B build-plc \
  -DEOS_BOARD=tms570 \
  -DEOS_PRODUCT=plc \
  -DCMAKE_TOOLCHAIN_FILE=toolchains/arm-none-eabi-r5.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DEOS_SAFETY_LEVEL=SIL2

cmake --build build-plc
```

---

## Profile Walkthrough: Automotive Infotainment (Full)

**File:** `products/infotainment.h`

**Target hardware:** Application processor (e.g., i.MX8M with display, GPU, audio, CAN)

```c
// products/infotainment.h
#define EOS_PRODUCT_NAME    "infotainment"

/* Core peripherals */
#define EOS_ENABLE_GPIO      1
#define EOS_ENABLE_UART      1
#define EOS_ENABLE_SPI       1
#define EOS_ENABLE_I2C       1
#define EOS_ENABLE_TIMER     1

/* Extended peripherals */
#define EOS_ENABLE_CAN       1      // vehicle bus
#define EOS_ENABLE_USB       1      // media playback
#define EOS_ENABLE_WIFI      1      // connectivity
#define EOS_ENABLE_BLE       1      // phone pairing
#define EOS_ENABLE_DISPLAY   1      // LCD/OLED panel
#define EOS_ENABLE_UI        1      // touch UI framework
#define EOS_ENABLE_TOUCH     1      // capacitive touch
#define EOS_ENABLE_AUDIO     1      // speakers, microphone
#define EOS_ENABLE_CAMERA    1      // rear-view camera
#define EOS_ENABLE_HDMI      1      // rear-seat display
#define EOS_ENABLE_GPU       1      // 2D/3D rendering
#define EOS_ENABLE_GNSS      1      // navigation
#define EOS_ENABLE_DMA       1      // high-bandwidth transfers
#define EOS_ENABLE_FLASH     1
#define EOS_ENABLE_RTC       1
#define EOS_ENABLE_SDIO      1      // SD card media
#define EOS_ENABLE_HAPTICS   1      // touch feedback
#define EOS_ENABLE_CELLULAR  1      // 4G/5G connectivity
#define EOS_ENABLE_NFC       1      // key fob, payments

/* Services */
#define EOS_ENABLE_CRYPTO    1
#define EOS_ENABLE_OTA       1
#define EOS_ENABLE_NET       1
#define EOS_ENABLE_FILESYSTEM 1
#define EOS_ENABLE_MULTICORE 1      // multi-core SoC
```

**Key differences:**
- **Everything enabled** — full multimedia, connectivity, and UI stack
- **GPU + Display + Touch + UI** — complete graphical interface
- **Audio + Camera + HDMI** — multimedia I/O
- **CAN** — vehicle bus integration
- **Multicore** — leverages all cores on the SoC
- **Largest firmware footprint** — requires application-class processor with
  several MB of flash

**Build command:**

```bash
cmake -B build-infotainment \
  -DEOS_BOARD=imx8m \
  -DEOS_PRODUCT=infotainment \
  -DCMAKE_TOOLCHAIN_FILE=toolchains/aarch64-linux-gnu.cmake \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build-infotainment
```

### Profile Comparison

| Feature | IoT Sensor | Industrial PLC | Infotainment |
|---------|-----------|----------------|--------------|
| Wireless | WiFi + BLE + Cell | ❌ | WiFi + BLE + Cell + NFC |
| Display/UI | ❌ | ❌ | ✅ Full GUI |
| Audio | ❌ | ❌ | ✅ |
| CAN bus | ❌ | ✅ | ✅ |
| Safety | ❌ | ✅ SIL2 | ❌ |
| Motor control | ❌ | ✅ | ❌ |
| OTA | ✅ | ❌ | ✅ |
| Multicore | ❌ | ❌ | ✅ |
| Flash usage | ~48 KB | ~96 KB | ~2 MB+ |
| Typical MCU | STM32F4 | TMS570 | i.MX8M |

---

## Board Config Customization

### Adding a New Board YAML

To add support for a new board, create a YAML file in `boards/`:

```yaml
# boards/custom_board.yaml
board:
  name: custom_board
  mcu: MY_MCU_123
  family: MY_MCU_FAMILY
  arch: arm
  core: cortex-m7
  vendor: My Vendor
  clock_hz: 400000000          # 400 MHz
  memory:
    flash: 2097152             # 2 MB
    ram: 524288                # 512 KB
  peripherals:
    - {name: USART1, type: uart, bus: apb2}
    - {name: USART3, type: uart, bus: apb1}
    - {name: SPI1, type: spi, bus: apb2}
    - {name: SPI4, type: spi, bus: apb2}
    - {name: I2C1, type: i2c, bus: apb1}
    - {name: I2C2, type: i2c, bus: apb1}
    - {name: GPIO, type: gpio, count: 168}
    - {name: ADC1, type: adc, bus: apb2, channels: 20}
    - {name: ADC3, type: adc, bus: apb2, channels: 20}
    - {name: DAC, type: dac, bus: apb1, channels: 2}
    - {name: TIM1, type: timer, bus: apb2}
    - {name: TIM2, type: timer, bus: apb1}
    - {name: TIM3, type: timer, bus: apb1}
    - {name: CAN1, type: can, bus: apb1}
    - {name: CAN2, type: can, bus: apb1}
    - {name: USB_OTG_FS, type: usb, speed: full}
    - {name: USB_OTG_HS, type: usb, speed: high}
    - {name: ETH, type: ethernet, bus: ahb}
    - {name: SDMMC1, type: sdio, bus: ahb}
    - {name: DMA1, type: dma, bus: ahb}
    - {name: DMA2, type: dma, bus: ahb}
    - {name: WDT, type: watchdog, bus: apb1}
    - {name: RTC, type: rtc, bus: apb1}
  features: [fpu, dsp, mpu, ethernet, can, usb, dma, dcache, icache]
  toolchain: arm-none-eabi
```

### Peripheral Declaration

Each peripheral entry maps to a driver class in `drivers/include/eos/driver.h`:

| YAML `type` | Driver Class | EoS Enable Flag |
|-------------|-------------|-----------------|
| `uart` | `EOS_DRV_CLASS_UART` | `EOS_ENABLE_UART` |
| `spi` | `EOS_DRV_CLASS_SPI` | `EOS_ENABLE_SPI` |
| `i2c` | `EOS_DRV_CLASS_I2C` | `EOS_ENABLE_I2C` |
| `gpio` | `EOS_DRV_CLASS_GPIO` | `EOS_ENABLE_GPIO` |
| `adc` | `EOS_DRV_CLASS_ADC` | `EOS_ENABLE_ADC` |
| `dac` | `EOS_DRV_CLASS_DAC` | `EOS_ENABLE_DAC` |
| `timer` | `EOS_DRV_CLASS_TIMER` | `EOS_ENABLE_TIMER` |
| `can` | `EOS_DRV_CLASS_CAN` | `EOS_ENABLE_CAN` |
| `usb` | `EOS_DRV_CLASS_USB` | `EOS_ENABLE_USB` |
| `ethernet` | `EOS_DRV_CLASS_ETHERNET` | `EOS_ENABLE_ETHERNET` |
| `dma` | `EOS_DRV_CLASS_DMA` | `EOS_ENABLE_DMA` |
| `sdio` | `EOS_DRV_CLASS_SDIO` | `EOS_ENABLE_SDIO` |
| `watchdog` | N/A | `EOS_ENABLE_WDT` |
| `rtc` | N/A | `EOS_ENABLE_RTC` |

### Memory Map

The linker script defines the memory layout. For a typical Cortex-M:

```
/* boards/custom_board/custom_board_flash.ld */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 512K
    CCRAM (rwx) : ORIGIN = 0x10000000, LENGTH = 64K    /* optional: CCM RAM */
}

SECTIONS
{
    .isr_vector : {
        KEEP(*(.isr_vector))
    } > FLASH

    .text : {
        *(.text*)
        *(.rodata*)
        _etext = .;
    } > FLASH

    .data : AT(_etext) {
        _sdata = .;
        *(.data*)
        _edata = .;
    } > RAM

    .bss : {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        _ebss = .;
    } > RAM

    /* Stack at end of RAM */
    ._stack : {
        . = ALIGN(8);
        _sstack = .;
        . = . + 4K;     /* 4 KB main stack */
        _estack = .;
    } > RAM

    /* Heap between BSS and stack */
    ._heap : {
        _sheap = .;
        . = . + 16K;    /* 16 KB heap */
        _eheap = .;
    } > RAM
}
```

---

## Build Variants

### Debug vs Release

```bash
# Debug — no optimization, full symbols, all log levels, assertions enabled
cmake -B build-debug \
  -DEOS_BOARD=stm32f4 \
  -DEOS_PRODUCT=iot \
  -DCMAKE_TOOLCHAIN_FILE=toolchains/arm-none-eabi-stm32f4.cmake \
  -DCMAKE_BUILD_TYPE=Debug

# Release — full optimization, stripped symbols, WARN+ logs only
cmake -B build-release \
  -DEOS_BOARD=stm32f4 \
  -DEOS_PRODUCT=iot \
  -DCMAKE_TOOLCHAIN_FILE=toolchains/arm-none-eabi-stm32f4.cmake \
  -DCMAKE_BUILD_TYPE=Release
```

| Setting | Debug | Release |
|---------|-------|---------|
| Optimization | `-O0` | `-O2` |
| Debug symbols | `-g3` | Stripped |
| Assertions | Enabled | Disabled (`-DNDEBUG`) |
| Log level | `EOS_LOG_TRACE` (all) | `EOS_LOG_WARN` |
| Stack canaries | Enabled | Disabled |
| Binary size | ~120 KB | ~48 KB |

### Size-Optimized vs Speed-Optimized

```bash
# Size-optimized — minimize flash usage
cmake -B build-size \
  -DEOS_BOARD=stm32f4 \
  -DEOS_PRODUCT=iot \
  -DCMAKE_TOOLCHAIN_FILE=toolchains/arm-none-eabi-stm32f4.cmake \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DCMAKE_C_FLAGS_MINSIZEREL="-Os -flto -ffunction-sections -fdata-sections" \
  -DCMAKE_EXE_LINKER_FLAGS="-Wl,--gc-sections"

# Speed-optimized — maximize runtime performance
cmake -B build-speed \
  -DEOS_BOARD=stm32f4 \
  -DEOS_PRODUCT=iot \
  -DCMAKE_TOOLCHAIN_FILE=toolchains/arm-none-eabi-stm32f4.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS_RELEASE="-O3 -flto"
```

| Flag | Purpose |
|------|---------|
| `-Os` | Optimize for size (like `-O2` but prefers smaller code) |
| `-O3` | Optimize for speed (aggressive inlining, vectorization) |
| `-flto` | Link-time optimization (cross-module inlining) |
| `-ffunction-sections` | Place each function in its own section |
| `-fdata-sections` | Place each global in its own section |
| `-Wl,--gc-sections` | Strip unused sections at link time |

### Feature Stripping

Reduce binary size further by selectively disabling features:

```bash
# Minimal sensor build — no networking, no crypto, no OTA
cmake -B build-minimal \
  -DEOS_BOARD=stm32f4 \
  -DEOS_PRODUCT=iot \
  -DCMAKE_TOOLCHAIN_FILE=toolchains/arm-none-eabi-stm32f4.cmake \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DEOS_ENABLE_NET=OFF \
  -DEOS_ENABLE_CRYPTO=OFF \
  -DEOS_ENABLE_OTA=OFF \
  -DEOS_ENABLE_WIFI=OFF \
  -DEOS_ENABLE_BLE=OFF \
  -DEOS_ENABLE_FILESYSTEM=OFF
```

Or create a custom minimal product profile:

```c
// products/sensor_minimal.h
#define EOS_PRODUCT_NAME    "sensor_minimal"

#define EOS_ENABLE_GPIO      1
#define EOS_ENABLE_UART      1
#define EOS_ENABLE_I2C       1
#define EOS_ENABLE_ADC       1
#define EOS_ENABLE_TIMER     1
#define EOS_ENABLE_WDT       1

// Everything else defaults to 0 via eos_config.h
```

Then register it in `include/eos/eos_config.h`:

```c
#elif defined(EOS_PRODUCT_SENSOR_MINIMAL)
#   include "products/sensor_minimal.h"
```

**Size comparison:**

```bash
arm-none-eabi-size build-*/eos-firmware.elf
```

| Variant | text | data | bss | Total |
|---------|------|------|-----|-------|
| IoT (full) | 48,312 | 1,024 | 8,192 | 57,528 |
| IoT (no net) | 28,140 | 512 | 4,096 | 32,748 |
| Sensor minimal | 12,288 | 256 | 2,048 | 14,592 |
| Infotainment | 2,097,152+ | 16,384 | 65,536 | 2 MB+ |

---

## Creating a Custom Product Profile — Step by Step

1. **Copy an existing profile** as a starting point:

   ```bash
   cp products/iot.h products/my_device.h
   ```

2. **Edit the feature flags** — enable only what you need:

   ```c
   // products/my_device.h
   #ifndef EOS_PRODUCT_MY_DEVICE_H
   #define EOS_PRODUCT_MY_DEVICE_H

   #define EOS_PRODUCT_NAME    "my_device"

   #define EOS_ENABLE_GPIO      1
   #define EOS_ENABLE_UART      1
   #define EOS_ENABLE_I2C       1
   #define EOS_ENABLE_SPI       1
   #define EOS_ENABLE_ADC       1
   #define EOS_ENABLE_TIMER     1
   #define EOS_ENABLE_WIFI      1
   #define EOS_ENABLE_NET       1
   #define EOS_ENABLE_CRYPTO    1
   #define EOS_ENABLE_FLASH     1
   #define EOS_ENABLE_WDT       1

   #endif
   ```

3. **Register in `eos_config.h`**:

   ```c
   #elif defined(EOS_PRODUCT_MY_DEVICE)
   #   include "products/my_device.h"
   ```

4. **Build:**

   ```bash
   cmake -B build-mydevice \
     -DEOS_BOARD=stm32f4 \
     -DEOS_PRODUCT=my_device \
     -DCMAKE_TOOLCHAIN_FILE=toolchains/arm-none-eabi-stm32f4.cmake \
     -DCMAKE_BUILD_TYPE=Release

   cmake --build build-mydevice
   ```

5. **Verify binary size** meets your flash budget:

   ```bash
   arm-none-eabi-size build-mydevice/eos-firmware.elf
   ```

---

## Troubleshooting

| Issue | Cause | Fix |
|-------|-------|-----|
| `EOS_PRODUCT_NAME undefined` | Missing `-DEOS_PRODUCT=` | Add CMake flag |
| Module compiles but shouldn't | Product profile enables it | Remove the `EOS_ENABLE_*` line |
| Linker error: undefined `eos_mqtt_*` | `EOS_ENABLE_NET` is 0 | Enable NET in profile or add `-DEOS_ENABLE_NET=ON` |
| Flash overflow | Too many features | Strip features or switch to MinSizeRel |
| Board not found | YAML file missing | Create `boards/<name>.yaml` |
| Wrong clock speed | Board YAML `clock_hz` incorrect | Update YAML and rebuild |

---

## Next Steps

- [STM32 Deployment](stm32-deployment.md) — Board bring-up tutorial
- [Debugging Guide](debugging-guide.md) — GDB, hard faults, memory leaks
- [Wireless Stacks](wireless-stacks.md) — BLE, WiFi, LoRa configuration
- [Networking Protocols](networking-protocols.md) — MQTT, HTTP, mDNS
