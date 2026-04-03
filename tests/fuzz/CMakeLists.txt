# EoS Fuzz Testing Targets
#
# Build with: cmake -B build -DEOS_ENABLE_FUZZING=ON -DCMAKE_C_COMPILER=clang
# Run:        ./build/tests/fuzz/fuzz_sha256 -max_total_time=60

option(EOS_ENABLE_FUZZING "Build libFuzzer fuzz targets" OFF)

if(EOS_ENABLE_FUZZING)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
        message(WARNING "Fuzzing requires Clang — skipping fuzz targets")
        return()
    endif()

    set(FUZZ_FLAGS "-fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer")

    add_executable(fuzz_sha256 fuzz_sha256.c)
    target_link_libraries(fuzz_sha256 PRIVATE eos_crypto)
    set_target_properties(fuzz_sha256 PROPERTIES
        COMPILE_FLAGS "${FUZZ_FLAGS}"
        LINK_FLAGS "${FUZZ_FLAGS}"
    )

    add_executable(fuzz_aes fuzz_aes.c)
    target_link_libraries(fuzz_aes PRIVATE eos_crypto)
    set_target_properties(fuzz_aes PROPERTIES
        COMPILE_FLAGS "${FUZZ_FLAGS}"
        LINK_FLAGS "${FUZZ_FLAGS}"
    )

    # These require the parser libraries to be linked
    # add_executable(fuzz_devicetree fuzz_devicetree.c)
    # target_link_libraries(fuzz_devicetree PRIVATE eos_devicetree)
    # set_target_properties(fuzz_devicetree PROPERTIES
    #     COMPILE_FLAGS "${FUZZ_FLAGS}"
    #     LINK_FLAGS "${FUZZ_FLAGS}"
    # )

    # add_executable(fuzz_ota_header fuzz_ota_header.c)
    # target_link_libraries(fuzz_ota_header PRIVATE eos_ota)
    # set_target_properties(fuzz_ota_header PROPERTIES
    #     COMPILE_FLAGS "${FUZZ_FLAGS}"
    #     LINK_FLAGS "${FUZZ_FLAGS}"
    # )

    message(STATUS "  Fuzz targets: fuzz_sha256, fuzz_aes")
endif()
