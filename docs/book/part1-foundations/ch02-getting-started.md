# Chapter 2: Getting Started

*Srikanth Patchava & EmbeddedOS Contributors*

---

## 2.1 Prerequisites

Before building EoS, ensure the following tools are installed:

| Tool | Minimum Version | Purpose |
|------|----------------|---------|
| CMake | 3.16 | Build system generator |
| GCC / Clang | 10.x | C compiler |
| Ninja | 1.10 | Fast build backend (recommended) |
| Git | 2.x | Source control |
| Python 3 | 3.8 | Test scripts, tooling |

### Installation by Platform

**Ubuntu / Debian:**

```bash
sudo apt update
sudo apt install cmake gcc g++ git ninja-build python3 python3-pip
```

**macOS:**

```bash
brew install cmake ninja python3
# Xcode Command Line Tools provide GCC/Clang
xcode-select --install
```

**Windows (PowerShell):**

```powershell
winget install CMake.CMake
winget install Ninja-build.Ninja
# Install MinGW-w64 or Visual Studio Build Tools for C compiler
```

### Cross-Compilation Toolchains

For targeting embedded hardware, install the appropriate cross-compiler:

| Target | Toolchain | Install Command |
|--------|-----------|----------------|
| ARM Cortex-M | `arm-none-eabi-gcc` | `sudo apt install gcc-arm-none-eabi` |
| ARM64 Linux | `aarch64-linux-gnu-gcc` | `sudo apt install gcc-aarch64-linux-gnu` |
| RISC-V 64 | `riscv64-linux-gnu-gcc` | `sudo apt install gcc-riscv64-linux-gnu` |

## 2.2 Cloning the Repository

```bash
git clone https://github.com/embeddedos-org/eos.git
cd eos
```

The repository structure:

```
eos/
├── hal/              # Hardware Abstraction Layer
│   └── include/eos/  #   hal.h, hal_extended.h
├── kernel/           # RTOS kernel
│   ├── include/eos/  #   kernel.h, multicore.h
│   └── src/          #   kernel.c, multicore.c
├── drivers/          # Driver framework
│   ├── include/eos/  #   driver.h
│   ├── src/          #   driver.c, driver_framework.c
│   └── devicetree/   #   devicetree.h, devicetree.c
├── products/         # 48 product profile headers
├── examples/         # Working example applications
├── backends/         # Platform-specific HAL implementations
├── boards/           # Board support packages
├── toolchains/       # CMake toolchain files
├── tests/            # Unit and integration tests
├── docs/             # Documentation
└── CMakeLists.txt    # Top-level build configuration
```

## 2.3 Building from Source

### Host Build (No Hardware Required)

The fastest way to start. The host build uses a Linux/POSIX HAL backend that simulates
peripherals via printf and stdin.

```bash
# Configure
cmake -B build -G Ninja

# Build
cmake --build build

# Run tests
cmake -B build -DEOS_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### Building with a Product Profile

```bash
# Build for a specific product category
cmake -B build -DEOS_PRODUCT=iot -G Ninja
cmake --build build
```

The `EOS_PRODUCT` flag includes the matching header from `products/` (e.g.,
`products/iot.h`), which defines the appropriate `EOS_ENABLE_*` flags.

## 2.4 Your First Application: Blink GPIO

The classic "Hello World" of embedded systems. This example toggles an LED every 500ms.

### Source Code (`examples/blink-gpio/main.c`)

```c
#include <eos/hal.h>
#include <stdio.h>

#define LED_PIN 13

int main(void)
{
    eos_hal_init();

    eos_gpio_config_t led_cfg = {
        .pin   = LED_PIN,
        .mode  = EOS_GPIO_OUTPUT,
        .pull  = EOS_GPIO_PULL_NONE,
        .speed = EOS_GPIO_SPEED_LOW,
    };
    eos_gpio_init(&led_cfg);

    printf("[blink] Starting LED blink on pin %d\n", LED_PIN);

    while (1) {
        eos_gpio_write(LED_PIN, true);
        printf("[blink] LED ON\n");
        eos_delay_ms(500);

        eos_gpio_write(LED_PIN, false);
        printf("[blink] LED OFF\n");
        eos_delay_ms(500);
    }

    return 0;
}
```

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
project(blink-gpio LANGUAGES C)

get_filename_component(EOS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_subdirectory(${EOS_ROOT} ${CMAKE_BINARY_DIR}/eos)

add_executable(blink-gpio main.c)
target_link_libraries(blink-gpio PRIVATE eos_hal)
target_include_directories(blink-gpio PRIVATE
    ${EOS_ROOT}/hal/include
    ${EOS_ROOT}/include
)
```

### eos.yaml

```yaml
project:
  name: blink-gpio
  version: 0.1.0

workspace:
  backend: ninja
  build_dir: .eos/build

toolchain:
  target: host

system:
  kind: baremetal
  entry: main.c
  output: blink-gpio
```

### Build and Run

```bash
cd examples/blink-gpio
cmake -B build -DEOS_PRODUCT=iot
cmake --build build
./build/blink-gpio
```

Expected output:

```
[blink] Starting LED blink on pin 13
[blink] LED ON
[blink] LED OFF
[blink] LED ON
...
```

### How It Works on the Host

On a host build, the HAL backend maps peripheral calls to POSIX equivalents:

| HAL Function | Host Behavior |
|-------------|--------------|
| `eos_gpio_write(pin, val)` | Prints `GPIO <pin>: HIGH/LOW` |
| `eos_gpio_read(pin)` | Returns simulated value |
| `eos_uart_write(port, data, len)` | Writes to stdout |
| `eos_uart_read(port, buf, len, t)` | Reads from stdin with timeout |
| `eos_delay_ms(ms)` | Calls `usleep()` / `Sleep()` |
| `eos_get_tick_ms()` | Uses `clock_gettime()` / `QueryPerformanceCounter()` |

## 2.5 Quickstart: Host Build

The host build is the recommended starting point for all developers.

```bash
git clone https://github.com/embeddedos-org/eos.git
cd eos/examples/blink-gpio

cmake -B build -DEOS_PRODUCT=iot
cmake --build build
./build/blink-gpio
```

To build and run the full test suite:

```bash
cd eos
cmake -B build -DEOS_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Try all included examples:

```bash
cd examples
for example in blink-gpio uart-echo multitask-rtos posix-app multicore-amp; do
  echo "=== Building $example ==="
  cd $example
  cmake -B build -DEOS_PRODUCT=iot
  cmake --build build
  cd ..
done
```

## 2.6 Quickstart: STM32

### Hardware

- **Board:** STM32H743-Nucleo, STM32F4-Discovery, or any STM32 Nucleo/Discovery
- **Debugger:** Built-in ST-Link
- **Interface:** USB for power/programming, UART2 for serial output

### Build

```bash
cd eos/examples/blink-gpio

cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=../../toolchains/arm-none-eabi.cmake \
  -DEOS_PRODUCT=industrial
cmake --build build
```

### Flash via OpenOCD

```bash
# STM32H7 Nucleo:
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "program build/blink-gpio.bin 0x08000000 verify reset exit"
```

### Pin Mapping (STM32 Nucleo)

| EoS Function | STM32 Pin | Nucleo Label |
|-------------|-----------|-------------|
| LED | PA5 | LD2 (green) |
| UART2 TX | PA2 | D1 |
| UART2 RX | PA3 | D0 |
| Button | PC13 | B1 (blue) |
| SPI1 SCK | PA5 | D13 |
| I2C1 SDA | PB9 | D14 |
| I2C1 SCL | PB8 | D15 |

### Debugging with GDB

```bash
# Terminal 1: Start OpenOCD
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg

# Terminal 2: Connect GDB
arm-none-eabi-gdb build/blink-gpio.elf
(gdb) target remote :3333
(gdb) monitor reset halt
(gdb) load
(gdb) break main
(gdb) continue
```

## 2.7 Quickstart: Raspberry Pi 4

### Build Natively on Pi

```bash
sudo apt install cmake gcc g++ git
git clone https://github.com/embeddedos-org/eos.git
cd eos/examples/blink-gpio

cmake -B build -DEOS_PRODUCT=gateway
cmake --build build
./build/blink-gpio
```

### Cross-Compile from Host

```bash
sudo apt install gcc-aarch64-linux-gnu
cd eos/examples/blink-gpio

cmake -B build \
  -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
  -DEOS_PRODUCT=gateway
cmake --build build

scp build/blink-gpio pi@raspberrypi.local:~/
```

### GPIO Pin Mapping (BCM)

| BCM GPIO | Physical Pin | Common Use |
|----------|-------------|-----------|
| 2 | 3 | I2C SDA |
| 3 | 5 | I2C SCL |
| 4 | 7 | General GPIO |
| 14 | 8 | UART TX |
| 15 | 10 | UART RX |
| 17 | 11 | General GPIO |
| 18 | 12 | PWM |

For real GPIO control on the Pi, run with `sudo`:

```bash
sudo ./build/blink-gpio
```

## 2.8 Quickstart: nRF52

### Hardware

- **Board:** nRF52840-DK (PCA10056) or nRF52832-DK (PCA10040)
- **Debugger:** Built-in J-Link

### Prerequisites

```bash
sudo apt install gcc-arm-none-eabi
# Install Nordic nRF Command Line Tools from nordicsemi.com
```

### Build and Flash

```bash
cd eos/examples/blink-gpio

cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=../../toolchains/arm-none-eabi.cmake \
  -DEOS_PRODUCT=iot
cmake --build build

# Flash via J-Link
nrfjprog --program build/blink-gpio.hex --chiperase --verify
nrfjprog --reset
```

### Verify via Serial

```bash
screen /dev/ttyACM0 115200
```

### Pin Mapping (nRF52840-DK)

| EoS Pin | nRF52840 GPIO | Board Label | Function |
|---------|--------------|-------------|----------|
| 13 | P0.13 | LED1 | User LED |
| 14 | P0.14 | LED2 | User LED |
| 15 | P0.15 | LED3 | User LED |
| 16 | P0.16 | LED4 | User LED |
| 11 | P0.11 | Button 1 | User button |
| 6 | P0.06 | UART TX | Debug UART |
| 8 | P0.08 | UART RX | Debug UART |

## 2.9 Troubleshooting

| Problem | Solution |
|---------|----------|
| CMake version too old | Upgrade: `pip install cmake --upgrade` |
| `arm-none-eabi-gcc` not found | Install: `sudo apt install gcc-arm-none-eabi` |
| Host build: no output | Check that `eos_hal_init()` is called first |
| STM32: flash fails | Try erasing first, check USB cable |
| nRF52: device locked | Run `nrfjprog --recover` |
| RPi4: GPIO permission denied | Run with `sudo` or add user to `gpio` group |
| Link errors | Ensure `target_link_libraries` includes `eos_hal` |

## 2.10 Next Steps

Now that you have a working build, continue with:

- **Chapter 3** — Understand the layered architecture
- **Chapter 4** — Deep dive into the HAL API
- **Chapter 6** — Build multitasking applications with the kernel

---

*Next: [Chapter 3 — Architecture](ch03-architecture.md)*
