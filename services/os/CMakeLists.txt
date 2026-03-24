add_library(eos_os_services STATIC
    src/os_services.c
)

target_include_directories(eos_os_services PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(eos_os_services PRIVATE eos_crypto)

target_compile_options(eos_os_services PRIVATE
    $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)
