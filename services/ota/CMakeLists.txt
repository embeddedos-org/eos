add_library(eos_ota STATIC
    src/ota.c
)
target_include_directories(eos_ota PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/services/crypto/include
)
target_link_libraries(eos_ota PUBLIC eos_hal eos_crypto)