# Adding Hardware to EoS and eboot

This guide covers every way a hardware vendor or contributor can extend EoS with their own hardware. There are **4 extension paths**, from simplest to most involved.

---

## Overview: 4 Extension Paths

| What you want to do | Where | Difficulty | Files to create/modify |
|---------------------|-------|-----------|----------------------|
| **1. Add a new board to eboot** | eboot | Easy | 2 new files |
| **2. Add a new product profile to eos** | eos | Easy | 1 new file + 1 line in eos_config.h |
| **3. Add a hardware driver for an existing peripheral** | eos | Medium | 1 new file (backend registration) |
| **4. Add an entirely new peripheral type** | eos | Advanced | 3-4 files across hal, config, products |

---

## Path 1: Add a New Board to eboot

**Use case:** You have a new MCU/SoC board and want eboot to support it.

### Step 1: Create the board header

```c
/* eboot/boards/my_board/board_my_board.h */
#ifndef BOARD_MY_BOARD_H
#define BOARD_MY_BOARD_H

#include "eos_hal.h"

/* Memory map */
#define MY_BOARD_FLASH_BASE   0x08000000
#define MY_BOARD_FLASH_SIZE   (1024 * 1024)  /* 1 MB */
#define MY_BOARD_RAM_BASE     0x20000000
#define MY_BOARD_RAM_SIZE     (256 * 1024)   /* 256 KB */

/* Flash layout */
#define MY_BOARD_STAGE0_ADDR  0x08000000
#define MY_BOARD_STAGE0_SIZE  (16 * 1024)
#define MY_BOARD_STAGE1_ADDR  0x08004000
#define MY_BOARD_STAGE1_SIZE  (64 * 1024)
#define MY_BOARD_BOOTCTL_ADDR 0x08014000
#define MY_BOARD_SLOT_A_ADDR  0x08016000
#define MY_BOARD_SLOT_A_SIZE  (468 * 1024)
#define MY_BOARD_SLOT_B_ADDR  0x0808A000
#define MY_BOARD_SLOT_B_SIZE  (468 * 1024)

/* Public API */
void board_my_board_early_init(void);
const eos_board_ops_t *board_my_board_get_ops(void);

#endif /* BOARD_MY_BOARD_H */
```

### Step 2: Implement the board operations

```c
/* eboot/boards/my_board/board_my_board.c */
#include "board_my_board.h"
#include "eos_board_registry.h"

/* --- Flash driver --- */
static int my_flash_read(uint32_t addr, void *buf, size_t len) {
    /* Read len bytes from flash addr into buf */
    memcpy(buf, (const void *)addr, len);
    return 0;
}

static int my_flash_write(uint32_t addr, const void *buf, size_t len) {
    /* Implement flash write for your MCU */
    /* ... MCU-specific register writes ... */
    return 0;
}

static int my_flash_erase(uint32_t addr, size_t len) {
    /* Implement sector erase for your MCU */
    return 0;
}

/* --- Other operations --- */
static void my_watchdog_init(uint32_t timeout_ms) { /* ... */ }
static void my_watchdog_feed(void) { /* ... */ }
static void my_system_reset(void) { /* ... */ }
static void my_jump(uint32_t vector_addr) {
    void (*app_entry)(void) = (void (*)(void))(*(uint32_t *)(vector_addr + 4));
    __set_MSP(*(uint32_t *)vector_addr);
    app_entry();
}

/* --- Board ops vtable --- */
static const eos_board_ops_t my_board_ops = {
    .flash_base          = MY_BOARD_FLASH_BASE,
    .flash_size          = MY_BOARD_FLASH_SIZE,
    .slot_a_addr         = MY_BOARD_SLOT_A_ADDR,
    .slot_a_size         = MY_BOARD_SLOT_A_SIZE,
    .slot_b_addr         = MY_BOARD_SLOT_B_ADDR,
    .slot_b_size         = MY_BOARD_SLOT_B_SIZE,
    .bootctl_addr        = MY_BOARD_BOOTCTL_ADDR,

    .flash_read          = my_flash_read,
    .flash_write         = my_flash_write,
    .flash_erase         = my_flash_erase,
    .watchdog_init       = my_watchdog_init,
    .watchdog_feed       = my_watchdog_feed,
    .system_reset        = my_system_reset,
    .jump                = my_jump,
};

const eos_board_ops_t *board_my_board_get_ops(void) {
    return &my_board_ops;
}

/* --- Auto-registration (GCC/Clang) --- */
static bool my_board_detect(void) {
    /* Return true if this board is the active hardware.
     * Read a board ID register, GPIO strapping pins, etc. */
    return true;
}

EBOOT_REGISTER_BOARD("my_board", EOS_PLATFORM_ARM_CM4, 0x1234,
                      board_my_board_get_ops, my_board_detect);
```

### Step 3: Add to CMakeLists.txt

```cmake
# eboot/CMakeLists.txt — add one line:
eboot_add_board(my_board)
```

### Step 4: Build and test

```bash
cmake -B build -DEBLDR_BOARD=my_board
cmake --build build
```

**That's it.** Two files and one CMake line.

---

## Path 2: Add a New Product Profile to eos

**Use case:** You have a specific device with a unique combination of peripherals.

### Step 1: Create the profile header

```c
/* eos/products/my_device.h */
#ifndef EOS_PRODUCT_MY_DEVICE_H
#define EOS_PRODUCT_MY_DEVICE_H

#define EOS_PRODUCT_NAME "my_device"

/* Enable the peripherals your device has */
#define EOS_ENABLE_GPIO        1
#define EOS_ENABLE_UART        1
#define EOS_ENABLE_SPI         1
#define EOS_ENABLE_I2C         1
#define EOS_ENABLE_BLE         1
#define EOS_ENABLE_DISPLAY     1
#define EOS_ENABLE_ADC         1
#define EOS_ENABLE_IMU         1

/* Services */
#define EOS_ENABLE_CRYPTO      1
#define EOS_ENABLE_OTA         1

/* Everything else defaults to 0 via eos_config.h */

#endif
```

### Step 2: Register in eos_config.h

Add one `#elif` line to the product chain:

```c
/* eos/include/eos/eos_config.h — add before the #else block: */
#elif defined(EOS_PRODUCT_MY_DEVICE)
#   include "products/my_device.h"
```

### Step 3: Build

```bash
cmake -B build -DEOS_PRODUCT=my_device
cmake --build build
```

**Note:** Any `EOS_ENABLE_*` flag you don't define will default to `0` (disabled) thanks to the `#ifndef` guards at the bottom of `eos_config.h`.

---

## Path 3: Add a Hardware Driver (Backend Registration)

**Use case:** You have a vendor-specific BLE stack, WiFi driver, or display controller and want to plug it into the EoS HAL without modifying any EoS source files.

### Core HAL (GPIO, UART, SPI, I2C, Timer)

Register via `eos_hal_register_backend()`:

```c
/* my_hal_backend.c */
#include <eos/hal.h>

static int my_gpio_init(const eos_gpio_config_t *cfg) {
    /* Implement GPIO init for your MCU */
    return 0;
}

static void my_gpio_write(uint16_t pin, bool value) {
    /* Write to your MCU's GPIO register */
}

/* ... implement all functions you need ... */

static const eos_hal_backend_t my_backend = {
    .name        = "my_mcu_hal",
    .init        = my_hal_init,
    .deinit      = my_hal_deinit,
    .delay_ms    = my_delay_ms,
    .get_tick_ms = my_get_tick_ms,
    .gpio_init   = my_gpio_init,
    .gpio_write  = my_gpio_write,
    .gpio_read   = my_gpio_read,
    .gpio_toggle = my_gpio_toggle,
    .uart_init   = my_uart_init,
    .uart_write  = my_uart_write,
    .uart_read   = my_uart_read,
    .spi_init    = my_spi_init,
    .spi_transfer = my_spi_transfer,
    .i2c_init    = my_i2c_init,
    .i2c_write   = my_i2c_write,
    .i2c_read    = my_i2c_read,
};

/* Call at startup, before eos_hal_init() */
void my_platform_init(void) {
    eos_hal_register_backend(&my_backend);
}
```

### Extended HAL (BLE, WiFi, Camera, Display, Motor, GNSS, etc.)

Register via `eos_hal_register_ext_backend()`:

```c
/* my_ble_driver.c */
#include <eos/hal_extended.h>

static int nrf_ble_init(const eos_ble_config_t *cfg) {
    /* Nordic softdevice BLE init */
    return 0;
}

static int nrf_ble_send(const uint8_t *data, size_t len) {
    /* Send via Nordic BLE stack */
    return 0;
}

/* ... implement all BLE functions ... */

static const eos_hal_ext_backend_t my_ext_backend = {
    .name = "nrf52_ble_wifi",

    /* Only fill in the peripherals you're implementing */
    .ble_init            = nrf_ble_init,
    .ble_deinit          = nrf_ble_deinit,
    .ble_advertise_start = nrf_ble_adv_start,
    .ble_advertise_stop  = nrf_ble_adv_stop,
    .ble_send            = nrf_ble_send,
    .ble_set_rx_callback = nrf_ble_set_rx_cb,

    /* Leave others as NULL — stubs will be used */
};

void my_ble_driver_init(void) {
    eos_hal_register_ext_backend(&my_ext_backend);
}
```

**Key point:** You don't touch any EoS source files. Just compile your driver alongside EoS and call the registration function at startup.

---

## Path 4: Add an Entirely New Peripheral Type

**Use case:** EoS doesn't have an API for your hardware (e.g., LoRa radio, LiDAR scanner, e-ink display).

### Step 1: Add the enable flag

```c
/* eos/include/eos/eos_config.h — add to the "default disabled" section: */
#ifndef EOS_ENABLE_LORA
#   define EOS_ENABLE_LORA 0
#endif
```

### Step 2: Define the API in hal_extended.h

```c
/* eos/hal/include/eos/hal_extended.h — add a new section: */

/* ============================================================
 * LoRa — Long Range Radio
 * ============================================================ */
#if EOS_ENABLE_LORA

typedef struct {
    uint8_t  port;         /* SPI port to LoRa module */
    uint32_t frequency_hz; /* 868 MHz or 915 MHz */
    uint8_t  spreading_factor; /* 7–12 */
    uint8_t  bandwidth;    /* 0=125kHz, 1=250kHz, 2=500kHz */
    int8_t   tx_power_dbm; /* -4 to +20 */
} eos_lora_config_t;

int  eos_lora_init(const eos_lora_config_t *cfg);
void eos_lora_deinit(void);
int  eos_lora_send(const uint8_t *data, size_t len);
int  eos_lora_receive(uint8_t *data, size_t max_len, uint32_t timeout_ms);
int  eos_lora_set_channel(uint32_t frequency_hz);
int8_t eos_lora_get_rssi(void);

#endif /* EOS_ENABLE_LORA */
```

### Step 3: Add stub implementation

```c
/* eos/hal/src/hal_extended_stubs.c — add weak stubs: */

#if EOS_ENABLE_LORA
__attribute__((weak)) int  eos_lora_init(const eos_lora_config_t *cfg) { return -1; }
__attribute__((weak)) void eos_lora_deinit(void) {}
__attribute__((weak)) int  eos_lora_send(const uint8_t *data, size_t len) { return -1; }
__attribute__((weak)) int  eos_lora_receive(uint8_t *data, size_t max_len, uint32_t t) { return -1; }
__attribute__((weak)) int  eos_lora_set_channel(uint32_t freq) { return -1; }
__attribute__((weak)) int8_t eos_lora_get_rssi(void) { return -128; }
#endif
```

### Step 4: Add to the extended backend vtable

```c
/* eos/hal/include/eos/hal_extended.h — add to eos_hal_ext_backend_t: */

#if EOS_ENABLE_LORA
    int  (*lora_init)(const eos_lora_config_t *cfg);
    void (*lora_deinit)(void);
    int  (*lora_send)(const uint8_t *data, size_t len);
    int  (*lora_receive)(uint8_t *data, size_t max_len, uint32_t timeout_ms);
#endif
```

### Step 5: Enable in product profiles that need it

```c
/* eos/products/iot.h — add: */
#define EOS_ENABLE_LORA 1
```

### Step 6: Implement the real driver

```c
/* my_lora_driver.c — real SX1276 implementation */
#include <eos/hal_extended.h>

int eos_lora_init(const eos_lora_config_t *cfg) {
    /* SX1276 SPI init, configure registers */
    return 0;
}

/* ... implement all functions ... */
/* Because stubs are __attribute__((weak)), your real implementation wins at link time */
```

---

## Registration API Summary

| Layer | Registration Function | Vtable Type | What it covers |
|-------|---------------------|-------------|---------------|
| **eos core HAL** | `eos_hal_register_backend()` | `eos_hal_backend_t` | GPIO, UART, SPI, I2C, Timer |
| **eos extended HAL** | `eos_hal_register_ext_backend()` | `eos_hal_ext_backend_t` | BLE, WiFi, Camera, Display, Motor, GNSS, IMU, Audio |
| **eboot board** | `EBOOT_REGISTER_BOARD()` | `eos_board_ops_t` | Flash, watchdog, reset, jump, UART, timing |
| **eboot board (manual)** | `eos_board_register()` | `eos_board_info_t` | Same, without GCC constructor |

## Platform Identifiers (eboot)

When registering a board, use the correct platform enum:

| ID | Enum | Architecture |
|----|------|-------------|
| 0 | `EOS_PLATFORM_ARM_CM0` | ARM Cortex-M0/M0+ |
| 2 | `EOS_PLATFORM_ARM_CM4` | ARM Cortex-M4/M4F |
| 3 | `EOS_PLATFORM_ARM_CM7` | ARM Cortex-M7 |
| 5 | `EOS_PLATFORM_ARM_CA53` | ARM Cortex-A53 |
| 6 | `EOS_PLATFORM_ARM_CA72` | ARM Cortex-A72 |
| 8 | `EOS_PLATFORM_RISCV32` | RISC-V 32-bit |
| 9 | `EOS_PLATFORM_RISCV64` | RISC-V 64-bit |
| 11 | `EOS_PLATFORM_X86_64` | x86_64 / UEFI |
| 12 | `EOS_PLATFORM_XTENSA` | Xtensa LX6/LX7 |
| 13 | `EOS_PLATFORM_POWERPC` | PowerPC |
| 14 | `EOS_PLATFORM_SPARC` | SPARC LEON |
| 15 | `EOS_PLATFORM_M68K` | Motorola 68000 |

See `eboot/include/eos_hal.h` for the complete list (23 platforms).
