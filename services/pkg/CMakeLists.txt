# The .eapp package manager. Nothing built this directory until now, which is
# why its all-zero trust anchor survived: no target compiled the file, so no
# test could reach it. See #98.
#
# The target is eos_eapp, not eos_pkg: pkg/ already claims that name for the
# build-recipe package registry, a different module with a similar one.
add_library(eos_eapp STATIC
    eos_pkg.c
)

target_include_directories(eos_eapp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(eos_eapp PUBLIC eos_crypto)

target_compile_options(eos_eapp PRIVATE
    $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)
