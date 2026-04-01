# VxWorks Compatibility Layer for EoS
# Gated by EOS_ENABLE_VXWORKS CMake option (set in parent CMakeLists.txt)

add_library(eos_vxworks STATIC
    src/vxworks_task.c
    src/vxworks_sem.c
    src/vxworks_msgq.c
    src/vxworks_wd.c
    src/vxworks_int.c
)

target_include_directories(eos_vxworks PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/kernel/include
    ${CMAKE_SOURCE_DIR}/hal/include
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(eos_vxworks PUBLIC eos_kernel eos_hal)
