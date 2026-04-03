cmake_minimum_required(VERSION 3.16)
project(ui-touchscreen LANGUAGES C)

get_filename_component(EOS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_subdirectory(${EOS_ROOT} ${CMAKE_BINARY_DIR}/eos)

add_executable(ui-touchscreen main.c)
target_link_libraries(ui-touchscreen PRIVATE eos_hal eos_kernel eos_ui)
target_include_directories(ui-touchscreen PRIVATE
    ${EOS_ROOT}/hal/include
    ${EOS_ROOT}/kernel/include
    ${EOS_ROOT}/services/ui/include
    ${EOS_ROOT}/include
)
