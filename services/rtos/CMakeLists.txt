add_library(eos_rtos_security STATIC
    src/rtos_security.c
)

target_include_directories(eos_rtos_security PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(eos_rtos_security PRIVATE
    $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)
