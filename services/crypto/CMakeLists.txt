add_library(eos_crypto STATIC
    src/sha256.c
    src/crc.c
    src/aes.c
    src/rsa_ecc_sha512.c
)

target_include_directories(eos_crypto PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(eos_crypto PRIVATE
    $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)
