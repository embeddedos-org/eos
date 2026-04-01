cmake_minimum_required(VERSION 3.16)
project(multitask-rtos LANGUAGES C)

get_filename_component(EOS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_subdirectory(${EOS_ROOT} ${CMAKE_BINARY_DIR}/eos)

add_executable(multitask-rtos main.c)
target_link_libraries(multitask-rtos PRIVATE eos_hal eos_kernel)
target_include_directories(multitask-rtos PRIVATE
    ${EOS_ROOT}/hal/include
    ${EOS_ROOT}/kernel/include
    ${EOS_ROOT}/include
)
