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
# eos_backends, not just at the consumers: systems/src/firmware.c includes
# eos/backend.h and calls eos_backend_find(), so this is where the dependency
# belongs. Declaring it at each consumer instead made every future user of
# eos_systems rediscover it -- which is how test_firmware came to name it.
target_link_libraries(eos_systems PRIVATE eos_core eos_toolchains eos_backends)
