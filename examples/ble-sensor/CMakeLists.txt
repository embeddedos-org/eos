cmake_minimum_required(VERSION 3.16)
project(ble-sensor LANGUAGES C)

get_filename_component(EOS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_subdirectory(${EOS_ROOT} ${CMAKE_BINARY_DIR}/eos)

add_executable(ble-sensor main.c)
target_link_libraries(ble-sensor PRIVATE eos_hal eos_kernel)
target_include_directories(ble-sensor PRIVATE
    ${EOS_ROOT}/hal/include
    ${EOS_ROOT}/kernel/include
    ${EOS_ROOT}/services/crypto/include
    ${EOS_ROOT}/include
)
