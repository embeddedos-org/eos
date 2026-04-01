cmake_minimum_required(VERSION 3.16)
project(uart-echo LANGUAGES C)

get_filename_component(EOS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_subdirectory(${EOS_ROOT} ${CMAKE_BINARY_DIR}/eos)

add_executable(uart-echo main.c)
target_link_libraries(uart-echo PRIVATE eos_hal)
target_include_directories(uart-echo PRIVATE
    ${EOS_ROOT}/hal/include
    ${EOS_ROOT}/include
)
