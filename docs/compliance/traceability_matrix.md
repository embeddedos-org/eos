# Requirements Traceability Matrix — eos

## Format: Requirement -> Design -> Implementation -> Test -> Release

| Req ID | Requirement | Design Doc | Source File | Test Case | Release |
|--------|-------------|------------|-------------|-----------|---------|
| REQ-001 | RTOS task scheduling | kernel.h API | kernel/src/task.c | test_kernel.c:test_task_create | v0.1.0 |
| REQ-002 | Mutual exclusion | kernel.h mutex API | kernel/src/sync.c | test_kernel.c:test_mutex | v0.1.0 |
| REQ-003 | Message passing | kernel.h queue API | kernel/src/ipc.c | test_kernel.c:test_queue | v0.1.0 |
| REQ-004 | SHA-256 hashing | crypto.h API | services/crypto/src/sha256.c | test_crypto.c:test_sha256_nist | v0.1.0 |
| REQ-005 | OTA firmware update | ota.h API | services/ota/src/ota.c | test_ota.c:test_ota_full_update | v0.1.0 |
| REQ-006 | Sensor data filtering | sensor.h API | services/sensor/src/sensor.c | test_sensor.c:test_sensor_filter | v0.1.0 |
| REQ-007 | PID motor control | motor_ctrl.h API | services/motor/src/motor_ctrl.c | — | v0.1.0 |
| REQ-008 | File system operations | filesystem.h API | services/filesystem/src/filesystem.c | test_filesystem.c | v0.1.0 |
| REQ-009 | GDB remote debug | gdb_stub.h API | debug/src/gdb_stub.c | test_debug.c:test_gdb_* | v0.1.0 |
| REQ-010 | Core dump capture | coredump.h API | debug/src/coredump.c | test_debug.c:test_coredump_* | v0.1.0 |
| REQ-011 | Service lifecycle | service.h API | services/init/src/service.c | test_service.c | v0.1.0 |
| REQ-012 | Driver hot-plug | driver.h API | drivers/src/driver.c | test_drivers.c | v0.1.0 |
| REQ-013 | Device tree parsing | devicetree.h API | drivers/devicetree/devicetree.c | — | v0.1.0 |
| REQ-014 | Secure boot chain | eboot headers | core/eboot/stage0+stage1 | test_bootctl.c | v0.1.0 |
| REQ-015 | Cross-platform build | CMakeLists.txt | CI workflow | CI 3 platforms | v0.1.0 |
| REQ-016 | AES-128/256 encryption | crypto.h API | services/crypto/src/aes.c | test_crypto_aes.c | v0.2.0 |
| REQ-017 | SHA-512 hashing | crypto.h API | services/crypto/src/sha512.c | test_crypto_sha512.c | v0.2.0 |
| REQ-018 | RSA sign/verify | crypto.h API | services/crypto/src/rsa.c | test_crypto_rsa.c | v0.2.0 |
| REQ-019 | ECC sign/verify | crypto.h API | services/crypto/src/ecc.c | test_crypto_ecc.c | v0.2.0 |
| REQ-020 | Board config validation | boards/*.yaml | boards/ | test_board_configs.c | v0.2.0 |
| REQ-021 | Product profile regression | products/*.yaml | products/ | test_profiles.c | v0.2.0 |
| REQ-022 | Driver error recovery | drivers.h API | drivers/src/driver_framework.c | test_driver_recovery.c | v0.2.0 |
| REQ-023 | STM32F4 GPIO | hal_extended.h | hal/src/hal_stm32f4/hal_stm32f4_gpio.c | bsp_test_runner.c | v0.2.0 |
| REQ-024 | STM32F4 UART | hal_extended.h | hal/src/hal_stm32f4/hal_stm32f4_uart.c | bsp_test_runner.c | v0.2.0 |
| REQ-025 | Fuzz testing | — | tests/fuzz/ | fuzz_sha256.c, fuzz_aes.c | v0.2.0 |

## Traceability Coverage
- Requirements with tests: 21/25 (84%)
- Requirements with implementation: 25/25 (100%)
- Requirements in release: 25/25 (100%)
