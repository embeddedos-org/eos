# Integration Guide: eos + eboot + ebuild

This document explains how the three EoS components work together, and how to use them independently or in combination.

---

## How They Connect

```
┌─────────────────────────────────────────────────────────┐
│                      ebuild                             │
│                                                         │
│  Hardware Description ──► EoS AI Analyzer               │
│         │                      │                        │
│         ▼                      ▼                        │
│    board.yaml            eos_product_config.h           │
│    boot.yaml             eboot_flash_layout.h           │
│         │                      │                        │
└─────────┼──────────────────────┼────────────────────────┘
          │                      │
     ┌────▼────┐            ┌────▼────┐
     │  eboot  │            │   eos   │
     │         │            │         │
     │ Stage-0 │            │   HAL   │
     │ Stage-1 │            │ Kernel  │
     │ Crypto  │            │Services │
     │ Flash   │            │  App    │
     └────┬────┘            └────┬────┘
          │                      │
          ▼                      ▼
     ┌────────────────────────────────┐
     │        Flash Image            │
     │  [Stage-0][Stage-1][App A][B] │
     └────────────────────────────────┘
```

**Data flow:** Hardware description → ebuild generates configs → eos compiles firmware → eboot provides boot + OTA → flash image deployed to hardware.

---

## Section 1: Build eos standalone (no eboot, no ebuild)

EoS works perfectly without eboot or ebuild. Use your own build system.

### With CMake

```bash
cd EoS/eos
cmake -B build -DEOS_PRODUCT=iot -DEOS_BUILD_TESTS=ON
cmake --build build
```

### With Make / manual compilation

```bash
# Compile only what you need
gcc -c eos/hal/src/hal_common.c -I eos/hal/include -I eos/include \
    -DEOS_ENABLE_GPIO -DEOS_ENABLE_UART -DEOS_ENABLE_SPI
gcc -c eos/kernel/src/task.c -I eos/kernel/include
gcc -c eos/services/crypto/src/sha256.c -I eos/services/crypto/include
ar rcs libeos.a *.o

# Link with your application
gcc -o my_app main.c -L. -leos -I eos/hal/include -I eos/kernel/include
```

### With Yocto

```bitbake
# eos.bb recipe
SUMMARY = "EoS Embedded Operating System"
SRC_URI = "git://github.com/anthropic/EoS.git;branch=main"
inherit cmake
EXTRA_OECMAKE = "-DEOS_PRODUCT=gateway"
```

---

## Section 2: Add eboot secure boot to your eos app

### Step 1: Build eboot for your board

```bash
cd EoS/eboot
cmake -B build -DEBLDR_BOARD=nrf52
cmake --build build
```

This produces `eboot.bin` — the bootloader binary that goes at the start of flash.

### Step 2: Understand the flash layout

eboot divides flash into regions:

```
┌──────────────┐ 0x00000
│   Stage-0    │ 16 KB — minimal init, jumps to Stage-1
├──────────────┤ 0x04000
│   Stage-1    │ 64 KB — full bootloader with crypto + flash mgmt
├──────────────┤ 0x14000
│   Boot Ctrl  │ 8 KB — boot control blocks (2 × 4 KB)
├──────────────┤ 0x16000
│   Slot A     │ ~212 KB — primary firmware slot
├──────────────┤ 0x4B000
│   Slot B     │ ~212 KB — secondary firmware slot (for OTA)
├──────────────┤ 0x80000
│   Storage    │ remaining — config, filesystem, logs
└──────────────┘
```

### Step 3: Link your eos app to the correct slot

Use the generated linker script to place your firmware at the Slot A address:

```bash
# Generate the flash layout header and linker script
cd EoS/ebuild
python -m ebuild generate-boot ../eboot/configs/nrf52_boot.yaml --output-dir _generated

# The outputs:
#   _generated/eboot_flash_layout.h  — flash addresses
#   _generated/eboot_linker.ld       — linker script
```

Build your eos app with the linker script:
```bash
cd EoS/eos
cmake -B build -DEOS_PRODUCT=iot \
  -DCMAKE_TOOLCHAIN_FILE=toolchains/arm-none-eabi.cmake \
  -DEOS_LINKER_SCRIPT=../ebuild/_generated/eboot_linker.ld
cmake --build build
```

### Step 4: Sign and flash

```bash
# Generate signing key (first time only)
cd EoS/eboot/tools
python3 sign_image.py --generate-key firmware-key.pem

# Sign the firmware
python3 sign_image.py \
  --key firmware-key.pem \
  --input ../../eos/build/my_app.bin \
  --output signed-firmware.bin

# Flash eboot + signed firmware
nrfjprog --program ../build/eboot.hex --chiperase
nrfjprog --program signed-firmware.hex --verify
nrfjprog --reset
```

---

## Section 3: Use ebuild to automate everything

ebuild wraps eos + eboot into a single workflow.

### From a YAML hardware description

```bash
cd EoS/ebuild
pip install -e .

# Analyze hardware and generate all configs
ebuild analyze hardware/board/sample_iot_gateway.yaml

# Generate configs
ebuild generate-project --file hardware/board/sample_iot_gateway.yaml --output-dir my_project
```

### From a text prompt

```bash
ebuild analyze "nRF52840 with BLE, I2C, SPI, 1MB flash, 256KB RAM"
```

This generates:
- `board.yaml` — board definition
- `boot.yaml` — flash layout + boot policy
- `eos_product_config.h` — `EOS_ENABLE_*` flags
- `eboot_flash_layout.h` — flash addresses

### Build everything

```bash
# Build eos
cd my_project
cmake -B build -DEOS_PRODUCT=iot
cmake --build build

# Build eboot
cd ../eboot
cmake -B build -DEBLDR_BOARD=nrf52
cmake --build build
```

---

## Section 4: Use ebuild generate-project for customer deliveries

For producing a complete project package for a customer:

```bash
ebuild generate-project \
  --text "STM32H7 automotive ECU with CAN, Ethernet, ADC, 2MB flash, 1MB RAM" \
  --output-dir customer_delivery

# The output directory contains:
# customer_delivery/
# ├── board.yaml
# ├── boot.yaml
# ├── eos_product_config.h
# ├── eboot_flash_layout.h
# ├── build_instructions.md
# └── llm_prompt.txt (optional: for deeper LLM analysis)
```

The customer can then:
1. Copy `eos_product_config.h` into their eos build
2. Copy `eboot_flash_layout.h` into their eboot build
3. Follow `build_instructions.md` to compile and flash

---

## Summary: Which components do you need?

| Use Case | eos | eboot | ebuild |
|----------|-----|-------|--------|
| Simple firmware (no OTA, no secure boot) | ✅ | ❌ | ❌ |
| Firmware with OTA updates | ✅ | ✅ | ❌ |
| Firmware with secure boot | ✅ | ✅ | ❌ |
| Automated project generation | ✅ | ❌ | ✅ |
| Full workflow (generate + build + secure boot + OTA) | ✅ | ✅ | ✅ |
| AI hardware analysis | ❌ | ❌ | ✅ |
