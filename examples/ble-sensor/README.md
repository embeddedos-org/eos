# BLE Sensor — Real IoT Application

A complete IoT sensor node that reads temperature from an I2C sensor, computes a CRC checksum, and advertises the data over BLE. This example demonstrates how multiple EoS modules work together in a real application.

## What it demonstrates

- BLE initialization, advertising, and data transmission
- I2C sensor communication (read temperature register)
- CRC32 checksum computation for data integrity
- Kernel task creation for periodic sensor sampling
- GPIO LED toggle for activity indication

## Modules used

| Module | Header | Functions |
|--------|--------|-----------|
| HAL Core | `eos/hal.h` | `eos_hal_init`, `eos_gpio_init`, `eos_gpio_toggle` |
| HAL I2C | `eos/hal.h` | `eos_i2c_init`, `eos_i2c_read_reg` |
| HAL BLE | `eos/hal_extended.h` | `eos_ble_init`, `eos_ble_advertise_start`, `eos_ble_send` |
| Kernel | `eos/kernel.h` | `eos_kernel_init`, `eos_kernel_start`, `eos_task_create`, `eos_task_delay_ms` |
| Crypto | `eos/crypto.h` | `eos_crc32` |

## Hardware

- **MCU:** nRF52840 (or any BLE-capable MCU)
- **Sensor:** I2C temperature sensor (TMP102, LM75, or similar) on I2C port 0
- **LED:** Connected to pin 13

## How to build

```bash
# Cross-compile for nRF52
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=../../toolchains/arm-none-eabi.cmake \
  -DEOS_PRODUCT=iot
cmake --build build

# Host build (simulated peripherals)
cmake -B build -DEOS_PRODUCT=iot
cmake --build build
./build/ble-sensor
```

## Expected output

```
[ble-sensor] BLE advertising as 'EoS-Sensor'
[ble-sensor] Sensor task started
[ble-sensor] Starting kernel...
[ble-sensor] TX seq=0 temp=23.50°C crc=0xa1b2c3d4
[ble-sensor] TX seq=1 temp=23.56°C crc=0xe5f6a7b8
...
```

## Data format

The BLE advertisement payload is a `sensor_packet_t`:

| Field | Type | Description |
|-------|------|-------------|
| `temperature_x100` | `int16_t` | Temperature in 0.01°C units (2350 = 23.50°C) |
| `sequence` | `uint32_t` | Monotonic packet counter |
| `crc` | `uint32_t` | CRC32 of the preceding fields |
