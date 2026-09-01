# EoS Unit Tests
# --- test_board_configs: Board YAML schema shape ---
add_executable(test_board_configs test_board_configs.c)
add_test(NAME test_board_configs COMMAND test_board_configs boards)
set_tests_properties(test_board_configs PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# --- test_os_adapter: OS abstraction layer and its adapters ---
add_executable(test_os_adapter test_os_adapter.c)
target_link_libraries(test_os_adapter PRIVATE eos_compat)
add_test(NAME test_os_adapter COMMAND test_os_adapter)

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

# --- test_priority_inherit: Mutex priority inheritance ---
add_executable(test_priority_inherit test_priority_inherit.c)
target_link_libraries(test_priority_inherit PRIVATE eos_kernel)
add_test(NAME test_priority_inherit COMMAND test_priority_inherit)

# --- test_heap: Kernel allocator and overflow handling ---
add_executable(test_heap test_heap.c)
target_link_libraries(test_heap PRIVATE eos_kernel)
add_test(NAME test_heap COMMAND test_heap)

# --- test_multicore: Multicore framework ---
add_executable(test_multicore test_multicore.c)
target_link_libraries(test_multicore PRIVATE eos_kernel)
add_test(NAME test_multicore COMMAND test_multicore)

# --- test_drivers: Driver framework ---
add_executable(test_drivers test_drivers.c)
target_link_libraries(test_drivers PRIVATE eos_drivers)
add_test(NAME test_drivers COMMAND test_drivers)

# --- test_crypto: SHA-256, CRC32 ---
add_executable(test_crypto test_crypto.c)
target_link_libraries(test_crypto PRIVATE eos_crypto)
add_test(NAME test_crypto COMMAND test_crypto)

# --- test_crypto_aes: AES-128/192/256 ECB + CBC, NIST FIPS-197 / SP 800-38A ---
add_executable(test_crypto_aes test_crypto_aes.c)
target_link_libraries(test_crypto_aes PRIVATE eos_crypto)
add_test(NAME test_crypto_aes COMMAND test_crypto_aes)

# --- test_crypto_ecc: ECC point arithmetic and ECDSA ---
add_executable(test_crypto_ecc test_crypto_ecc.c)
target_link_libraries(test_crypto_ecc PRIVATE eos_crypto)
add_test(NAME test_crypto_ecc COMMAND test_crypto_ecc)

# --- test_crypto_rsa: RSA modular exponentiation and verify ---
add_executable(test_crypto_rsa test_crypto_rsa.c)
target_link_libraries(test_crypto_rsa PRIVATE eos_crypto)
add_test(NAME test_crypto_rsa COMMAND test_crypto_rsa)

add_executable(test_crypto_sha512 test_crypto_sha512.c)
target_link_libraries(test_crypto_sha512 PRIVATE eos_crypto)
add_test(NAME test_crypto_sha512 COMMAND test_crypto_sha512)

# --- test_security: Keystore, ACL ---
add_executable(test_security test_security.c)
target_link_libraries(test_security PRIVATE eos_security eos_crypto)
add_test(NAME test_security COMMAND test_security)

# --- test_os_services: Watchdog, audit ---
add_executable(test_os_services test_os_services.c)
target_link_libraries(test_os_services PRIVATE eos_os_services eos_crypto)
add_test(NAME test_os_services COMMAND test_os_services)

# --- test_ota: OTA update with SHA-256 verify ---
add_executable(test_ota test_ota.c)
target_link_libraries(test_ota PRIVATE eos_ota eos_crypto)
add_test(NAME test_ota COMMAND test_ota)

# --- test_sensor: Sensor framework with filters ---
add_executable(test_sensor test_sensor.c)
target_link_libraries(test_sensor PRIVATE eos_sensor eos_hal)
if(UNIX AND NOT APPLE)
    target_link_libraries(test_sensor PRIVATE m)
endif()
add_test(NAME test_sensor COMMAND test_sensor)

# --- test_filesystem: RAM filesystem ---
add_executable(test_filesystem test_filesystem.c)
target_link_libraries(test_filesystem PRIVATE eos_filesystem)
add_test(NAME test_filesystem COMMAND test_filesystem)

# --- test_service: Service manager ---
add_executable(test_service test_service.c)
target_link_libraries(test_service PRIVATE eos_service)
add_test(NAME test_service COMMAND test_service)

# --- test_gps: GPS NMEA sentence parsing ---
add_executable(test_gps test_gps.c)
target_link_libraries(test_gps PRIVATE eos_gps)
if(UNIX AND NOT APPLE)
    target_link_libraries(test_gps PRIVATE m)
endif()
add_test(NAME test_gps COMMAND test_gps)

# --- test_debug: GDB stub + core dump ---
add_executable(test_debug test_debug.c)
target_include_directories(test_debug PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../debug/include
)
target_link_libraries(test_debug PRIVATE eos_debug eos_core)
add_test(NAME test_debug COMMAND test_debug)

# --- test_ui_gesture: UI gesture recognition ---
add_executable(test_ui_gesture test_ui_gesture.c)
target_compile_definitions(test_ui_gesture PRIVATE
    EOS_ENABLE_UI=1
    EOS_ENABLE_TOUCH=1
    EOS_ENABLE_DISPLAY=1
)
target_include_directories(test_ui_gesture PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../hal/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../services/ui/include
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
)
add_test(NAME test_ui_gesture COMMAND test_ui_gesture)

# --- test_power: Power management framework ---
add_executable(test_power test_power.c)
target_link_libraries(test_power PRIVATE eos_power)
add_test(NAME test_power COMMAND test_power)

# --- test_net: Networking abstraction ---
add_executable(test_net test_net.c)
target_link_libraries(test_net PRIVATE eos_net)
# The POSIX round-trip cases need the same switch the library was built with,
# plus pthreads for the listener they connect to.
if(NOT CMAKE_CROSSCOMPILING)
    target_compile_definitions(test_net PRIVATE EOS_NET_POSIX=1)
    find_package(Threads REQUIRED)
    target_link_libraries(test_net PRIVATE Threads::Threads)
endif()
add_test(NAME test_net COMMAND test_net)

# --- test_motor_ctrl: Motor control PID ---
add_executable(test_motor_ctrl test_motor_ctrl.c)
target_include_directories(test_motor_ctrl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../services/motor/include
)
target_link_libraries(test_motor_ctrl PRIVATE eos_motor_ctrl)
if(UNIX AND NOT APPLE)
    target_link_libraries(test_motor_ctrl PRIVATE m)
endif()
add_test(NAME test_motor_ctrl COMMAND test_motor_ctrl)

# --- test_package: Package management ---
add_executable(test_package test_package.c)
target_include_directories(test_package PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
)
target_link_libraries(test_package PRIVATE eos_pkg eos_core)
add_test(NAME test_package COMMAND test_package)

# --- test_devicetree: Flattened device tree parser ---
add_executable(test_devicetree test_devicetree.c)
target_link_libraries(test_devicetree PRIVATE eos_devicetree)
add_test(NAME test_devicetree COMMAND test_devicetree)


# ---------------------------------------------------------------------------
# Suites that existed in tests/ but were never registered.
#
# Each of these has a main() and compiles; none was in any add_executable(),
# so CI never built or ran them. test_crypto_failclosed.c is the one that
# matters most -- it asserts that the stub RSA/ECC routines refuse to run,
# written after a 256-byte file with the right trailing bytes made secure
# boot report VERIFIED for an arbitrary image. A regression guard that is
# never executed is exactly the failure it was written to prevent.
# ---------------------------------------------------------------------------
add_executable(test_crypto_failclosed test_crypto_failclosed.c)
target_link_libraries(test_crypto_failclosed PRIVATE eos_crypto)
add_test(NAME test_crypto_failclosed COMMAND test_crypto_failclosed)

add_executable(test_crc test_crc.c)
target_link_libraries(test_crc PRIVATE eos_crypto)
add_test(NAME test_crc COMMAND test_crc)

add_executable(test_driver_recovery test_driver_recovery.c)
target_link_libraries(test_driver_recovery PRIVATE eos_drivers)
add_test(NAME test_driver_recovery COMMAND test_driver_recovery)

# test_net_integration.c is deliberately NOT registered. It hangs.
#
# test_accept_no_client() asserts that eos_net_accept() returns
# EOS_SOCKET_INVALID with no client pending -- its own comment says
# "No connection pending in stub". That was true of the net.c stub, which
# returned INVALID unconditionally. Since the POSIX implementation landed,
# accept() on a blocking listening socket waits for a connection that
# never arrives, so the suite never returns.
#
# eos_net_accept() takes no timeout and there is no non-blocking option in
# the API, so the test cannot simply be retimed: either it grows a client
# to accept, or the API grows a way to poll. That is a design decision,
# not a registration, so it is left out here rather than bundled in.

add_executable(test_stress test_stress.c)
target_link_libraries(test_stress PRIVATE eos_kernel)
add_test(NAME test_stress COMMAND test_stress)

# --- Fuzz targets ---
# Opt-in (-DEOS_ENABLE_FUZZING=ON, Clang only); the subdirectory guards itself.
add_subdirectory(fuzz)