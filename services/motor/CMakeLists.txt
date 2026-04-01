add_library(eos_motor_ctrl STATIC
    src/motor_ctrl.c
)

target_include_directories(eos_motor_ctrl PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(eos_motor_ctrl PUBLIC eos_hal)
