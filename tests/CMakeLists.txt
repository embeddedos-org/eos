# EoS Unit Tests
add_executable(test_config test_config.c)
target_link_libraries(test_config PRIVATE eos_core)
add_test(NAME test_config COMMAND test_config)

add_executable(test_graph test_graph.c)
target_link_libraries(test_graph PRIVATE eos_core)
add_test(NAME test_graph COMMAND test_graph)

add_executable(test_hal test_hal.c)
target_link_libraries(test_hal PRIVATE eos_hal)
add_test(NAME test_hal COMMAND test_hal)

add_executable(test_kernel test_kernel.c)
target_link_libraries(test_kernel PRIVATE eos_kernel)
add_test(NAME test_kernel COMMAND test_kernel)

add_executable(test_multicore test_multicore.c)
target_link_libraries(test_multicore PRIVATE eos_kernel)
add_test(NAME test_multicore COMMAND test_multicore)

add_executable(test_drivers test_drivers.c)
target_link_libraries(test_drivers PRIVATE eos_drivers)
add_test(NAME test_drivers COMMAND test_drivers)

add_executable(test_crypto test_crypto.c)
target_link_libraries(test_crypto PRIVATE eos_crypto)
add_test(NAME test_crypto COMMAND test_crypto)

add_executable(test_security test_security.c)
target_link_libraries(test_security PRIVATE eos_security eos_crypto)
add_test(NAME test_security COMMAND test_security)

add_executable(test_os_services test_os_services.c)
target_link_libraries(test_os_services PRIVATE eos_os_services eos_crypto)
add_test(NAME test_os_services COMMAND test_os_services)

add_executable(test_ota test_ota.c)
target_link_libraries(test_ota PRIVATE eos_ota eos_crypto)
add_test(NAME test_ota COMMAND test_ota)

add_executable(test_sensor test_sensor.c)
target_link_libraries(test_sensor PRIVATE eos_sensor eos_hal)
if(UNIX AND NOT APPLE)
    target_link_libraries(test_sensor PRIVATE m)
endif()
add_test(NAME test_sensor COMMAND test_sensor)

add_executable(test_filesystem test_filesystem.c)
target_link_libraries(test_filesystem PRIVATE eos_filesystem)
add_test(NAME test_filesystem COMMAND test_filesystem)

add_executable(test_service test_service.c)
target_link_libraries(test_service PRIVATE eos_service)
add_test(NAME test_service COMMAND test_service)

add_executable(test_debug test_debug.c)
target_include_directories(test_debug PRIVATE ${CMAKE_SOURCE_DIR}/debug/include)
target_link_libraries(test_debug PRIVATE eos_debug eos_core)
add_test(NAME test_debug COMMAND test_debug)

add_executable(test_ui_gesture test_ui_gesture.c)
target_include_directories(test_ui_gesture PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/hal/include
    ${CMAKE_SOURCE_DIR}/services/ui/include
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
)
add_test(NAME test_ui_gesture COMMAND test_ui_gesture)

add_executable(test_power test_power.c)
target_link_libraries(test_power PRIVATE eos_power)
add_test(NAME test_power COMMAND test_power)

add_executable(test_net test_net.c)
target_link_libraries(test_net PRIVATE eos_net)
add_test(NAME test_net COMMAND test_net)

add_executable(test_motor_ctrl test_motor_ctrl.c)
target_include_directories(test_motor_ctrl PRIVATE ${CMAKE_SOURCE_DIR}/services/motor/include)
target_link_libraries(test_motor_ctrl PRIVATE eos_motor_ctrl)
if(UNIX AND NOT APPLE)
    target_link_libraries(test_motor_ctrl PRIVATE m)
endif()
add_test(NAME test_motor_ctrl COMMAND test_motor_ctrl)
