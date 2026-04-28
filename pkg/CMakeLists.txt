add_library(eos_pkg STATIC
    src/package.c
    src/fetch.c
)

target_include_directories(eos_pkg PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(eos_pkg PRIVATE eos_core)

target_compile_options(eos_pkg PRIVATE
    $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)
