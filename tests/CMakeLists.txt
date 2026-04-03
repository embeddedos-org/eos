# EoS Unit Tests
add_executable(test_config test_config.c)
target_link_libraries(test_config PRIVATE eos_core)
add_test(NAME test_config COMMAND test_config)
set_tests_properties(test_config PROPERTIES LABELS "core")

add_executable(test_graph test_graph.c)
target_link_libraries(test_graph PRIVATE eos_core)
add_test(NAME test_graph COMMAND test_graph)
set_tests_properties(test_graph PROPERTIES LABELS "core")

add_executable(test_hal test_hal.c)
target_link_libraries(test_hal PRIVATE eos_hal)
add_test(NAME test_hal COMMAND test_hal)
set_tests_properties(test_hal PROPERTIES LABELS "hal")

add_executable(test_kernel test_kernel.c)
target_link_libraries(test_kernel PRIVATE eos_kernel)
add_test(NAME test_kernel COMMAND test_kernel)
set_tests_properties(test_kernel PROPERTIES LABELS "kernel")

add_executable(test_multicore test_multicore.c)
target_link_libraries(test_multicore PRIVATE eos_kernel)
add_test(NAME test_multicore COMMAND test_multicore)
set_tests_properties(test_multicore PROPERTIES LABELS "kernel")

add_executable(test_drivers test_drivers.c)
target_link_libraries(test_drivers PRIVATE eos_drivers)
add_test(NAME test_drivers COMMAND test_drivers)
set_tests_properties(test_drivers PROPERTIES LABELS "driver")

add_executable(test_crypto test_crypto.c)
target_link_libraries(test_crypto PRIVATE eos_crypto)
add_test(NAME test_crypto COMMAND test_crypto)
set_tests_properties(test_crypto PROPERTIES LABELS "crypto")

add_executable(test_security test_security.c)
target_link_libraries(test_security PRIVATE eos_security eos_crypto)
add_test(NAME test_security COMMAND test_security)
set_tests_properties(test_security PROPERTIES LABELS "security")

add_executable(test_os_services test_os_services.c)
target_link_libraries(test_os_services PRIVATE eos_os_services eos_crypto)
add_test(NAME test_os_services COMMAND test_os_services)
set_tests_properties(test_os_services PROPERTIES LABELS "services")

add_executable(test_ota test_ota.c)
target_link_libraries(test_ota PRIVATE eos_ota eos_crypto)
add_test(NAME test_ota COMMAND test_ota)
set_tests_properties(test_ota PROPERTIES LABELS "services")

add_executable(test_sensor test_sensor.c)
target_link_libraries(test_sensor PRIVATE eos_sensor eos_hal)
if(UNIX AND NOT APPLE)
    target_link_libraries(test_sensor PRIVATE m)
endif()
add_test(NAME test_sensor COMMAND test_sensor)
set_tests_properties(test_sensor PROPERTIES LABELS "services")

add_executable(test_filesystem test_filesystem.c)
target_link_libraries(test_filesystem PRIVATE eos_filesystem)
add_test(NAME test_filesystem COMMAND test_filesystem)
set_tests_properties(test_filesystem PROPERTIES LABELS "services")

add_executable(test_service test_service.c)
target_link_libraries(test_service PRIVATE eos_service)
add_test(NAME test_service COMMAND test_service)
set_tests_properties(test_service PROPERTIES LABELS "services")

add_executable(test_debug test_debug.c)
target_include_directories(test_debug PRIVATE ${CMAKE_SOURCE_DIR}/debug/include)
target_link_libraries(test_debug PRIVATE eos_debug eos_core)
add_test(NAME test_debug COMMAND test_debug)
set_tests_properties(test_debug PROPERTIES LABELS "debug")

add_executable(test_ui_gesture test_ui_gesture.c)
target_include_directories(test_ui_gesture PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/hal/include
    ${CMAKE_SOURCE_DIR}/services/ui/include
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
)
add_test(NAME test_ui_gesture COMMAND test_ui_gesture)
set_tests_properties(test_ui_gesture PROPERTIES LABELS "ui")

add_executable(test_power test_power.c)
target_link_libraries(test_power PRIVATE eos_power)
add_test(NAME test_power COMMAND test_power)
set_tests_properties(test_power PROPERTIES LABELS "power")

add_executable(test_net test_net.c)
target_link_libraries(test_net PRIVATE eos_net)
add_test(NAME test_net COMMAND test_net)
set_tests_properties(test_net PROPERTIES LABELS "net")

add_executable(test_motor_ctrl test_motor_ctrl.c)
target_include_directories(test_motor_ctrl PRIVATE ${CMAKE_SOURCE_DIR}/services/motor/include)
target_link_libraries(test_motor_ctrl PRIVATE eos_motor_ctrl)
if(UNIX AND NOT APPLE)
    target_link_libraries(test_motor_ctrl PRIVATE m)
endif()
add_test(NAME test_motor_ctrl COMMAND test_motor_ctrl)
set_tests_properties(test_motor_ctrl PROPERTIES LABELS "services")

# ====================================================================
# New Crypto Tests (Phase 2)
# ====================================================================

add_executable(test_crypto_aes test_crypto_aes.c)
target_link_libraries(test_crypto_aes PRIVATE eos_crypto)
add_test(NAME test_crypto_aes COMMAND test_crypto_aes)
set_tests_properties(test_crypto_aes PROPERTIES LABELS "crypto" TIMEOUT 30)

add_executable(test_crypto_sha512 test_crypto_sha512.c)
target_link_libraries(test_crypto_sha512 PRIVATE eos_crypto)
add_test(NAME test_crypto_sha512 COMMAND test_crypto_sha512)
set_tests_properties(test_crypto_sha512 PROPERTIES LABELS "crypto" TIMEOUT 30)

add_executable(test_crypto_rsa test_crypto_rsa.c)
target_link_libraries(test_crypto_rsa PRIVATE eos_crypto)
add_test(NAME test_crypto_rsa COMMAND test_crypto_rsa)
set_tests_properties(test_crypto_rsa PROPERTIES LABELS "crypto" TIMEOUT 30)

add_executable(test_crypto_ecc test_crypto_ecc.c)
target_link_libraries(test_crypto_ecc PRIVATE eos_crypto)
add_test(NAME test_crypto_ecc COMMAND test_crypto_ecc)
set_tests_properties(test_crypto_ecc PROPERTIES LABELS "crypto" TIMEOUT 30)

# ====================================================================
# Regression & Validation Tests (Phase 2)
# ====================================================================

add_executable(test_profiles test_profiles.c)
add_test(NAME test_profiles COMMAND test_profiles ${CMAKE_SOURCE_DIR}/products)
set_tests_properties(test_profiles PROPERTIES LABELS "regression" TIMEOUT 60)

add_executable(test_board_configs test_board_configs.c)
add_test(NAME test_board_configs COMMAND test_board_configs ${CMAKE_SOURCE_DIR}/boards)
set_tests_properties(test_board_configs PROPERTIES LABELS "bsp;regression" TIMEOUT 60)

add_executable(test_driver_recovery test_driver_recovery.c)
target_link_libraries(test_driver_recovery PRIVATE eos_drivers)
add_test(NAME test_driver_recovery COMMAND test_driver_recovery)
set_tests_properties(test_driver_recovery PROPERTIES LABELS "driver" TIMEOUT 30)

# ====================================================================
# Fuzz Targets (optional, requires Clang)
# ====================================================================
add_subdirectory(fuzz)

# ====================================================================
# Firmware test (existing)
# ====================================================================
if(TARGET eos_firmware)
    add_executable(test_firmware test_firmware.c)
    target_link_libraries(test_firmware PRIVATE eos_firmware)
    add_test(NAME test_firmware COMMAND test_firmware)
    set_tests_properties(test_firmware PROPERTIES LABELS "firmware")
endif()

# ====================================================================
# Package test (existing)
# ====================================================================
if(TARGET eos_package)
    add_executable(test_package test_package.c)
    target_link_libraries(test_package PRIVATE eos_package)
    add_test(NAME test_package COMMAND test_package)
    set_tests_properties(test_package PROPERTIES LABELS "core")
endif()
