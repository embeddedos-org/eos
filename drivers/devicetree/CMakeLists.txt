add_library(eos_devicetree STATIC
    devicetree.c
)
target_include_directories(eos_devicetree PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)