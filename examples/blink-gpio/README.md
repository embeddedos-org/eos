# Blink GPIO — Simplest EoS Application

Toggles an LED on and off every 500ms. This is the "Hello World" of EoS.

## What it demonstrates

- HAL initialization with `eos_hal_init()`
- GPIO output configuration with `eos_gpio_init()`
- GPIO write with `eos_gpio_write()`
- Delay with `eos_delay_ms()`

## Modules used

| Module | Header | Functions |
|--------|--------|-----------|
| HAL | `eos/hal.h` | `eos_hal_init`, `eos_gpio_init`, `eos_gpio_write`, `eos_delay_ms` |

## How to build

```bash
# Host build (no hardware needed)
cmake -B build -DEOS_PRODUCT=iot
cmake --build build
./build/blink-gpio

# ARM cross-compile
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=../../toolchains/arm-none-eabi.cmake \
  -DEOS_PRODUCT=iot
cmake --build build
```

## Expected output

```
[blink] Starting LED blink on pin 13
[blink] LED ON
[blink] LED OFF
[blink] LED ON
[blink] LED OFF
...
```

On real hardware, the LED connected to pin 13 will blink at 1 Hz.

## Customization

Change `LED_PIN` in `main.c` to match your board's LED pin:
- nRF52840-DK: pin 13 (LED1)
- STM32 Nucleo: pin 5 (LD2)
- Raspberry Pi Pico: pin 25
