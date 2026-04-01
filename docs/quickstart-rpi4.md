# Quickstart: Raspberry Pi 4

Run EoS on a Raspberry Pi 4 using the Linux HAL backend.

---

## Hardware

- **Board:** Raspberry Pi 4 Model B (any RAM variant)
- **OS:** Raspberry Pi OS (Bullseye or later) or Ubuntu Server
- **Interface:** SSH, serial console, or direct HDMI

## Prerequisites

On your Raspberry Pi (or cross-compile host):
```bash
sudo apt update
sudo apt install cmake gcc g++ git python3 python3-pip
```

---

## Step 1: Clone and build

```bash
# On the Raspberry Pi:
git clone https://github.com/anthropic/EoS.git
cd EoS/eos/examples/blink-gpio

# Build using the Linux HAL backend (native compile on Pi)
cmake -B build -DEOS_PRODUCT=gateway
cmake --build build
```

## Step 2: Run

```bash
./build/blink-gpio
```

Output:
```
[blink] Starting LED blink on pin 13
[blink] LED ON
[blink] LED OFF
...
```

On the Raspberry Pi, the Linux HAL backend uses sysfs GPIO. To control actual GPIO pins, run with `sudo`:
```bash
sudo ./build/blink-gpio
```

## Step 3: Try a real GPIO blink

Edit `main.c` and change `LED_PIN` to a physical GPIO pin:
```c
#define LED_PIN 18  /* BCM GPIO 18 = physical pin 12 */
```

Connect an LED + resistor (330Ω) between GPIO 18 and GND, then run.

---

## Cross-compiling from a host PC

```bash
# Install cross-compiler
sudo apt install gcc-aarch64-linux-gnu

# Build
cd EoS/eos/examples/blink-gpio
cmake -B build \
  -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
  -DEOS_PRODUCT=gateway
cmake --build build

# Copy to Pi
scp build/blink-gpio pi@raspberrypi.local:~/
```

## GPIO pin mapping (BCM)

| BCM GPIO | Physical Pin | Common Use |
|----------|-------------|-----------|
| 2 | 3 | I2C SDA |
| 3 | 5 | I2C SCL |
| 4 | 7 | General GPIO |
| 14 | 8 | UART TX |
| 15 | 10 | UART RX |
| 17 | 11 | General GPIO |
| 18 | 12 | PWM / General GPIO |
| 27 | 13 | General GPIO |

## Next steps

- Try the [POSIX app example](../../examples/posix-app/) — pthreads + message queues
- Build a [Linux app](../../GETTING_STARTED.md) with networking
- Create a project: `ebuild new my-gateway --template linux-app --board rpi4`

## Troubleshooting

| Problem | Solution |
|---------|----------|
| GPIO permission denied | Run with `sudo` or add user to `gpio` group |
| Can't find GPIO in sysfs | Enable GPIO in `/boot/config.txt`, reboot |
| Cross-compile fails | Ensure `aarch64-linux-gnu-gcc` is installed |
