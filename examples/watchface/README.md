# Smartwatch Watchface Example

Demonstrates the EoS UI framework on a **round 240×240 display** (typical smartwatch form factor).

## Features

- **Digital face:** Large time display, date, battery arc, step counter bar, heart rate
- **Analog face:** Clock hands (hour/minute/second), hour markers, center dot
- **Swipe gestures:** Left/right to switch between faces, up/down for notifications/apps
- **Auto-update:** 1-second timer ticks the clock and simulates step counter

## Target Hardware

| Component | Spec |
|---|---|
| Display | 240×240 round, GC9A01 SPI, RGB565 |
| Touch | CST816S capacitive, I2C (0x15) |
| MCU | Any with EoS HAL (nRF52, ESP32, STM32, etc.) |
| Product profile | `watch`, `wearable`, `fitness` |

## Build

```bash
cmake -B build -DEOS_PRODUCT=watch -DEOS_BUILD_TESTS=OFF
cmake --build build
```

## UI Structure

```
┌──────────────────────┐     ┌──────────────────────┐
│    ❤ 72 bpm    73%   │     │  ·   12  ·           │
│                ████  │     │         │             │
│                      │     │  ·      │        ·    │
│     10:42            │ ←→  │ 9 ──────┼──── 3  27  │
│   :15                │     │  ·       \       ·    │
│   Thu, Mar 27        │     │           \           │
│                      │     │  ·    ·    ·          │
│  ███████░░░░░░░░░░   │     │         6             │
│  🔀 6280 steps       │     │                       │
└──────────────────────┘     └──────────────────────┘
    Digital Face                  Analog Face
         ← Swipe Left/Right →
```
