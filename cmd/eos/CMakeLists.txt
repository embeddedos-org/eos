add_executable(eos main.c)

target_link_libraries(eos PRIVATE
    eos_core
    eos_backends
    eos_pkg
    eos_toolchains
    eos_systems
)

target_compile_options(eos PRIVATE
    $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)
