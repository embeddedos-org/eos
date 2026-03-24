# Quickstart: nRF52 (Nordic Semiconductor)

Get EoS running on an nRF52840-DK or nRF52832-DK in under 15 minutes.

---

## Hardware

- **Board:** nRF52840-DK (PCA10056) or nRF52832-DK (PCA10040)
- **Debugger:** Built-in J-Link (no external probe needed)
- **Interface:** USB for power, programming, and UART

## Prerequisites

```bash
# Install ARM toolchain
# Ubuntu/Debian:
sudo apt install gcc-arm-none-eabi

# macOS:
brew install --cask gcc-arm-embedded

# Install Nordic command-line tools
# Download from: https://www.nordicsemi.com/Products/Development-tools/nRF-Command-Line-Tools
```

Verify installation:
```bash
arm-none-eabi-gcc --version    # Should print 10.x or later
nrfjprog --version              # Should print 10.x or later
```

---

## Step 1: Clone and build

```bash
git clone https://github.com/anthropic/EoS.git
cd EoS/eos/examples/blink-gpio

cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=../../toolchains/arm-none-eabi.cmake \
  -DEOS_PRODUCT=iot
cmake --build build
```

## Step 2: Connect your board

1. Plug the nRF52-DK into your PC via the USB connector labeled **J2** (debug USB)
2. The board should appear as a J-Link device

Verify connection:
```bash
nrfjprog --ids
# Should print the serial number of your board
```

## Step 3: Flash

```bash
nrfjprog --program build/blink-gpio.hex --chiperase --verify
nrfjprog --reset
```

## Step 4: Verify output

Open a serial terminal at **115200 baud**:

```bash
# Linux
screen /dev/ttyACM0 115200

# macOS
screen /dev/cu.usbmodem* 115200

# Windows — use PuTTY or Tera Term on the appropriate COM port
```

Expected output:
```
[blink] Starting LED blink on pin 13
[blink] LED ON
[blink] LED OFF
...
```

You should also see **LED1** on the board blinking at 1 Hz.

---

## Pin mapping (nRF52840-DK)

| EoS Pin | nRF52840 GPIO | Board Label | Function |
|---------|--------------|-------------|----------|
| 13 | P0.13 | LED1 | User LED |
| 14 | P0.14 | LED2 | User LED |
| 15 | P0.15 | LED3 | User LED |
| 16 | P0.16 | LED4 | User LED |
| 11 | P0.11 | Button 1 | User button |
| 6 | P0.06 | UART TX | Debug UART |
| 8 | P0.08 | UART RX | Debug UART |

## Next steps

- Try the [BLE sensor example](../../examples/ble-sensor/) — uses BLE + I2C
- Add [eboot secure boot](../../../GETTING_STARTED.md#adding-eboot-secure-boot)
- Create a new project: `ebuild new my-app --template ble-sensor --board nrf52`

## Troubleshooting

| Problem | Solution |
|---------|----------|
| `nrfjprog` not found | Install Nordic command-line tools |
| No serial output | Check baud rate is 115200. Ensure USB cable supports data (not charge-only) |
| LED doesn't blink | Verify `LED_PIN` matches your board. nRF52840-DK uses pin 13 |
| Flash fails | Try `nrfjprog --recover` to unlock a protected device |
