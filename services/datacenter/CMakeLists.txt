add_library(eos_datacenter STATIC
    src/datacenter.c
)

target_include_directories(eos_datacenter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(eos_datacenter PRIVATE
    $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)
