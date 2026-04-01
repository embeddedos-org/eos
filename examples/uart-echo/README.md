# UART Echo — Basic Serial Communication

Reads bytes from UART and echoes them back. The simplest way to verify that serial communication works between your board and a host PC.

## What it demonstrates

- UART initialization with `eos_uart_init()`
- Writing strings with `eos_uart_write()`
- Blocking read with timeout using `eos_uart_read()`
- Echo loop pattern

## Modules used

| Module | Header | Functions |
|--------|--------|-----------|
| HAL | `eos/hal.h` | `eos_hal_init`, `eos_uart_init`, `eos_uart_write`, `eos_uart_read` |

## How to build

```bash
cmake -B build -DEOS_PRODUCT=iot
cmake --build build
./build/uart-echo
```

## Expected output

On the serial terminal (115200 baud, 8N1):
```
=== EoS UART Echo ===
Type characters and they will be echoed back.
Running at 115200 baud, 8N1
```

Every character you type is immediately echoed back.

## Hardware connections

| Board | UART TX | UART RX | Notes |
|-------|---------|---------|-------|
| nRF52840-DK | P0.06 | P0.08 | USB-UART via J-Link |
| STM32 Nucleo | PA2 | PA3 | UART2, connect USB-UART adapter |
| Host build | stdout | stdin | Simulated via printf |
