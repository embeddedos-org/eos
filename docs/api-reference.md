# API Reference

Complete reference for all EoS modules, organized by functional area.

---

## HAL — Hardware Abstraction Layer

### Core HAL (`eos/hal.h`)

#### System

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_hal_init` | `int eos_hal_init(void)` | Initialize HAL subsystem. Call first. |
| `eos_hal_deinit` | `void eos_hal_deinit(void)` | Release all HAL resources. |
| `eos_delay_ms` | `void eos_delay_ms(uint32_t ms)` | Blocking delay in milliseconds. |
| `eos_get_tick_ms` | `uint32_t eos_get_tick_ms(void)` | System uptime in milliseconds. |

#### GPIO

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_gpio_init` | `int eos_gpio_init(const eos_gpio_config_t *cfg)` | Configure a GPIO pin. |
| `eos_gpio_deinit` | `void eos_gpio_deinit(uint16_t pin)` | Release a GPIO pin. |
| `eos_gpio_write` | `void eos_gpio_write(uint16_t pin, bool value)` | Set pin high/low. |
| `eos_gpio_read` | `bool eos_gpio_read(uint16_t pin)` | Read pin state. |
| `eos_gpio_toggle` | `void eos_gpio_toggle(uint16_t pin)` | Toggle pin state. |
| `eos_gpio_set_irq` | `int eos_gpio_set_irq(uint16_t pin, eos_gpio_edge_t edge, eos_gpio_callback_t cb, void *ctx)` | Set interrupt on edge. |

**Types:** `eos_gpio_config_t` (pin, mode, pull, speed, af_num), `eos_gpio_mode_t` (INPUT, OUTPUT, AF, ANALOG), `eos_gpio_pull_t`, `eos_gpio_edge_t`

#### UART

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_uart_init` | `int eos_uart_init(const eos_uart_config_t *cfg)` | Configure UART port. |
| `eos_uart_write` | `int eos_uart_write(uint8_t port, const uint8_t *data, size_t len)` | Write data to UART. |
| `eos_uart_read` | `int eos_uart_read(uint8_t port, uint8_t *data, size_t len, uint32_t timeout_ms)` | Read with timeout. Returns bytes read. |

**Types:** `eos_uart_config_t` (port, baudrate, data_bits, parity, stop_bits)

#### SPI

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_spi_init` | `int eos_spi_init(const eos_spi_config_t *cfg)` | Configure SPI port. |
| `eos_spi_transfer` | `int eos_spi_transfer(uint8_t port, const uint8_t *tx, uint8_t *rx, size_t len)` | Full-duplex transfer. |
| `eos_spi_write` | `int eos_spi_write(uint8_t port, const uint8_t *data, size_t len)` | Write-only. |
| `eos_spi_read` | `int eos_spi_read(uint8_t port, uint8_t *data, size_t len)` | Read-only. |

#### I2C

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_i2c_init` | `int eos_i2c_init(const eos_i2c_config_t *cfg)` | Configure I2C port. |
| `eos_i2c_write` | `int eos_i2c_write(uint8_t port, uint16_t addr, const uint8_t *data, size_t len)` | Write to device. |
| `eos_i2c_read` | `int eos_i2c_read(uint8_t port, uint16_t addr, uint8_t *data, size_t len)` | Read from device. |
| `eos_i2c_write_reg` | `int eos_i2c_write_reg(uint8_t port, uint16_t addr, uint8_t reg, const uint8_t *data, size_t len)` | Write to register. |
| `eos_i2c_read_reg` | `int eos_i2c_read_reg(uint8_t port, uint16_t addr, uint8_t reg, uint8_t *data, size_t len)` | Read from register. |

#### Timer

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_timer_init` | `int eos_timer_init(const eos_timer_config_t *cfg)` | Configure hardware timer. |
| `eos_timer_start` | `int eos_timer_start(uint8_t timer_id)` | Start timer. |
| `eos_timer_stop` | `int eos_timer_stop(uint8_t timer_id)` | Stop timer. |

---

### Extended HAL (`eos/hal_extended.h`)

All extended peripherals are conditionally compiled behind `EOS_ENABLE_*` flags.

| Peripheral | API Prefix | Enable Flag | Key Functions |
|-----------|-----------|------------|---------------|
| ADC | `eos_adc_*` | `EOS_ENABLE_ADC` | `init`, `read`, `read_mv` |
| DAC | `eos_dac_*` | `EOS_ENABLE_DAC` | `init`, `write`, `write_mv` |
| PWM | `eos_pwm_*` | `EOS_ENABLE_PWM` | `init`, `set_duty`, `start`, `stop` |
| CAN | `eos_can_*` | `EOS_ENABLE_CAN` | `init`, `send`, `receive` |
| USB | `eos_usb_*` | `EOS_ENABLE_USB` | `init`, `write`, `read` |
| Ethernet | `eos_eth_*` | `EOS_ENABLE_ETHERNET` | `init`, `send`, `receive` |
| WiFi | `eos_wifi_*` | `EOS_ENABLE_WIFI` | `init`, `connect`, `send`, `receive` |
| BLE | `eos_ble_*` | `EOS_ENABLE_BLE` | `init`, `advertise_start`, `connect`, `send` |
| Camera | `eos_camera_*` | `EOS_ENABLE_CAMERA` | `init`, `capture`, `start_stream` |
| Audio | `eos_audio_*` | `EOS_ENABLE_AUDIO` | `init`, `play`, `record` |
| Display | `eos_display_*` | `EOS_ENABLE_DISPLAY` | `init`, `draw_pixel`, `draw_rect`, `flush` |
| Touch | `eos_touch_*` | `EOS_ENABLE_TOUCH` | `init`, `read`, `set_callback`, `calibrate` |
| GNSS | `eos_gnss_*` | `EOS_ENABLE_GNSS` | `init`, `get_position`, `has_fix` |
| IMU | `eos_imu_*` | `EOS_ENABLE_IMU` | `init`, `read_accel`, `read_gyro` |
| Motor | `eos_motor_*` | `EOS_ENABLE_MOTOR` | `init`, `set_speed`, `brake` |

---

## Kernel (`eos/kernel.h`)

### Task Management

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_kernel_init` | `int eos_kernel_init(void)` | Initialize kernel. |
| `eos_kernel_start` | `void eos_kernel_start(void)` | Start scheduler (does not return). |
| `eos_task_create` | `int eos_task_create(const char *name, eos_task_func_t entry, void *arg, uint8_t priority, uint32_t stack_size)` | Create a task. Returns task handle. |
| `eos_task_delete` | `int eos_task_delete(eos_task_handle_t handle)` | Delete a task. |
| `eos_task_delay_ms` | `void eos_task_delay_ms(uint32_t ms)` | Sleep current task. |
| `eos_task_yield` | `void eos_task_yield(void)` | Yield to scheduler. |

### Mutex

| Function | Description |
|----------|-------------|
| `eos_mutex_create(eos_mutex_handle_t *out)` | Create a mutex. |
| `eos_mutex_lock(handle, timeout_ms)` | Lock with timeout. Use `EOS_WAIT_FOREVER`. |
| `eos_mutex_unlock(handle)` | Unlock. |

### Semaphore

| Function | Description |
|----------|-------------|
| `eos_sem_create(out, initial, max)` | Create counting semaphore. |
| `eos_sem_wait(handle, timeout_ms)` | Wait (decrement). |
| `eos_sem_post(handle)` | Signal (increment). |

### Message Queue

| Function | Description |
|----------|-------------|
| `eos_queue_create(out, item_size, capacity)` | Create queue. |
| `eos_queue_send(handle, item, timeout_ms)` | Send item to queue. |
| `eos_queue_receive(handle, item, timeout_ms)` | Receive item from queue. |
| `eos_queue_count(handle)` | Get current queue depth. |

---

## Multicore (`eos/multicore.h`)

Requires `EOS_ENABLE_MULTICORE`.

| Function | Description |
|----------|-------------|
| `eos_multicore_init(mode)` | Init with `EOS_MP_SMP` or `EOS_MP_AMP`. |
| `eos_core_start(core_id, entry, arg)` | Start secondary core. |
| `eos_core_count()` | Get number of available cores. |
| `eos_core_id()` | Get current core ID. |
| `eos_spin_lock(lock)` | Acquire spinlock. |
| `eos_spin_unlock(lock)` | Release spinlock. |
| `eos_ipi_send(target, type, data)` | Send inter-processor interrupt. |
| `eos_shmem_create(cfg, region)` | Create shared memory region. |
| `eos_task_set_affinity(task_id, mask)` | Pin task to core(s). |

---

## Crypto (`eos/crypto.h`)

| Function | Description |
|----------|-------------|
| `eos_sha256_init/update/final` | SHA-256 hash computation. |
| `eos_sha256_data(data, len, hex)` | One-shot SHA-256 to hex string. |
| `eos_crc32(crc, data, len)` | CRC32 checksum. |
| `eos_aes_init(ctx, key, bits)` | Initialize AES context (128 or 256). |
| `eos_aes_cbc_encrypt/decrypt` | AES-CBC encryption/decryption. |
| `eos_rsa_sign_sha256` | RSA signature over SHA-256 hash. |
| `eos_rsa_verify_sha256` | RSA signature verification. |

---

## OTA (`eos/ota.h`)

Requires `EOS_ENABLE_OTA`.

| Function | Description |
|----------|-------------|
| `eos_ota_init()` | Initialize OTA subsystem. |
| `eos_ota_check_update(source, available)` | Check if update is available. |
| `eos_ota_begin(source)` | Begin downloading update. |
| `eos_ota_finish()` | Finalize download. |
| `eos_ota_verify()` | Verify integrity (SHA-256). |
| `eos_ota_apply()` | Apply update to inactive slot. |
| `eos_ota_rollback()` | Revert to previous firmware. |
| `eos_ota_get_status(status)` | Get current OTA state. |

---

## Source File Locations

| Module | Header | Source |
|--------|--------|--------|
| HAL Core | `eos/hal/include/eos/hal.h` | `eos/hal/src/hal_common.c` |
| HAL Extended | `eos/hal/include/eos/hal_extended.h` | `eos/hal/src/hal_extended_stubs.c` |
| Kernel | `eos/kernel/include/eos/kernel.h` | `eos/kernel/src/task.c`, `sync.c`, `ipc.c` |
| Multicore | `eos/kernel/include/eos/multicore.h` | `eos/kernel/src/multicore.c` |
| Crypto | `eos/services/crypto/include/eos/crypto.h` | `eos/services/crypto/src/sha256.c`, `aes.c`, `crc.c` |
| OTA | `eos/services/ota/include/eos/ota.h` | `eos/services/ota/src/ota.c` |
| POSIX | `eos/services/posix/include/eos/posix_*.h` | `eos/services/posix/src/posix_*.c` |
| UI | `eos/services/ui/include/eos/ui.h` | `eos/services/ui/src/ui.c`, `ui_display_drv.c`, `ui_touch_drv.c`, `ui_gesture.c` |
| Config | `eos/include/eos/eos_config.h` | — (header only) |
