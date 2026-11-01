cmake_minimum_required(VERSION 3.16)
project(secure-ota LANGUAGES C)

get_filename_component(EOS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_subdirectory(${EOS_ROOT} ${CMAKE_BINARY_DIR}/eos)

add_executable(secure-ota main.c)
# eos/ota.h is empty unless the product enables OTA, and the EoS tree's
# EOS_PRODUCT_<NAME> definition is directory-scoped to that tree, so this
# target needs it too.
if(EOS_PRODUCT)
    string(TOUPPER "${EOS_PRODUCT}" EOS_PRODUCT_UPPER)
    target_compile_definitions(secure-ota PRIVATE EOS_PRODUCT_${EOS_PRODUCT_UPPER})
endif()
target_link_libraries(secure-ota PRIVATE eos_hal eos_kernel eos_ota eos_crypto)
target_include_directories(secure-ota PRIVATE
    ${EOS_ROOT}/hal/include
    ${EOS_ROOT}/kernel/include
    ${EOS_ROOT}/services/ota/include
    ${EOS_ROOT}/services/crypto/include
    ${EOS_ROOT}/services/os/include
    ${EOS_ROOT}/include
    ${EOS_ROOT}
)
