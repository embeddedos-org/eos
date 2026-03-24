cmake_minimum_required(VERSION 3.16)
project(blink-gpio LANGUAGES C)

# Resolve EoS root (two levels up from examples/blink-gpio/)
get_filename_component(EOS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

# Add eos as a subdirectory
add_subdirectory(${EOS_ROOT} ${CMAKE_BINARY_DIR}/eos)

# Build the example
add_executable(blink-gpio main.c)
target_link_libraries(blink-gpio PRIVATE eos_hal)
target_include_directories(blink-gpio PRIVATE
    ${EOS_ROOT}/hal/include
    ${EOS_ROOT}/include
)
