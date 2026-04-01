add_library(eos_toolchains STATIC
    src/toolchain.c
)

target_include_directories(eos_toolchains PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(eos_toolchains PRIVATE eos_core)
