cmake_minimum_required(VERSION 3.16)
project(multicore-amp LANGUAGES C)

get_filename_component(EOS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_subdirectory(${EOS_ROOT} ${CMAKE_BINARY_DIR}/eos)

add_executable(multicore-amp main.c)
target_link_libraries(multicore-amp PRIVATE eos_hal eos_kernel)
target_include_directories(multicore-amp PRIVATE
    ${EOS_ROOT}/hal/include
    ${EOS_ROOT}/kernel/include
    ${EOS_ROOT}/include
)
