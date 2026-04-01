# Troubleshooting

Common issues and solutions when working with EoS, eboot, and ebuild.

---

## Build Issues

### 1. CMake: "EOS_PRODUCT not set"

```
CMake Error: EOS_PRODUCT is not defined
```

**Solution:** Specify the product profile:
```bash
cmake -B build -DEOS_PRODUCT=iot
```

See [Choosing a Product Profile](choosing-a-product-profile.md) for all 41 options.

---

### 2. "arm-none-eabi-gcc: command not found"

**Solution:** Install the ARM toolchain:
```bash
# Ubuntu/Debian
sudo apt install gcc-arm-none-eabi

# macOS
brew install --cask gcc-arm-embedded

# Windows — download from https://developer.arm.com/downloads/-/gnu-rm
```

---

### 3. Linker error: "undefined reference to eos_hal_init"

```
undefined reference to `eos_hal_init'
```

**Cause:** HAL source files not included in the build.

**Solution:** Ensure you're compiling the HAL sources:
```bash
# If using CMake, add_subdirectory(eos) handles this automatically.
# If using manual compilation:
gcc -c eos/hal/src/hal_common.c -I eos/hal/include -I eos/include
```

---

### 4. Linker error: "multiple definition of..."

**Cause:** Both Linux and RTOS backends compiled together.

**Solution:** Only compile ONE backend:
```bash
# For host/Linux builds:
gcc -c eos/hal/src/hal_linux.c ...

# For RTOS/bare-metal builds:
gcc -c eos/hal/src/hal_rtos.c ...
```

---

### 5. "EOS_ENABLE_BLE undeclared"

**Cause:** The product profile doesn't enable BLE, or `eos_config.h` is not included.

**Solution:**
```c
#include <eos/eos_config.h>  /* Must be included before hal_extended.h */
#include <eos/hal_extended.h>

/* Guard your BLE code: */
#if EOS_ENABLE_BLE
    eos_ble_init(&ble_cfg);
#endif
```

Or switch to a profile that enables BLE (e.g., `iot`, `watch`, `wearable`).

---

### 6. Build succeeds but binary is too large

**Cause:** Too many peripherals enabled, or debug symbols included.

**Solution:**
- Use a targeted product profile instead of building everything
- Disable unused peripherals in your profile header
- For release builds: `cmake -B build -DCMAKE_BUILD_TYPE=Release`
- Strip debug symbols: `arm-none-eabi-strip firmware.elf`

---

## Flash Issues

### 7. nrfjprog: "No debuggers found"

**Solution:**
- Check USB cable is connected (use the debug USB port, not the kit USB)
- Install J-Link drivers from SEGGER
- On Linux, add udev rules: `sudo cp 99-jlink.rules /etc/udev/rules.d/`
- Try `nrfjprog --recover` to unlock a locked device

---

### 8. OpenOCD: "Error connecting to ST-Link"

**Solution:**
- Install ST-Link drivers (Windows) or `libusb` (Linux)
- Check USB cable
- Try: `openocd -f interface/stlink.cfg -f target/stm32h7x.cfg`
- On Linux: `sudo openocd ...` or add udev rules

---

### 9. Flash verification fails

**Cause:** Flash might be write-protected or corrupted.

**Solution:**
```bash
# Mass erase first
nrfjprog --eraseall
# Then program
nrfjprog --program build/firmware.hex --verify

# For STM32:
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "init; reset halt; flash erase_address 0x08000000 0x200000; exit"
```

---

## Runtime Issues

### 10. No serial output (UART)

**Checklist:**
1. Correct baud rate? Default is **115200**
2. Correct COM port / device? (`/dev/ttyACM0`, `/dev/cu.usbmodem*`)
3. TX/RX pins not swapped?
4. USB cable supports data (not charge-only)?
5. UART initialized in code? Check `eos_uart_init()` is called before `eos_uart_write()`

---

### 11. LED doesn't blink

**Checklist:**
1. Correct pin number? Check your board's schematic:
   - nRF52840-DK: pin 13 (LED1)
   - STM32 Nucleo: pin 5 (PA5 = LD2)
   - Custom board: check your schematic
2. GPIO initialized as output? Check `eos_gpio_init()` config
3. For host builds: GPIO operations print to stdout (no physical LED)

---

### 12. Kernel tasks don't run

**Checklist:**
1. Called `eos_kernel_init()` before creating tasks?
2. Called `eos_kernel_start()` after creating tasks? (This function doesn't return)
3. Task stack size large enough? Try 1024 or 2048 bytes
4. Task priority valid? Higher number = higher priority
5. No infinite loop before `eos_kernel_start()`?

---

### 13. Multicore: secondary core doesn't start

**Checklist:**
1. `EOS_ENABLE_MULTICORE` set to 1 in your product profile?
2. Called `eos_multicore_init()` before `eos_core_start()`?
3. Target board actually has multiple cores?
4. Secondary core entry function has correct signature: `void fn(void *arg)`

---

### 14. OTA update fails verification

**Cause:** Firmware signature doesn't match, or firmware is corrupted.

**Solution:**
- Verify you're using the correct signing key
- Check that the firmware binary isn't truncated
- Ensure `eos_ota_finish()` is called before `eos_ota_verify()`
- Check `eos_ota_get_status()` for detailed error state

---

## ebuild Issues

### 15. "ebuild: command not found"

**Solution:**
```bash
cd EoS/ebuild
pip install -e .
# Or run directly:
python -m ebuild --help
```

---

## My MCU Isn't in the Database

If `ebuild analyze` doesn't recognize your MCU:

### Option 1: Add it to the hardware database

Edit `ebuild/hardware/soc/` and add a YAML file:
```yaml
# ebuild/hardware/soc/my_mcu.yaml
name: MY_MCU_FAMILY
vendor: my_vendor
architecture: arm
core: cortex-m4f
flash_kb: 512
ram_kb: 128
peripherals:
  - UART
  - SPI
  - I2C
  - GPIO
```

### Option 2: Use text prompt with manual specs

```bash
ebuild analyze "Custom ARM Cortex-M4F MCU, 512KB flash, 128KB RAM, UART SPI I2C GPIO"
```

### Option 3: Create a custom product profile manually

See [Choosing a Product Profile](choosing-a-product-profile.md#creating-a-custom-profile).

---

## Getting Help

- Check the [API Reference](api-reference.md) for correct function signatures
- Read the [example applications](../../examples/) for working code patterns
- Review the [Integration Guide](../../docs/integration-guide.md) for eos + eboot workflow
