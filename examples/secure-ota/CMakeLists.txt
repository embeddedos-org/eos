cmake_minimum_required(VERSION 3.16)
project(secure-ota LANGUAGES C)

get_filename_component(EOS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_subdirectory(${EOS_ROOT} ${CMAKE_BINARY_DIR}/eos)

add_executable(secure-ota main.c)
target_link_libraries(secure-ota PRIVATE eos_hal eos_kernel)
target_include_directories(secure-ota PRIVATE
    ${EOS_ROOT}/hal/include
    ${EOS_ROOT}/kernel/include
    ${EOS_ROOT}/services/ota/include
    ${EOS_ROOT}/services/crypto/include
    ${EOS_ROOT}/services/os/include
    ${EOS_ROOT}/include
)
