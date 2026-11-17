add_executable(eos main.c exec_argv.c)

# The banner prints the project version from the generated eos/version.h;
# it used to be a literal in main.c that still said 0.2.0 at 0.5.0.
target_include_directories(eos PRIVATE ${EOS_GENERATED_INCLUDE_DIR})

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
