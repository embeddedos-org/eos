# Chapter 34: Testing and Quality Assurance

*Author: Srikanth Patchava & EmbeddedOS Contributors*

---

## 34.1 Introduction

Quality assurance is non-negotiable in embedded systems. A bug in a medical
device or automotive controller can have life-threatening consequences. EoS
implements a comprehensive testing strategy spanning unit tests, integration
tests, fuzz testing, and CI validation across all supported platforms.

---

## 34.2 Test Infrastructure

The EoS test suite is located in the `tests/` directory:

| Test File                | Coverage Area                        |
|--------------------------|--------------------------------------|
| `test_hal.c`             | Hardware Abstraction Layer           |
| `test_kernel.c`          | RTOS kernel (tasks, mutexes, queues) |
| `test_drivers.c`         | Driver framework                     |
| `test_crypto.c`          | Cryptographic services               |
| `test_crypto_aes.c`      | AES encryption                       |
| `test_crypto_ecc.c`      | Elliptic curve cryptography          |
| `test_crypto_rsa.c`      | RSA operations                       |
| `test_crypto_sha512.c`   | SHA-512 hashing                      |
| `test_filesystem.c`      | Filesystem operations                |
| `test_firmware.c`        | Firmware management                  |
| `test_net.c`             | Networking stack                     |
| `test_security.c`        | Security services                    |
| `test_sensor.c`          | Sensor subsystem                     |
| `test_motor_ctrl.c`      | Motor control                        |
| `test_multicore.c`       | SMP/AMP multicore                    |
| `test_power.c`           | Power management                     |
| `test_ota.c`             | Over-the-air updates                 |
| `test_os_services.c`     | OS-level services                    |
| `test_config.c`          | Configuration system                 |
| `test_profiles.c`        | Product profiles                     |
| `test_driver_recovery.c` | Driver fault recovery                |
| `test_ui_gesture.c`      | UI gesture recognition               |
| `test_graph.c`           | Graph data structures                |
| `test_board_configs.c`   | Board configuration validation       |
| `test_service.c`         | Service framework                    |
| `test_package.c`         | Package management                   |
| `test_debug.c`           | Debug facilities                     |

---

## 34.3 Test Framework

EoS uses a lightweight, custom test framework designed for portability:

```c
#define TEST(name) \
    static void name(void); \
    static void run_##name(void) { \
        printf("  %-50s ", #name); \
        name(); \
        tests_passed++; \
        printf("[PASS]\n"); \
    } \
    static void name(void)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while(0)
```

This framework compiles on any C11 compiler with zero dependencies,
making it suitable for both host and cross-compiled testing.

---

## 34.4 HAL Testing with Mock Backends

The HAL tests use mock backends that simulate hardware behavior:

```c
static const eos_hal_backend_t mock_backend = {
    .name = "mock",
    .init = mock_init,
    .deinit = mock_deinit,
    .delay_ms = mock_delay,
    .get_tick_ms = mock_get_tick,
    .gpio_init = mock_gpio_init,
    .gpio_write = mock_gpio_write,
    .gpio_read = mock_gpio_read,
    .gpio_toggle = mock_gpio_toggle,
    .uart_init = mock_uart_init,
    .uart_write = mock_uart_write,
    .spi_init = mock_spi_init,
    .spi_transfer = mock_spi_transfer,
    .i2c_init = mock_i2c_init,
    .i2c_write = mock_i2c_write,
    .i2c_read = mock_i2c_read,
    .timer_init = mock_timer_init,
    .timer_start = mock_timer_start,
    .irq_disable = mock_irq_disable,
    .irq_enable = mock_irq_enable,
};
```

This enables full HAL testing on the host machine without hardware.

---

## 34.5 Kernel Testing

Kernel tests validate the RTOS primitives:

```c
static void test_task_create(void) {
    eos_kernel_init();
    int h = eos_task_create("t1", test_entry, NULL, 5, 1024);
    assert(h >= 0);
    assert(eos_task_get_state(h) == EOS_TASK_READY);
    assert(strcmp(eos_task_get_name(h), "t1") == 0);
}

static void test_mutex(void) {
    eos_kernel_init();
    eos_mutex_handle_t m;
    assert(eos_mutex_create(&m) == EOS_KERN_OK);
    assert(eos_mutex_lock(m, 0) == EOS_KERN_OK);
    assert(eos_mutex_lock(m, 0) == EOS_KERN_OK);  // recursive
    assert(eos_mutex_unlock(m) == EOS_KERN_OK);
    assert(eos_mutex_delete(m) == EOS_KERN_OK);
}

static void test_semaphore(void) {
    eos_kernel_init();
    eos_sem_handle_t s;
    assert(eos_sem_create(&s, 3, 5) == EOS_KERN_OK);
    assert(eos_sem_get_count(s) == 3);
    assert(eos_sem_wait(s, 0) == EOS_KERN_OK);
    assert(eos_sem_get_count(s) == 2);
}

static void test_queue(void) {
    eos_kernel_init();
    eos_queue_handle_t q;
    assert(eos_queue_create(&q, sizeof(int), 4) == EOS_KERN_OK);
    int val = 42;
    assert(eos_queue_send(q, &val, 0) == EOS_KERN_OK);
    int out = 0;
    assert(eos_queue_receive(q, &out, 0) == EOS_KERN_OK);
    assert(out == 42);
}
```

---

## 34.6 Fuzz Testing

The `tests/fuzz/` directory contains fuzz test harnesses for security-
critical components:

- Crypto input fuzzing
- Network packet parsing
- Configuration file parsing
- Filesystem operation fuzzing

Fuzz tests use libFuzzer or AFL and run in the nightly CI pipeline.

---

## 34.7 Running Tests

```bash
# Build with tests enabled
cmake -B build -DEOS_BUILD_TESTS=ON
cmake --build build

# Run all tests
ctest --test-dir build --output-on-failure

# Run specific test
./build/test_kernel

# Run with verbose output
ctest --test-dir build -V
```

---

## 34.8 CI Pipeline

All pull requests must pass:

| Check                     | Platform | What It Validates              |
|---------------------------|----------|--------------------------------|
| Build (ubuntu-latest)     | Linux    | GCC build with all libraries   |
| Build (windows-latest)    | Windows  | MSVC cross-platform check      |
| Build (macos-latest)      | macOS    | Clang build verification       |
| Product (robot)           | Linux    | Robot profile compiles         |
| Product (gateway)         | Linux    | Gateway profile compiles       |
| Product (medical)         | Linux    | Medical profile compiles       |
| Product (automotive)      | Linux    | Automotive profile compiles    |
| Product (iot)             | Linux    | IoT profile compiles           |
| Product (aerospace)       | Linux    | Aerospace profile compiles     |

### Nightly Regression

Additional nightly checks include:
- Full test suite on all 3 OS platforms
- All product profile builds
- Cross-compilation for AArch64, ARM hard-float, and RISC-V

---

## 34.9 Code Quality Standards

| Standard        | Requirement                               |
|-----------------|-------------------------------------------|
| C Standard      | C11 (`-std=c11`)                          |
| Warnings        | `-Wall -Wextra` with zero warnings        |
| Platform guards | All OS-specific code guarded with `#ifdef`|
| Naming          | `snake_case` with `eos_` prefix           |
| Documentation   | Doxygen-compatible `/** */` doc comments  |
| Types           | `<stdint.h>` types (`uint32_t`, etc.)     |

---

## 34.10 Test Coverage Goals

| Module              | Target Coverage | Current |
|---------------------|-----------------|---------|
| HAL                 | 90%             | 85%     |
| Kernel              | 95%             | 92%     |
| Crypto              | 95%             | 90%     |
| Networking          | 85%             | 80%     |
| Drivers             | 80%             | 75%     |
| Services            | 85%             | 80%     |

---

## 34.11 Summary

EoS implements a rigorous testing strategy appropriate for safety-critical
embedded systems.

**Key takeaways:**

- 27 test files covering HAL, kernel, crypto, networking, and more
- Mock-based HAL testing enables host-machine validation
- Fuzz testing for security-critical components
- CI validates builds on Linux, Windows, macOS
- Product profile builds verified for 6 safety-critical profiles
- Nightly cross-compilation regression for all architectures

---

*Next: Chapter 35 — Safety and Compliance*
