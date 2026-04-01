# POSIX Compatibility Layer for EoS
# Gated by EOS_ENABLE_POSIX option

option(EOS_ENABLE_POSIX "Enable POSIX compatibility layer" OFF)

if(EOS_ENABLE_POSIX)

    add_library(eos_posix STATIC
        src/posix_threads.c
        src/posix_sync.c
        src/posix_ipc.c
        src/posix_signals.c
        src/posix_io.c
        src/posix_time.c
    )

    target_include_directories(eos_posix
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../../kernel/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../../hal/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    )

    target_link_libraries(eos_posix
        PUBLIC
            eos_kernel
            eos_hal
    )

    # Propagate POSIX include path to consumers
    target_compile_definitions(eos_posix PUBLIC EOS_ENABLE_POSIX=1)

endif()
