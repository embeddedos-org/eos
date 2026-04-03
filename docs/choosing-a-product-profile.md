# Choosing a Product Profile

EoS provides 41 pre-defined product profiles. Each profile enables exactly the peripherals and services needed for a specific device category, keeping binary size minimal.

---

## Decision Tree

```
What are you building?
│
├─ Uses wireless (WiFi/BLE/Cellular)?
│   ├─ BLE only (wearable/beacon) ──────────► watch, wearable, fitness
│   ├─ WiFi + BLE ──────────────────────────► iot, smart_home, voice
│   ├─ Cellular ────────────────────────────► mobile, telecom
│   └─ WiFi + Ethernet ────────────────────► gateway, router
│
├─ Has motors or actuators?
│   ├─ Industrial robot ────────────────────► robot
│   ├─ Drone / UAV ─────────────────────────► drone
│   ├─ Electric vehicle ────────────────────► ev
│   ├─ 3D printer / CNC ───────────────────► printer
│   └─ Robot vacuum ────────────────────────► vacuum
│
├─ Has display / camera / GPU?
│   ├─ AR/VR headset ───────────────────────► xr_headset
│   ├─ Gaming console ──────────────────────► gaming
│   ├─ Smart TV ─────────────────────────────► smart_tv
│   ├─ Security camera ─────────────────────► security_cam
│   ├─ Industrial HMI ──────────────────────► hmi
│   └─ Cockpit display ─────────────────────► cockpit
│
├─ Safety-critical / certified?
│   ├─ Medical device ──────────────────────► medical, diagnostic
│   ├─ Aerospace ───────────────────────────► aerospace, satellite
│   ├─ Automotive ECU ──────────────────────► automotive
│   └─ Industrial PLC ──────────────────────► plc, industrial
│
├─ Financial / security?
│   ├─ Payment terminal ────────────────────► pos, banking
│   └─ Hardware security module ────────────► crypto_hw
│
└─ Server / compute?
    ├─ Cloud / VM ──────────────────────────► server
    └─ AI edge / neural compute ────────────► ai_edge
```

---

## Profile Comparison Table

| Profile | Key Peripherals | Typical MCU | Binary Size (approx.) |
|---------|----------------|-------------|----------------------|
| `iot` | WiFi, BLE, ADC | nRF52, ESP32 | 64 KB |
| `watch` | Display, BLE, IMU, ADC | nRF52, STM32L4 | 48 KB |
| `wearable` | ADC, BLE, NFC, IMU, Display | nRF52 | 56 KB |
| `automotive` | CAN, Ethernet, ADC, PWM, Safety | STM32H7 | 128 KB |
| `medical` | ADC, BLE, Display, Crypto, Safety | STM32L4 | 96 KB |
| `drone` | Motor, Camera, GPS, Radar, WiFi | STM32H7 | 160 KB |
| `robot` | Motor, IMU, Camera, WiFi, PWM | i.MX8M | 192 KB |
| `gateway` | Ethernet, WiFi, BLE, CAN, Crypto | STM32H7 | 128 KB |
| `server` | GPU, PCIe, Ethernet, Crypto, Multicore | x86_64 | 256 KB |
| `gaming` | GPU, HDMI, PCIe, Haptics, Multicore | custom SoC | 320 KB |
| `satellite` | ADC, GNSS, IMU, Crypto, Low-power | SPARC, ARM | 80 KB |
| `smart_home` | WiFi, BLE, Ethernet, IR, Audio | ESP32 | 96 KB |

---

## Using a Profile

### In CMake

```bash
cmake -B build -DEOS_PRODUCT=iot
```

### In code

```c
/* eos/include/eos/eos_config.h automatically includes the profile */
#include <eos/eos_config.h>

/* Now EOS_ENABLE_* flags are set based on the profile */
#if EOS_ENABLE_BLE
    eos_ble_init(&ble_cfg);
#endif
```

---

## Creating a Custom Profile

If none of the 41 profiles match your hardware:

### Step 1: Create a profile header

```c
/* eos/products/my_device.h */
#define EOS_PRODUCT_NAME      "my_device"

/* Enable only what you need */
#define EOS_ENABLE_GPIO        1
#define EOS_ENABLE_UART        1
#define EOS_ENABLE_SPI         1
#define EOS_ENABLE_I2C         1
#define EOS_ENABLE_BLE         1
#define EOS_ENABLE_DISPLAY     1
#define EOS_ENABLE_ADC         1

/* Disable everything else */
#define EOS_ENABLE_CAN         0
#define EOS_ENABLE_USB         0
#define EOS_ENABLE_ETHERNET    0
#define EOS_ENABLE_WIFI        0
#define EOS_ENABLE_CAMERA      0
#define EOS_ENABLE_MULTICORE   0
```

### Step 2: Register in eos_config.h

```c
/* eos/include/eos/eos_config.h */
#elif defined(EOS_PRODUCT_MY_DEVICE)
#   include "products/my_device.h"
```

### Step 3: Build

```bash
cmake -B build -DEOS_PRODUCT=my_device
cmake --build build
```

---

## Tips

- **Start small** — pick the closest profile and customize
- **Disable unused peripherals** — reduces binary size and attack surface
- **Use `ebuild analyze`** — describe your hardware and let AI suggest the profile:
  ```bash
  ebuild analyze "Custom board with STM32H7, CAN, Ethernet, 2MB flash"
  ```
