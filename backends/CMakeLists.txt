add_library(eos_backends STATIC
    src/backend.c
    src/cmake_backend.c
    src/ninja_backend.c
    src/make_backend.c
    src/kbuild_backend.c
    src/zephyr_backend.c
    src/freertos_backend.c
    src/nuttx_backend.c
    src/meson_backend.c
    src/autotools_backend.c
    src/buildroot_backend.c
    src/cargo_backend.c
)

target_link_libraries(eos_backends PRIVATE eos_core)

# The backends read package options through the eos_pkg_option_*
# accessors (#148 Step 0c), which live in eos_pkg. PUBLIC, so every
# consumer of eos_backends links the accessors too — declaring it only
# here keeps future consumers from rediscovering the dependency.
target_link_libraries(eos_backends PUBLIC eos_pkg)

target_compile_options(eos_backends PRIVATE
    $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)
