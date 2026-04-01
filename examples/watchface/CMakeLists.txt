cmake_minimum_required(VERSION 3.16)
project(eos_example_watchface C)

find_package(eos QUIET)
find_package(lvgl QUIET)

add_executable(watchface main.c)

target_link_libraries(watchface PRIVATE
    eos_ui
    eos_hal
    eos_kernel
)

if(TARGET lvgl)
    target_link_libraries(watchface PRIVATE lvgl)
endif()
