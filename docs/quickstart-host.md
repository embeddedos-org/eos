# Quickstart: Host Build (No Hardware Required)

Build and test EoS on your development machine — Linux, macOS, or Windows. No MCU needed.

---

## Why host build?

- **Explore the API** without buying hardware
- **Run unit tests** to verify EoS kernel and services
- **Develop application logic** before targeting real hardware
- **CI/CD** — run automated tests on any build server

The host build uses the **Linux HAL backend** which simulates GPIO, UART, and other peripherals via printf/stdin.

## Prerequisites

```bash
# Ubuntu/Debian
sudo apt install cmake gcc g++ git

# macOS
brew install cmake

# Windows (with Visual Studio or MinGW)
winget install CMake.CMake
```

---

## Step 1: Build an example

```bash
git clone https://github.com/anthropic/EoS.git
cd EoS/eos/examples/blink-gpio

cmake -B build -DEOS_PRODUCT=iot
cmake --build build
```

## Step 2: Run it

```bash
./build/blink-gpio
```

Output:
```
[blink] Starting LED blink on pin 13
[blink] LED ON
[blink] LED OFF
[blink] LED ON
...
```

Press `Ctrl+C` to stop.

## Step 3: Run the full test suite

```bash
cd EoS/eos
cmake -B build -DEOS_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## Try all examples

```bash
cd EoS/eos/examples

# Each example builds the same way:
for example in blink-gpio uart-echo multitask-rtos posix-app multicore-amp; do
  echo "=== Building $example ==="
  cd $example
  cmake -B build -DEOS_PRODUCT=iot
  cmake --build build
  cd ..
done
```

## How the host build works

| HAL Function | Host Behavior |
|-------------|--------------|
| `eos_gpio_write(pin, val)` | Prints `GPIO <pin>: HIGH/LOW` |
| `eos_gpio_read(pin)` | Returns simulated value |
| `eos_uart_write(port, data, len)` | Writes to stdout |
| `eos_uart_read(port, buf, len, timeout)` | Reads from stdin (with timeout) |
| `eos_delay_ms(ms)` | Calls `usleep()` / `Sleep()` |
| `eos_get_tick_ms()` | Uses `clock_gettime()` / `QueryPerformanceCounter()` |

## Moving to real hardware

When you're ready to target an MCU:

1. Install the cross-compiler (`arm-none-eabi-gcc`)
2. Add `-DCMAKE_TOOLCHAIN_FILE=../../toolchains/arm-none-eabi.cmake` to your cmake command
3. Flash the resulting binary to your board

Your application code stays the same — only the build command changes.

## Next steps

- Pick a board: [nRF52](quickstart-nrf52.md), [STM32](quickstart-stm32.md), [RPi4](quickstart-rpi4.md)
- Create a project: `ebuild new my-app --template bare-metal --board generic`
- Read the [API Reference](api-reference.md)
