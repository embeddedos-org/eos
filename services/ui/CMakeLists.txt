option(EOS_BUILD_UI "Build EoS UI module (requires LVGL)" OFF)
if(NOT EOS_BUILD_UI)
    return()
endif()

add_library(eos_ui STATIC
    src/ui.c
    src/ui_display_drv.c
    src/ui_touch_drv.c
    src/ui_gesture.c
)
target_include_directories(eos_ui
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_include_directories(eos_ui
    PUBLIC ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(eos_ui
    PUBLIC eos_hal
)
if(TARGET lvgl)
    target_link_libraries(eos_ui PUBLIC lvgl)
endif()
