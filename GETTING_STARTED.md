# Getting Started with EoS

This guide takes you from zero to a running embedded application. Pick your path based on what hardware you have.

---

## Prerequisites

| Tool | Version | Purpose |
|------|---------|---------|
| **CMake** | ≥ 3.16 | Build system for eos and eboot |
| **Python** | ≥ 3.9 | Required for ebuild CLI |
| **GCC** (host) | ≥ 9.0 | For host builds and testing |
| **arm-none-eabi-gcc** | ≥ 10.0 | For ARM targets (nRF52, STM32) |
| **Git** | ≥ 2.0 | Clone the repository |

### Install prerequisites

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install cmake gcc g++ python3 python3-pip git
# For ARM targets:
sudo apt install gcc-arm-none-eabi
```

**macOS:**
```bash
brew install cmake python3 git
# For ARM targets:
brew install --cask gcc-arm-embedded
```

**Windows:**
```powershell
# Install via winget or download installers
winget install CMake.CMake Python.Python.3.12 Git.Git
# For ARM targets, download from https://developer.arm.com/downloads/-/gnu-rm
```

---

## Path 1: "I have an nRF52"

Best for: nRF52840-DK, nRF52832-DK, or custom nRF52 boards.

### 1. Clone the repository
```bash
git clone https://github.com/anthropic/EoS.git
cd EoS
```

### 2. Build the blink example
```bash
cd eos/examples/blink-gpio
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=../../toolchains/arm-none-eabi.cmake \
  -DEOS_PRODUCT=iot
cmake --build build
```

### 3. Flash to your board
```bash
# Using J-Link (nRF52-DK has built-in J-Link)
nrfjprog --program build/blink-gpio.hex --chiperase --verify
nrfjprog --reset
```

### 4. Verify output
Connect a USB cable and open a serial terminal (115200 baud):
```bash
# Linux
screen /dev/ttyACM0 115200
# macOS
screen /dev/cu.usbmodem* 115200
# Windows
# Use PuTTY or Tera Term on the COM port
```

Expected output:
```
[EoS] HAL initialized
[EoS] LED ON  (pin 13)
[EoS] LED OFF (pin 13)
[EoS] LED ON  (pin 13)
...
```

→ Next: Try the [ble-sensor](eos/examples/ble-sensor/) example or [add eboot secure boot](#adding-eboot-secure-boot).

---

## Path 2: "I have an STM32"

Best for: STM32H743-Nucleo, STM32F4-Discovery, or custom STM32 boards.

### 1. Clone and build
```bash
git clone https://github.com/anthropic/EoS.git
cd EoS/eos/examples/blink-gpio

cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=../../toolchains/arm-none-eabi.cmake \
  -DEOS_PRODUCT=industrial
cmake --build build
```

### 2. Flash via ST-Link
```bash
# Using OpenOCD
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "program build/blink-gpio.bin 0x08000000 verify reset exit"

# Or using STM32CubeProgrammer
STM32_Programmer_CLI -c port=SWD -w build/blink-gpio.bin 0x08000000 -v -rst
```

### 3. Verify on UART
Connect UART2 (PA2/PA3) to a USB-UART adapter and open at 115200 baud.

→ See the full [STM32 quickstart guide](docs/quickstart-stm32.md).

---

## Path 3: "I just want to try it" (No Hardware)

Build and run on your development machine — no MCU needed.

### 1. Clone and build
```bash
git clone https://github.com/anthropic/EoS.git
cd EoS/eos/examples/blink-gpio

# Host build uses Linux HAL backend (simulated GPIO via sysfs/printf)
cmake -B build -DEOS_PRODUCT=iot -DEOS_BUILD_TESTS=ON
cmake --build build
```

### 2. Run the example
```bash
./build/blink-gpio
```

Output:
```
[EoS] HAL initialized (Linux backend)
[EoS] GPIO 13: HIGH
[EoS] GPIO 13: LOW
[EoS] GPIO 13: HIGH
...
```

### 3. Run the test suite
```bash
cd ../../
cmake -B build -DEOS_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

→ See the full [host build quickstart](docs/quickstart-host.md).

---

## Your First Application

Now that you have the toolchain working, let's write a real application.

### GPIO Blink + UART Echo

Create a new project:
```bash
# Option A: Using ebuild scaffolding
cd EoS/ebuild && pip install -e .
ebuild new my-app --template bare-metal --board nrf52
cd my-app

# Option B: Manual setup
mkdir my-app && cd my-app
```

Create `main.c`:
```c
#include <eos/hal.h>
#include <stdio.h>

#define LED_PIN   13
#define UART_PORT 0

int main(void) {
    /* Initialize HAL */
    eos_hal_init();

    /* Configure LED as output */
    eos_gpio_config_t led = {
        .pin  = LED_PIN,
        .mode = EOS_GPIO_OUTPUT,
        .pull = EOS_GPIO_PULL_NONE,
        .speed = EOS_GPIO_SPEED_LOW,
    };
    eos_gpio_init(&led);

    /* Configure UART for debug output */
    eos_uart_config_t uart = {
        .port      = UART_PORT,
        .baudrate  = 115200,
        .data_bits = 8,
        .parity    = EOS_UART_PARITY_NONE,
        .stop_bits = EOS_UART_STOP_1,
    };
    eos_uart_init(&uart);

    const char *msg = "Hello from EoS!\r\n";
    eos_uart_write(UART_PORT, (const uint8_t *)msg, 17);

    /* Main loop: blink + echo */
    while (1) {
        eos_gpio_toggle(LED_PIN);

        uint8_t byte;
        if (eos_uart_read(UART_PORT, &byte, 1, 100) == 1) {
            eos_uart_write(UART_PORT, &byte, 1);  /* echo back */
        }

        eos_delay_ms(500);
    }

    return 0;
}
```

Build and run:
```bash
cmake -B build -DEOS_PRODUCT=iot
cmake --build build
./build/my-app       # host build
# or flash to MCU    # target build
```

---

## Adding eboot Secure Boot

Once your application works, add secure boot with eboot:

### 1. Build eboot for your board
```bash
cd EoS/eboot
cmake -B build -DEBLDR_BOARD=nrf52
cmake --build build
```

### 2. Generate flash layout
```bash
cd EoS/ebuild
ebuild generate-boot _generated/boot.yaml --output-dir _generated
```

This produces:
- `eboot_flash_layout.h` — flash addresses for Stage-0, Stage-1, Slot A/B
- `eboot_linker.ld` — linker script placing firmware in the correct slot
- `eboot_pack.sh` — script to create a signed firmware image

### 3. Sign and flash
```bash
# Create signing key (first time only)
cd EoS/eboot/tools
python3 sign_image.py --generate-key my-key.pem

# Sign firmware
python3 sign_image.py --key my-key.pem --input ../build/eboot.bin --output signed-fw.bin

# Flash both eboot + signed firmware
nrfjprog --program build/eboot.hex --chiperase
nrfjprog --program signed-fw.hex --verify
nrfjprog --reset
```

→ See the full [Integration Guide](docs/integration-guide.md) for the complete eos + eboot + ebuild workflow.

---

## Using ebuild AI

Describe your hardware in plain text, and ebuild generates all configs:

```bash
cd EoS/ebuild && pip install -e .

# Describe your hardware
ebuild analyze "nRF52840 BLE sensor with I2C temperature, SPI flash, 1MB flash, 256KB RAM"

# Generate a complete stripped-down project
ebuild generate-project \
  --text "nRF52840 BLE sensor with I2C temperature and SPI flash" \
  --output my-sensor-project
```

This generates:
- `board.yaml` — MCU, peripherals, memory map
- `boot.yaml` — flash layout, boot policy
- `eos_product_config.h` — `EOS_ENABLE_*` flags for your hardware
- `eboot_flash_layout.h` — flash addresses for bootloader

→ See the full [EoS AI Guide](ebuild/docs/eos_ai_guide.md).

---

## What's Next?

| Goal | Guide |
|------|-------|
| Explore more examples | [examples/](examples/) |
| Understand the architecture | [docs/architecture.md](docs/architecture.md) |
| Choose a product profile | [Choosing a Profile](docs/choosing-a-product-profile.md) |
| Full API reference | [API Reference](docs/api-reference.md) |
| Troubleshooting | [FAQ & Common Issues](docs/troubleshooting.md) |
| Contributing | [CONTRIBUTING.md](CONTRIBUTING.md) |
