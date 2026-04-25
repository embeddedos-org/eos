# Appendix A: API Quick Reference

*Author: Srikanth Patchava & EmbeddedOS Contributors*

---

## Top 50 EoS API Functions

| #  | Function                          | Header           | Description                              |
|----|-----------------------------------|-------------------|------------------------------------------|
| 1  | `eos_hal_init()`                  | `eos/hal.h`       | Initialize the HAL with a backend        |
| 2  | `eos_hal_deinit()`                | `eos/hal.h`       | Deinitialize the HAL                     |
| 3  | `eos_gpio_init()`                 | `eos/hal.h`       | Configure a GPIO pin                     |
| 4  | `eos_gpio_write()`                | `eos/hal.h`       | Write digital value to GPIO pin          |
| 5  | `eos_gpio_read()`                 | `eos/hal.h`       | Read digital value from GPIO pin         |
| 6  | `eos_gpio_toggle()`               | `eos/hal.h`       | Toggle GPIO pin state                    |
| 7  | `eos_gpio_set_irq()`              | `eos/hal.h`       | Set GPIO interrupt callback              |
| 8  | `eos_uart_init()`                 | `eos/hal.h`       | Initialize UART peripheral               |
| 9  | `eos_uart_write()`                | `eos/hal.h`       | Write data to UART                       |
| 10 | `eos_uart_read()`                 | `eos/hal.h`       | Read data from UART with timeout         |
| 11 | `eos_spi_init()`                  | `eos/hal.h`       | Initialize SPI peripheral                |
| 12 | `eos_spi_transfer()`              | `eos/hal.h`       | Full-duplex SPI transfer                 |
| 13 | `eos_i2c_init()`                  | `eos/hal.h`       | Initialize I2C peripheral                |
| 14 | `eos_i2c_write()`                 | `eos/hal.h`       | Write data to I2C device                 |
| 15 | `eos_i2c_read()`                  | `eos/hal.h`       | Read data from I2C device                |
| 16 | `eos_timer_init()`                | `eos/hal.h`       | Initialize hardware timer                |
| 17 | `eos_timer_start()`               | `eos/hal.h`       | Start a timer                            |
| 18 | `eos_timer_stop()`                | `eos/hal.h`       | Stop a timer                             |
| 19 | `eos_delay_ms()`                  | `eos/hal.h`       | Blocking delay in milliseconds           |
| 20 | `eos_get_tick_ms()`               | `eos/hal.h`       | Get system tick in milliseconds          |
| 21 | `eos_kernel_init()`               | `eos/kernel.h`    | Initialize the RTOS kernel               |
| 22 | `eos_kernel_start()`              | `eos/kernel.h`    | Start the RTOS scheduler                 |
| 23 | `eos_task_create()`               | `eos/kernel.h`    | Create a new task                        |
| 24 | `eos_task_delete()`               | `eos/kernel.h`    | Delete a task                            |
| 25 | `eos_task_suspend()`              | `eos/kernel.h`    | Suspend a task                           |
| 26 | `eos_task_resume()`               | `eos/kernel.h`    | Resume a suspended task                  |
| 27 | `eos_task_get_state()`            | `eos/kernel.h`    | Get current task state                   |
| 28 | `eos_task_get_name()`             | `eos/kernel.h`    | Get task name string                     |
| 29 | `eos_mutex_create()`              | `eos/kernel.h`    | Create a recursive mutex                 |
| 30 | `eos_mutex_lock()`                | `eos/kernel.h`    | Lock mutex with timeout                  |
| 31 | `eos_mutex_unlock()`              | `eos/kernel.h`    | Unlock a mutex                           |
| 32 | `eos_mutex_delete()`              | `eos/kernel.h`    | Delete a mutex                           |
| 33 | `eos_sem_create()`                | `eos/kernel.h`    | Create a counting semaphore              |
| 34 | `eos_sem_wait()`                  | `eos/kernel.h`    | Wait on semaphore with timeout           |
| 35 | `eos_sem_post()`                  | `eos/kernel.h`    | Post (signal) a semaphore                |
| 36 | `eos_sem_get_count()`             | `eos/kernel.h`    | Get current semaphore count              |
| 37 | `eos_queue_create()`              | `eos/kernel.h`    | Create a message queue                   |
| 38 | `eos_queue_send()`                | `eos/kernel.h`    | Send message to queue                    |
| 39 | `eos_queue_receive()`             | `eos/kernel.h`    | Receive message from queue               |
| 40 | `eos_queue_is_empty()`            | `eos/kernel.h`    | Check if queue is empty                  |
| 41 | `eos_crypto_sha256()`             | `eos/crypto.h`    | Compute SHA-256 hash                     |
| 42 | `eos_crypto_aes_encrypt()`        | `eos/crypto.h`    | AES-256 encryption                       |
| 43 | `eos_crypto_aes_decrypt()`        | `eos/crypto.h`    | AES-256 decryption                       |
| 44 | `eos_net_socket_create()`         | `eos/net.h`       | Create a network socket                  |
| 45 | `eos_net_connect()`               | `eos/net.h`       | Connect socket to remote                 |
| 46 | `eos_net_send()`                  | `eos/net.h`       | Send data over socket                    |
| 47 | `eos_net_recv()`                  | `eos/net.h`       | Receive data from socket                 |
| 48 | `eos_fs_open()`                   | `eos/filesystem.h`| Open a file                              |
| 49 | `eos_fs_read()`                   | `eos/filesystem.h`| Read from a file                         |
| 50 | `eos_fs_write()`                  | `eos/filesystem.h`| Write to a file                          |

---

## Return Codes

| Code               | Value | Meaning                |
|---------------------|-------|------------------------|
| `EOS_OK`            | 0     | Success                |
| `EOS_ERR_INVALID`   | -1    | Invalid parameter      |
| `EOS_ERR_TIMEOUT`   | -2    | Operation timed out    |
| `EOS_ERR_NOMEM`     | -3    | Out of memory          |
| `EOS_ERR_BUSY`      | -4    | Resource busy          |
| `EOS_ERR_IO`        | -5    | I/O error              |
| `EOS_KERN_OK`       | 0     | Kernel operation OK    |
| `EOS_KERN_INVALID`  | -1    | Invalid kernel param   |
| `EOS_KERN_TIMEOUT`  | -2    | Kernel timeout         |
| `EOS_KERN_EMPTY`    | -3    | Queue/resource empty   |
