cmake_minimum_required(VERSION 3.15)

# EoS AI Layer — integrates EAI, ENI, EIPC into EoS
add_library(eos_ai STATIC
    src/eos_ai.c
)

target_include_directories(eos_ai PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Optional: link against EIPC C SDK if available
if(TARGET eipc_sdk_c)
    target_link_libraries(eos_ai PRIVATE eipc_sdk_c)
    target_compile_definitions(eos_ai PRIVATE
        ENI_EIPC_ENABLED
        EAI_EIPC_ENABLED
    )
endif()

# Optional: link against ENI if available
if(TARGET eni_common)
    target_link_libraries(eos_ai PRIVATE eni_common)
endif()

# Optional: link against EAI if available
if(TARGET eai_common)
    target_link_libraries(eos_ai PRIVATE eai_common)
endif()
