add_library(eos_service STATIC
    src/service.c
)
target_include_directories(eos_service PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)