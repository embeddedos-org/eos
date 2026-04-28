# 🚀 EoS — Embedded Operating System

[![CI](https://github.com/embeddedos-org/eos/actions/workflows/ci.yml/badge.svg)](https://github.com/embeddedos-org/eos/actions/workflows/ci.yml)
[![Nightly](https://github.com/embeddedos-org/eos/actions/workflows/nightly.yml/badge.svg)](https://github.com/embeddedos-org/eos/actions/workflows/nightly.yml)
[![Release](https://github.com/embeddedos-org/eos/actions/workflows/release.yml/badge.svg)](https://github.com/embeddedos-org/eos/actions/workflows/release.yml)
[![Version](https://img.shields.io/github/v/tag/embeddedos-org/eos?label=version)](https://github.com/embeddedos-org/eos/releases/latest)

**Universal embedded OS framework — compile once, run on any hardware**

EoS is a modular, portable operating system framework supporting **41 product categories** across every hardware class — from smartwatches to spacecraft, gaming consoles to 5G base stations, medical devices to autonomous vehicles.

---


## What's New in 0.5.0

| Feature | Description |
|---|---|
| **Firmware build pipeline** | End-to-end firmware assembly from source to deployable image |
| **Build scheduler** | Parallel build orchestration with dependency-aware caching |
| **41 product profiles** | Full coverage across automotive, medical, aerospace, consumer, industrial, networking, financial, server, and HMI |
| **`backend.h`** | Unified platform backend abstraction header for Linux and RTOS targets |
| **`package.h`** | Package metadata and dependency declaration header for modular builds |
| **UI module** | Optional LVGL-based UI service for display-equipped products (`EOS_ENABLE_UI`) |
| **CI tests enabled** | Unit test suites now run automatically in CI across all 3 platforms |
| **Multicore SMP/AMP** | Enhanced multicore scheduling with per-core load balancing |
| **33 HAL peripherals** | Complete hardware abstraction layer with conditional compilation |
| **Cross-compilation** | CMake toolchain files for AArch64, ARM hard-float, and RISC-V 64 |

---
## ⚡ Quick Start

```bash
# Build for a specific product
cmake -B build -DEOS_PRODUCT=robot -DEOS_BUILD_TESTS=ON
cmake --build build

# Build everything (development mode)
cmake -B build -DEOS_BUILD_TESTS=ON
cmake --build build
```

→ **New to EoS?** See the [Getting Started Guide](GETTING_STARTED.md) for a full walkthrough with 3 hardware paths.

→ **No hardware?** Try the [Host Build Quickstart](docs/quickstart-host.md) — runs on Linux/macOS/Windows.

→ **Pick a board:** [nRF52](docs/quickstart-nrf52.md) · [STM32](docs/quickstart-stm32.md) · [Raspberry Pi 4](docs/quickstart-rpi4.md)

---

## 🎯 Supported Product Categories (41 Profiles)

| Profile | Use Case | Key Hardware |
|---|---|---|
| **Automotive & Transport** | | |
| `automotive` | ECU / MCU devices | CAN, Ethernet, ADC, PWM, Motor, Safety |
| `cockpit` | Cockpit display systems | HDMI, GPU, Touch, CAN, IMU, GNSS, Multicore |
| `ev` | Electric vehicles | CAN, Motor, BMS, GPS, Cellular, Multicore |
| `autonomous` | Autonomous driving / ADAS | Camera, Radar, Lidar, GPU, PCIe, Multicore |
| `infotainment` | In-vehicle infotainment | HDMI, GPU, Touch, Audio, NFC, Cellular, Multicore |
| **Industrial & Manufacturing** | | |
| `industrial` | Industrial controllers | CAN, Ethernet, ADC, DAC, Safety |
| `plc` | Programmable Logic Controllers | CAN, ADC, DAC, PWM, Motor, Safety |
| `robot` | Industrial / service robots | Motor, IMU, Camera, WiFi, PWM, ADC |
| `vacuum` | Robot vacuum cleaners | Motor, ADC, PWM, BLE |
| `printer` | 3D printers / CNC machines | Motor, Stepper, ADC, USB, WiFi |
| **Medical & Health** | | |
| `medical` | Medical monitoring devices | ADC, BLE, Display, Crypto, Safety, Audit |
| `diagnostic` | Diagnostic / imaging machines | ADC, DAC, Camera, Ethernet, Touch, Safety |
| `telemedicine` | Remote patient monitoring | ADC, Camera, Audio, Cellular, WiFi, BLE |
| `fitness` | Fitness trackers / sports bands | IMU, ADC, BLE, Display, Haptics |
| `wearable` | Wearable health monitors | ADC, BLE, NFC, IMU, Touch, Display |
| **Space & Aerospace** | | |
| `aerospace` | Aircraft flight controllers | CAN, IMU, GNSS, Safety, Redundancy |
| `satellite` | Satellites / spacecraft | ADC, GNSS, IMU, Crypto, Low-power |
| `ground_control` | Ground control stations | HDMI, GPU, GNSS, Ethernet, Multicore |
| `space_comm` | Space communication networks | DAC, Crypto, Redundancy, Safety |
| `drone` | Camera drones / delivery drones | Motor, Camera, GPS, Radar, WiFi |
| **Consumer Electronics** | | |
| `mobile` | Smartphones / tablets | Display, Camera, Audio, WiFi, BLE, USB, GPS |
| `watch` | Smartwatches | Display, BLE, IMU, ADC, Low-power |
| `smart_tv` | Smart TVs / streaming | HDMI, GPU, IR, WiFi, Audio, Multicore |
| `gaming` | Gaming consoles / handhelds | GPU, HDMI, PCIe, Haptics, WiFi, Multicore |
| `computer` | Laptops / PCs / SBCs | GPU, HDMI, PCIe, USB, NFC, Multicore |
| `xr_headset` | AR/VR/MR headsets | GPU, HDMI, IMU, Camera, Haptics, Multicore |
| `voice` | Smart speakers / voice assistants | Audio, WiFi, BLE |
| `smart_home` | Home automation hubs | WiFi, BLE, Ethernet, IR, Audio, Touch |
| `thermostat` | Smart thermostats / HVAC | ADC, PWM, WiFi, IR, Display, Touch |
| `security_cam` | IP security cameras / DVR | Camera, WiFi, Ethernet, IR, Radar, SDIO |
| **Networking & Telecom** | | |
| `router` | Routers / switches / firewalls | Ethernet, WiFi, PCIe, Crypto, WDT |
| `gateway` | IoT gateways / edge devices | Ethernet, WiFi, BLE, CAN, Crypto, OTA |
| `telecom` | 5G base stations / telecom | Cellular, Ethernet, PCIe, Multicore |
| `adapter` | USB / network adapters | USB, Ethernet, WiFi, BLE |
| `iot` | IoT sensor nodes | ADC, WiFi, BLE, Cellular, Low-power |
| **Financial & Security** | | |
| `banking` | Banking / ATM platforms | NFC, Touch, Display, Crypto, Security, Audit |
| `pos` | Payment / POS terminals | NFC, Touch, Cellular, Crypto, Audit |
| `crypto_hw` | HSM / blockchain accelerators | PCIe, DMA, Crypto, Security, Audit |
| **Server & AI** | | |
| `server` | Servers / cloud / VMs / databases | GPU, PCIe, Ethernet, Crypto, Multicore |
| `ai_edge` | AI edge / vision / neural compute | GPU, PCIe, Camera, DMA, Multicore |
| **HMI & Display** | | |
| `hmi` | Industrial touchscreen panels | Display, Touch, Ethernet, CAN, Audio |

### Custom Product Profiles

Create a custom profile for any hardware:

```c
/* products/my_device.h */
#define EOS_PRODUCT_NAME    "my_device"
#define EOS_ENABLE_GPIO      1
#define EOS_ENABLE_UART      1
#define EOS_ENABLE_BLE       1
#define EOS_ENABLE_DISPLAY   1
#define EOS_ENABLE_MULTICORE 1
```

Then add to `eos_config.h`:
```c
#elif defined(EOS_PRODUCT_MY_DEVICE)
#   include "products/my_device.h"
```

---

## 🔧 HAL Peripherals (33 Interfaces)

Every peripheral is conditionally compiled — only enabled peripherals are included in the binary.

| Category | Peripheral | API Prefix | Use Cases |
|---|---|---|---|
| **Core Bus** | GPIO | `eos_gpio_*` | All products |
| | UART | `eos_uart_*` | Debug, serial comms |
| | SPI | `eos_spi_*` | Flash, sensors, displays |
| | I2C | `eos_i2c_*` | Sensors, EEPROMs |
| | Timer | `eos_timer_*` | Scheduling, PWM base |
| **Analog** | ADC | `eos_adc_*` | Sensors, battery monitoring |
| | DAC | `eos_dac_*` | Audio output, motor control |
| | PWM | `eos_pwm_*` | Motors, LEDs, servos |
| **Buses** | CAN | `eos_can_*` | Automotive, industrial |
| | USB | `eos_usb_*` | Mobile, adapters |
| | PCIe | `eos_pcie_*` | Servers, GPUs, NICs |
| **Networking** | Ethernet | `eos_eth_*` | Gateways, industrial |
| | WiFi | `eos_wifi_*` | IoT, consumer |
| | BLE | `eos_ble_*` | Wearables, beacons |
| | Cellular | `eos_cellular_*` | Mobile, IoT, 5G |
| | NFC | `eos_nfc_*` | Payments, access |
| | IR | `eos_ir_*` | Remote controls, sensors |
| **Media** | Camera | `eos_camera_*` | Robots, phones, security |
| | Audio | `eos_audio_*` | Voice, music, calls |
| | Display | `eos_display_*` | Watches, HMI, phones |
| | HDMI | `eos_hdmi_*` | TVs, gaming, infotainment |
| | GPU | `eos_gpu_*` | AI, gaming, AR/VR |
| **Sensors** | GPS/GNSS | `eos_gnss_*` | Navigation, tracking |
| | IMU | `eos_imu_*` | Motion, orientation |
| | Radar/Lidar | `eos_radar_*` | Distance, ADAS |
| **Motion** | Motor | `eos_motor_*` | Robots, EVs, drones |
| | Haptics | `eos_haptic_*` | Phones, controllers |
| **Storage** | Flash/EEPROM | `eos_flash_*` | Firmware, configs |
| | SDIO | `eos_sdio_*` | SD cards, eMMC |
| **System** | RTC | `eos_rtc_*` | Timekeeping, alarms |
| | DMA | `eos_dma_*` | High-speed transfers |
| | Watchdog | `eos_wdt_*` | Safety-critical |
| | Touch | `eos_touch_*` | Touchscreens |

---

## 🧠 Multicore / Multiprocessor Support

EoS provides full multicore support for SMP and AMP architectures:

```c
#include <eos/multicore.h>

void secondary_core_main(void *arg) {
    while (1) { /* process on core 1 */ }
}

int main(void) {
    eos_multicore_init(EOS_MP_SMP);

    /* Start secondary core */
    eos_core_start(1, secondary_core_main, NULL);

    /* Pin task to specific core */
    eos_task_set_affinity(task_id, EOS_CORE_MASK(0));

    /* Cross-core synchronization */
    eos_spinlock_t lock = EOS_SPINLOCK_INIT;
    eos_spin_lock(&lock);
    /* ... critical section ... */
    eos_spin_unlock(&lock);
}
```

| Feature | API | Description |
|---|---|---|
| Core Management | `eos_core_start/stop/get_info` | Start/stop/query individual cores |
| Spinlocks | `eos_spin_lock/unlock/trylock` | Multi-core synchronization |
| IPI | `eos_ipi_send/broadcast` | Inter-Processor Interrupts |
| Shared Memory | `eos_shmem_create/open/flush` | Cross-core data sharing |
| Core Affinity | `eos_task_set_affinity/migrate` | Pin tasks to specific cores |
| Remote Processor | `eos_rproc_init/start/send` | Multi-controller communication |
| Atomics | `eos_atomic_add/cas/load/store` | Lock-free operations |
| Barriers | `eos_dmb/dsb/isb` | Memory ordering |

---

## 📦 Services & Frameworks

| Module | Header | Description |
|---|---|---|
| **Kernel** | `eos/kernel.h` | Tasks, mutexes, semaphores, message queues |
| **Multicore** | `eos/multicore.h` | SMP/AMP, spinlocks, IPI, shared memory |
| **Power** | `eos/power.h` | Sleep modes, battery, CPU scaling |
| **Networking** | `eos/net.h` | Sockets, HTTP, MQTT, mDNS |
| **Sensor** | `eos/sensor.h` | Unified sensor API, calibration, filtering |
| **Motor Control** | `eos/motor_ctrl.h` | PID loops, trajectory, encoder feedback |
| **Crypto** | `eos/crypto.h` | SHA-256, AES, CRC, RSA/ECC |
| **Security** | `eos/security.h` | Keystore, ACL, secure boot |
| **OTA** | `eos/ota.h` | Firmware update, A/B slots, rollback |
| **Filesystem** | `eos/filesystem.h` | File I/O over Flash, SD, LittleFS |
| **OS Services** | `eos/os_services.h` | Watchdog, audit, secure storage |

---

## 📂 Repository Structure

```
eos/
├── hal/                    # Hardware Abstraction Layer (33 peripherals)
│   ├── include/eos/        #   hal.h, hal_extended.h
│   └── src/                #   hal_common.c, hal_extended_stubs.c, hal_linux.c, hal_rtos.c
├── kernel/                 # RTOS Kernel + Multicore
│   ├── include/eos/        #   kernel.h, multicore.h
│   └── src/                #   task.c, sync.c, ipc.c, multicore.c
├── drivers/                # Driver framework
├── core/                   # OS core — config, logging, layers
├── include/eos/            # Global headers
│   └── eos_config.h        #   Product configuration (39 EOS_ENABLE_* flags)
├── products/               # 41 product profile headers
├── power/                  # Power management (sleep, battery, CPU scaling)
├── net/                    # Networking (sockets, HTTP, MQTT, mDNS)
├── services/               # Runtime services
│   ├── crypto/             #   SHA-256, AES, CRC, RSA/ECC
│   ├── security/           #   Keystore, ACL, attestation
│   ├── os/                 #   Watchdog, audit, secure storage
│   ├── sensor/             #   Unified sensor framework
│   ├── motor/              #   PID motor control
│   ├── ota/                #   Over-the-air firmware update
│   ├── filesystem/         #   File system abstraction
│   ├── linux/              #   Linux-specific services
│   └── rtos/               #   RTOS-specific services
├── systems/                # System image assembly
├── boards/                 # Board definitions
├── toolchains/             # Cross-compilation configs (ARM, RISC-V, x86)
├── examples/               # Example projects
├── docs/                   # Documentation
└── tests/                  # Unit tests (8 test suites)
```

---

## 🏗 Architecture

```
            ┌─────────────────────┐
            │    Application      │
            └──────────┬──────────┘
                       │
            ┌──────────▼──────────┐
            │    EoS Services     │
            │  crypto · security  │
            │  sensor · motor     │
            │  ota · filesystem   │
            └──────────┬──────────┘
                       │
         ┌─────────────┼─────────────┐
         │             │             │
    ┌────▼────┐  ┌─────▼────┐  ┌────▼────┐
    │ Kernel  │  │  Power   │  │   Net   │
    │ task    │  │  sleep   │  │ socket  │
    │ sync    │  │  battery │  │ HTTP    │
    │ IPC     │  │  CPU     │  │ MQTT    │
    │multicore│  │  scaling │  │ mDNS    │
    └────┬────┘  └─────┬────┘  └────┬────┘
         │             │            │
         └─────────────┼────────────┘
                       │
              ┌────────▼────────┐
              │      HAL        │
              │  33 peripherals │
              │  GPIO·UART·SPI  │
              │  I2C·CAN·USB    │
              │  WiFi·BLE·ETH   │
              │  Camera·Audio   │
              │  GPU·HDMI·PCIe  │
              │  Radar·GNSS·IMU │
              └────────┬────────┘
                       │
            ┌──────────┼──────────┐
            │                     │
       ┌────▼─────┐        ┌─────▼────┐
       │  Linux   │        │   RTOS   │
       │ Backend  │        │ Backend  │
       │  sysfs   │        │  regs    │
       └──────────┘        └──────────┘
```

---

## 🧪 Example Applications

EoS ships with **7 complete example applications** with full source code, build configs, and documentation:

| Example | Description | Key Modules |
|---------|------------|-------------|
| [blink-gpio](examples/blink-gpio/) | Toggle LED — simplest app | HAL (GPIO) |
| [uart-echo](examples/uart-echo/) | Serial read/echo | HAL (UART) |
| [ble-sensor](examples/ble-sensor/) | BLE + I2C temperature sensor | HAL, Kernel, Crypto |
| [multitask-rtos](examples/multitask-rtos/) | 3 tasks, mutex, message queue | Kernel |
| [secure-ota](examples/secure-ota/) | OTA firmware update + verify | OTA, Crypto |
| [posix-app](examples/posix-app/) | POSIX threads + semaphores | POSIX compat layer |
| [multicore-amp](examples/multicore-amp/) | Dual-core shared memory IPC | Multicore |

Each example has a `main.c` you can compile and run immediately.

### Code Snippets

### GPS Tracking
```c
#include <eos/hal_extended.h>

eos_gnss_config_t gps = { .port = 1, .baudrate = 9600 };
eos_gnss_init(&gps);

eos_gnss_position_t pos;
if (eos_gnss_get_position(&pos) == 0 && pos.fix_valid) {
    printf("Lat: %f, Lon: %f, Sats: %d\n",
           pos.latitude, pos.longitude, pos.satellites);
}
```

### Sensor Framework
```c
#include <eos/sensor.h>

int temp_read(uint8_t id, float *val) { *val = 25.5f; return 0; }

eos_sensor_init();
eos_sensor_config_t cfg = {
    .id = 0, .name = "temp", .type = EOS_SENSOR_TEMPERATURE,
    .filter = EOS_FILTER_AVERAGE, .filter_window = 8,
    .read_fn = temp_read
};
eos_sensor_register(&cfg);

eos_sensor_reading_t reading;
eos_sensor_read_filtered(0, &reading);
```

### OTA Firmware Update
```c
#include <eos/ota.h>

eos_ota_init();
eos_ota_source_t src = { .url = "https://fw.example.com/v2.bin", .version = "2.0.0" };
eos_ota_begin(&src);
/* ... write chunks ... */
eos_ota_finish();
eos_ota_verify();
eos_ota_apply();
```

---

## 💡 Design Principles

- ✔ **Universal** — One OS framework for 41+ product categories
- ✔ **Portable** — Same API across Linux and RTOS targets
- ✔ **Minimal** — Compile-time selection: only enabled modules are built
- ✔ **Multicore** — SMP/AMP with spinlocks, IPI, shared memory, affinity
- ✔ **Secure** — Crypto, secure boot, audit logging, OTA with rollback
- ✔ **Connected** — WiFi, BLE, Cellular, Ethernet, CAN, NFC
- ✔ **Real-time** — RTOS kernel with task scheduling and synchronization
- ✔ **Extensible** — Add new peripherals and products with a single `.h` file

---

## 🎯 Use Cases

### Who uses eos and why

| Customer | Use Case | Product Profile | Key Modules |
|---|---|---|---|
| **Automotive OEM** | Vehicle ECU with sensor fusion + CAN bus | `automotive` | HAL (CAN, ADC, PWM), kernel, motor control, watchdog |
| **Medical device maker** | Patient monitor with BLE + encrypted storage | `medical` | HAL (BLE, ADC), crypto (AES), secure storage, audit |
| **Drone company** | Flight controller with GPS + IMU + motor | `drone` | HAL (PWM, UART, SPI), motor control, sensor, GPS |
| **IoT startup** | WiFi sensor node reporting to cloud | `iot` | HAL (WiFi, ADC), OTA update, power management |
| **Smart home vendor** | Hub controlling lights, locks, cameras | `smart_home` | HAL (WiFi, BLE, IR), networking (MQTT), OTA |
| **Robotics company** | Industrial robot arm with 6-axis control | `robot` | Motor control, IMU, camera, kernel (real-time) |
| **Wearable maker** | Fitness band with heart rate + display | `wearable` | HAL (BLE, ADC, Display), power (low-power) |
| **Aerospace firm** | Satellite attitude controller | `satellite` | HAL (IMU, GNSS), crypto, watchdog, redundancy |
| **EV manufacturer** | Battery management + motor control | `ev` | HAL (CAN, ADC), motor, power, multicore |
| **Telecom vendor** | 5G base station module | `telecom` | HAL (Ethernet, PCIe, Cellular), multicore, crypto |
| **Gaming console** | Handheld with GPU + haptics | `gaming` | HAL (GPU, HDMI, Haptics), multicore, audio |
| **Factory automation** | PLC controlling motors + sensors | `plc` | HAL (CAN, ADC, DAC), motor, watchdog |

### eos standalone (without eboot or ebuild)

```bash
# Customer uses their own Makefile — no eboot, no ebuild needed
gcc -c eos/hal/src/hal_rtos.c -I eos/hal/include -DEOS_ENABLE_UART -DEOS_ENABLE_SPI
gcc -c eos/kernel/src/task.c -I eos/kernel/include
gcc -c eos/services/crypto/src/sha256.c -I eos/services/crypto/include
ar rcs libeos.a *.o
```

### eos with any build system

| Build System | Integration |
|---|---|
| **CMake** | `add_subdirectory(eos)` or `find_package(eos)` |
| **Make / Makefile** | Compile `*.c` with `-I` include paths |
| **Yocto** | `.bb` recipe, `inherit cmake`, `EXTRA_OECMAKE` |
| **Buildroot** | `eos.mk` with `$(eval $(cmake-package))` |
| **Meson** | `subproject('eos')` |
| **Zephyr west** | External module in `west.yml` |
| **PlatformIO** | Library in `platformio.ini` |
| **Vendor SDK** | Drop source files into project |
| **ebuild** | `ebuild build` (auto-detect CMake) |
| **Bazel** | `cc_library` rule |

---

## Related Projects

| Project | Description | Link |
|---|---|---|
| **eboot** | Bootloader — staged boot, secure boot, A/B slots, recovery | [embeddedos-org/eboot](https://github.com/embeddedos-org/eboot) |
| **ebuild** | Build system — YAML config, Ninja backend, package management | [embeddedos-org/ebuild](https://github.com/embeddedos-org/ebuild) |
| **eipc** | Inter-process communication — shared memory, message passing, RPC | [embeddedos-org/eipc](https://github.com/embeddedos-org/eipc) |
| **eai** | AI/ML framework — on-device inference, model management, edge AI | [embeddedos-org/eai](https://github.com/embeddedos-org/eai) |
| **eni** | Network infrastructure — protocol stacks, mesh networking, discovery | [embeddedos-org/eni](https://github.com/embeddedos-org/eni) |
| **eos-sdk** | SDK and tooling — developer tools, templates, CLI, documentation | [embeddedos-org/eos-sdk](https://github.com/embeddedos-org/eos-sdk) |

---

## 🔐 Hardware Schemas (for ebuild EoS AI)

The `schemas/` directory provides machine-readable hardware vocabulary that ebuild's EoS AI analyzer uses to generate configs:

| Schema | Coverage |
|---|---|
| `board.schema.yaml` | 6 architectures, 19 core types, 32 peripheral types, 10 vendors |
| `hal_map.yaml` | 22 HAL interfaces → API prefix, enable flags, pin types |
| `peripherals.yaml` | 20 peripheral types with use cases, common chips, 10 product mappings |

```bash
# ebuild reads eos schemas for hardware analysis
ebuild analyze --file board.kicad_sch --eos-schemas ../eos/schemas/
```

---

## 🛡 Linux Security Services

| Service | Header | Description |
|---|---|---|
| SELinux | `linux_security.h` | Policy management, rootfs labeling, enforcing/permissive modes |
| IMA | `linux_security.h` | Integrity Measurement Architecture — measure, appraise, enforce |
| dm-verity | `linux_security.h` | Block-level integrity verification with root hash |
| Kernel Audit | `linux_security.h` | Exec/net/file/mount logging, audit rules generation |
| BusyBox | `linux_security.h` | Applet management, cross-compile, rootfs install |

## 🔧 RTOS Security Services

| Service | Header | Description |
|---|---|---|
| MPU Isolation | `rtos_security.h` | Per-task memory regions, permissions, ARM MPU programming |
| RTOS ACL | `rtos_security.h` | Task→peripheral/IRQ/DMA access control policy |
| RTOS Audit | `rtos_security.h` | Lightweight tick-based ring buffer logging, fault recording |

---

## 🚀 CI/CD

GitHub Actions runs on every push/PR to `master`:

- **Build matrix**: Linux × Windows × macOS
- **Product profiles**: 8 products built in parallel (robot, gateway, medical, automotive, iot, aerospace, wearable, industrial)
- **Tests**: `ctest` runs all test suites
- **Releases**: Tag `v*.*.*` → auto-generate changelog → GitHub Release with artifacts

```bash
# Create a release
git tag v1.0.0
git push origin v1.0.0
```

---

## 📚 Documentation

| Document | Description |
|----------|------------|
| [Getting Started](GETTING_STARTED.md) | Setup guide with 3 hardware paths |
| [API Reference](docs/api-reference.md) | All modules, types, function signatures |
| [Choosing a Product Profile](docs/choosing-a-product-profile.md) | Decision tree for 41 profiles |
| [Integration Guide](docs/integration-guide.md) | How eos + eboot + ebuild work together |
| [Troubleshooting](docs/troubleshooting.md) | Top 15 FAQs and common pitfalls |
| Board Quickstarts | [nRF52](docs/quickstart-nrf52.md) · [STM32](docs/quickstart-stm32.md) · [RPi4](docs/quickstart-rpi4.md) · [Host](docs/quickstart-host.md) |

---

## 📜 License

MIT License
