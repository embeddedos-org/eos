add_library(eos_sensor STATIC
    src/sensor.c
)

target_include_directories(eos_sensor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(eos_sensor PUBLIC eos_hal)
