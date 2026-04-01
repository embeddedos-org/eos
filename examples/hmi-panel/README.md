# Industrial HMI Button Panel Example

Demonstrates the EoS UI framework on a **7-inch 800×480 industrial touchscreen** with motor control, temperature monitoring, and emergency stop.

## Features

- **Motor control:** Start/Stop/Reverse buttons with LED status indicator
- **Speed slider:** 0–100% motor speed with live readout
- **Temperature gauge:** Bar graph with color-coded alarm thresholds (green → yellow → red)
- **Status bar:** Live uptime, fault counter, connection status
- **Emergency stop:** Long-press (400ms) to activate/deactivate E-STOP
- **Dark theme:** Industrial-grade dark UI with high-contrast controls

## Target Hardware

| Component | Spec |
|---|---|
| Display | 800×480 7" TFT, SSD1963/RA8875, RGB565 |
| Touch | GT911 capacitive, I2C (0x5D), 5-point multi-touch |
| MCU | STM32H7, i.MX RT, AM64x, or any EoS-supported board |
| Product profile | `hmi`, `plc`, `industrial` |

## Build

```bash
cmake -B build -DEOS_PRODUCT=hmi -DEOS_BUILD_TESTS=OFF
cmake --build build
```

## UI Layout

```
┌────────────────────────────────────────────────────────────┐
│ Uptime: 01:01:01  |  Faults: 0  |  📶 Connected           │
├──────────────────────────────┬─────────────────────────────┤
│ ⚙ Motor Control             │                             │
│                   ● STOPPED  │                             │
│                              │      ┌───────────────┐     │
│  ┌──────────┐ ┌──────────┐  │      │               │     │
│  │▶ START   │ │■ STOP    │  │      │    E-STOP     │     │
│  └──────────┘ └──────────┘  │      │    (hold)     │     │
│  ┌──────────┐ Speed: 0%     │      │               │     │
│  │🔄 REVERSE│ ═══○═══════   │      └───────────────┘     │
│  └──────────┘               │                             │
├──────────────────────────────┤                             │
│ ⚡ Temperature               │                             │
│ ████████████░░░░░░░░░░░░░░  │                             │
│        42.5°C               │                             │
└──────────────────────────────┴─────────────────────────────┘
```

## Simulated Behavior

- Temperature rises when motor is running, cools when stopped
- Status bar updates every second (uptime counter)
- E-STOP disables all motor controls until released
