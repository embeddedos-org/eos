# EoS Linux IPC Emulation Layer
if(EOS_ENABLE_LINUX_IPC)

add_library(eos_linux_ipc STATIC
    src/linux_ipc.c
    src/linux_sysv_ipc.c
)

target_include_directories(eos_linux_ipc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/kernel/include
    ${CMAKE_SOURCE_DIR}/hal/include
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(eos_linux_ipc PUBLIC eos_kernel eos_hal)

target_compile_options(eos_linux_ipc PRIVATE
    $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)

endif()
