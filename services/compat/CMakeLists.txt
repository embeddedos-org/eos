# OS Adapter / Compatibility Layer
if(EOS_ENABLE_COMPAT OR NOT DEFINED EOS_ENABLE_COMPAT)

add_library(eos_compat STATIC
    src/os_adapter.c
    src/os_adapter_eos.c
)

target_include_directories(eos_compat PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/kernel/include
    ${CMAKE_SOURCE_DIR}/hal/include
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(eos_compat PUBLIC eos_kernel eos_hal)

endif()
