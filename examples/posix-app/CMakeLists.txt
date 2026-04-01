cmake_minimum_required(VERSION 3.16)
project(posix-app LANGUAGES C)

get_filename_component(EOS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_subdirectory(${EOS_ROOT} ${CMAKE_BINARY_DIR}/eos)

add_executable(posix-app main.c)
target_link_libraries(posix-app PRIVATE eos_hal)
target_include_directories(posix-app PRIVATE
    ${EOS_ROOT}/hal/include
    ${EOS_ROOT}/services/posix/include
    ${EOS_ROOT}/include
)
