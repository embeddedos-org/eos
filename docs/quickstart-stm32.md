# Quickstart: STM32 (ST Microelectronics)

Get EoS running on an STM32 Nucleo or Discovery board.

---

## Hardware

- **Board:** STM32H743-Nucleo, STM32F4-Discovery, or any STM32 Nucleo/Discovery board
- **Debugger:** Built-in ST-Link (on Nucleo/Discovery boards)
- **Interface:** USB for power and programming, UART2 for serial output

## Prerequisites

```bash
# Install ARM toolchain
sudo apt install gcc-arm-none-eabi    # Ubuntu/Debian
brew install --cask gcc-arm-embedded  # macOS

# Install OpenOCD (for flashing and debugging)
sudo apt install openocd              # Ubuntu/Debian
brew install openocd                  # macOS

# Alternative: Install STM32CubeProgrammer from ST website
```

---

## Step 1: Clone and build

```bash
git clone https://github.com/anthropic/EoS.git
cd EoS/eos/examples/blink-gpio

cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=../../toolchains/arm-none-eabi.cmake \
  -DEOS_PRODUCT=industrial
cmake --build build
```

## Step 2: Flash via OpenOCD

```bash
# STM32H7 Nucleo:
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "program build/blink-gpio.bin 0x08000000 verify reset exit"

# STM32F4 Discovery:
openocd -f interface/stlink-v2.cfg -f target/stm32f4x.cfg \
  -c "program build/blink-gpio.bin 0x08000000 verify reset exit"
```

Or via STM32CubeProgrammer:
```bash
STM32_Programmer_CLI -c port=SWD -w build/blink-gpio.bin 0x08000000 -v -rst
```

## Step 3: Verify output

Connect UART2 pins to a USB-UART adapter:
- **PA2** → UART adapter RX
- **PA3** → UART adapter TX
- **GND** → GND

Open serial terminal at **115200 baud**.

---

## Pin mapping (STM32 Nucleo)

| EoS Function | STM32 Pin | Nucleo Label |
|-------------|-----------|-------------|
| LED | PA5 | LD2 (green LED) |
| UART2 TX | PA2 | D1 |
| UART2 RX | PA3 | D0 |
| Button | PC13 | B1 (blue button) |
| SPI1 SCK | PA5 | D13 |
| I2C1 SDA | PB9 | D14 |
| I2C1 SCL | PB8 | D15 |

## Debugging with OpenOCD + GDB

```bash
# Terminal 1: Start OpenOCD server
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg

# Terminal 2: Connect GDB
arm-none-eabi-gdb build/blink-gpio.elf
(gdb) target remote :3333
(gdb) monitor reset halt
(gdb) load
(gdb) break main
(gdb) continue
```

## Troubleshooting

| Problem | Solution |
|---------|----------|
| `openocd` can't connect | Check USB cable, install ST-Link drivers |
| No serial output | Verify UART2 pin connections and baud rate |
| Flash verification fails | Try erasing first: add `-c "flash erase_address 0x08000000 0x100000"` |
| LED doesn't blink | Change `LED_PIN` to 5 (PA5) for Nucleo boards |
