# OS Adapter / Compatibility Layer
if(EOS_ENABLE_COMPAT OR NOT DEFINED EOS_ENABLE_COMPAT)

add_library(eos_compat STATIC
    src/os_adapter.c
    src/os_adapter_eos.c
)

# The POSIX adapter lets EoS services run on a host that already has an OS.
# Only built where pthreads exist; it is not a target-side component.
if(UNIX AND NOT CMAKE_CROSSCOMPILING)
    target_sources(eos_compat PRIVATE src/os_adapter_posix.c)
    target_compile_definitions(eos_compat PUBLIC EOS_HAVE_POSIX_ADAPTER=1)
    find_package(Threads REQUIRED)
    target_link_libraries(eos_compat PUBLIC Threads::Threads)
endif()

target_include_directories(eos_compat PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/kernel/include
    ${CMAKE_SOURCE_DIR}/hal/include
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(eos_compat PUBLIC eos_kernel eos_hal)

endif()
