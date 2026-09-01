add_library(eos_gps STATIC
    src/gps_nmea.c
)

target_compile_definitions(eos_gps PUBLIC EOS_ENABLE_GPS=1)

target_include_directories(eos_gps PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
)