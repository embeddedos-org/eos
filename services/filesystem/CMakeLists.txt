add_library(eos_filesystem STATIC
    src/filesystem.c
)

target_include_directories(eos_filesystem PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(eos_filesystem PUBLIC eos_hal)
