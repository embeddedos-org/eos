add_library(eos_systems STATIC
    src/system.c
    src/rootfs.c
    src/image.c
    src/docs.c
    src/firmware.c
)
target_include_directories(eos_systems PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(eos_systems PRIVATE eos_core eos_toolchains)
