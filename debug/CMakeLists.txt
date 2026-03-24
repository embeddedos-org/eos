add_library(eos_debug STATIC
    src/gdb_stub.c
    src/coredump.c
)
target_include_directories(eos_debug PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
)
target_link_libraries(eos_debug PUBLIC eos_core)