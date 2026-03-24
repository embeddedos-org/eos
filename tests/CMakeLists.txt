# EoS Unit Tests

# --- test_config: OS configuration ---
add_executable(test_config test_config.c)
target_link_libraries(test_config PRIVATE eos_core)
add_test(NAME test_config COMMAND test_config)

# --- test_graph: Dependency graph ---
add_executable(test_graph test_graph.c)
target_link_libraries(test_graph PRIVATE eos_core)
add_test(NAME test_graph COMMAND test_graph)

# --- test_hal: Hardware abstraction layer ---
add_executable(test_hal test_hal.c)
target_link_libraries(test_hal PRIVATE eos_hal)
add_test(NAME test_hal COMMAND test_hal)

# --- test_kernel: RTOS kernel (tasks, sync, IPC) ---
add_executable(test_kernel test_kernel.c)
target_link_libraries(test_kernel PRIVATE eos_kernel)
add_test(NAME test_kernel COMMAND test_kernel)

# --- test_drivers: Driver framework ---
add_executable(test_drivers test_drivers.c)
target_link_libraries(test_drivers PRIVATE eos_drivers)
add_test(NAME test_drivers COMMAND test_drivers)

# --- test_crypto: SHA-256, AES, CRC ---
add_executable(test_crypto test_crypto.c)
target_link_libraries(test_crypto PRIVATE eos_crypto)
add_test(NAME test_crypto COMMAND test_crypto)

# --- test_security: Keystore, ACL, secure boot ---
add_executable(test_security test_security.c)
target_link_libraries(test_security PRIVATE eos_security eos_crypto)
add_test(NAME test_security COMMAND test_security)

# --- test_os_services: Watchdog, audit, secure storage ---
add_executable(test_os_services test_os_services.c)
target_link_libraries(test_os_services PRIVATE eos_os_services eos_crypto)
add_test(NAME test_os_services COMMAND test_os_services)
