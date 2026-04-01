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

target_include_directories(eos_backends PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(eos_backends PRIVATE eos_core)

target_compile_options(eos_backends PRIVATE
    $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)
