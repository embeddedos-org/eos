cmake_minimum_required(VERSION 3.16)
project(EoS
    VERSION 0.1.0
    DESCRIPTION "EoS — Multi-platform embedded OS framework with runtime services"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W3)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra)
endif()

option(EOS_BUILD_TESTS "Build unit tests" OFF)
option(EOS_ENABLE_ASAN "Enable AddressSanitizer + UBSan" OFF)
option(EOS_ENABLE_CPPCHECK "Enable cppcheck static analysis target" OFF)
set(EOS_PLATFORM "linux" CACHE STRING "Target platform (linux|rtos)")
set(EOS_BOARD "" CACHE STRING "Target board (e.g. stm32f407_discovery)")

# AddressSanitizer / UndefinedBehaviorSanitizer
if(EOS_ENABLE_ASAN AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# Board-specific CMake fragment
if(EOS_BOARD AND EXISTS "${CMAKE_SOURCE_DIR}/boards/${EOS_BOARD}/board.cmake")
    include("${CMAKE_SOURCE_DIR}/boards/${EOS_BOARD}/board.cmake")
    message(STATUS "  Board: ${EOS_BOARD}")
endif()

# Static analysis
include(cmake/cppcheck.cmake)
set(EOS_PRODUCT "" CACHE STRING "Target product profile (robot|vacuum|industrial|voice|aerospace|satellite|mobile|watch|adapter|gateway|medical|automotive|drone|iot|hmi|wearable|pos|printer|cockpit|banking|crypto_hw|telecom|diagnostic|telemedicine|ground_control|space_comm|plc|autonomous|infotainment|thermostat|security_cam|smart_home|xr_headset|computer|smart_tv|gaming|server|ai_edge|router|ev|fitness|iptv_stb|cast_device|smart_speaker|home_camera|tv_os|vbox_test)")

# ====================================================================
# Dependency Resolution — auto-fetch eboot + ebuild
# ====================================================================
# This MUST come before any target definitions so that eboot targets
# (eboot_hal, eboot_core, eboot_stage1) are available for linking.
include(cmake/deps.cmake)

# ====================================================================
# Product Configuration
# ====================================================================
if(EOS_PRODUCT)
    string(TOUPPER "${EOS_PRODUCT}" EOS_PRODUCT_UPPER)
    add_compile_definitions(EOS_PRODUCT_${EOS_PRODUCT_UPPER})
    message(STATUS "  Product: ${EOS_PRODUCT}")
else()
    message(STATUS "  Product: all (no product selected — full build)")
endif()

# Global include for eos_config.h and product profiles
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ====================================================================
# HAL — Hardware Abstraction Layer
# ====================================================================
add_library(eos_hal STATIC
    hal/src/hal_common.c
    hal/src/hal_extended_stubs.c
)

if(EOS_PLATFORM STREQUAL "linux" OR NOT CMAKE_CROSSCOMPILING)
    target_sources(eos_hal PRIVATE hal/src/hal_linux.c)
else()
    target_sources(eos_hal PRIVATE hal/src/hal_rtos.c)
endif()

target_include_directories(eos_hal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/hal/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link eos_hal to eboot_hal when available (shared HAL interface)
if(EOS_HAS_EBOOT AND TARGET eboot_hal)
    target_link_libraries(eos_hal PUBLIC eboot_hal)
endif()

# ====================================================================
# Kernel — Lightweight RTOS kernel + Multicore
# ====================================================================
add_library(eos_kernel STATIC
    kernel/src/task.c
    kernel/src/sync.c
    kernel/src/ipc.c
    kernel/src/multicore.c
)

target_include_directories(eos_kernel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(eos_kernel PUBLIC eos_hal)

# ====================================================================
# Driver Framework
# ====================================================================
add_library(eos_drivers STATIC
    drivers/src/driver_framework.c
)

target_include_directories(eos_drivers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/include
)

target_link_libraries(eos_drivers PUBLIC eos_hal)

# ====================================================================
# Core — OS config, logging, layers
# ====================================================================
add_subdirectory(core)

# ====================================================================
# Toolchains
# ====================================================================
add_subdirectory(toolchains)

# ====================================================================
# Systems — rootfs, image, firmware assembly
# ====================================================================
add_subdirectory(systems)

# ====================================================================
# Services — crypto, security, os, linux, rtos, sensor, motor, ota, fs
# ====================================================================
add_subdirectory(services)

# ====================================================================
# Debug - GDB stub, core dump handler
# ====================================================================
add_subdirectory(debug)

# ====================================================================
# Device Tree Parser
# ====================================================================
add_subdirectory(drivers/devicetree)

# ====================================================================
# Service Manager - daemon lifecycle
# ====================================================================
add_subdirectory(services/init)

# ====================================================================
# Power Management Framework
# ====================================================================
add_library(eos_power STATIC
    power/src/power.c
)

target_include_directories(eos_power PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/power/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(eos_power PUBLIC eos_hal)

# ====================================================================
# Networking Abstraction
# ====================================================================
add_library(eos_net STATIC
    net/src/net.c
)

target_include_directories(eos_net PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(eos_net PUBLIC eos_hal)

# ====================================================================
# eBoot Integration — link eboot_core into eos services
# ====================================================================
if(EOS_HAS_EBOOT AND TARGET eboot_core)
    # Make eboot headers available to eos_systems (for firmware build)
    if(TARGET eos_systems)
        target_link_libraries(eos_systems PRIVATE eboot_core)
    endif()

    # Convenience alias: eos::eboot for downstream consumers
    if(NOT TARGET eos::eboot)
        add_library(eos::eboot ALIAS eboot_core)
    endif()

    message(STATUS "  eBoot targets available: eboot_hal, eboot_core, eboot_stage1")
endif()

# ====================================================================
# ebuild Layers — optional components (EAI, ENI, EIPC, eOSuite)
# ====================================================================
include(cmake/ebuild_layers.cmake)

# ====================================================================
# Tests
# ====================================================================
if(EOS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ====================================================================
# Documentation
# ====================================================================
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
    message(STATUS "  Docs:  ON  (doxygen found)")
else()
    message(STATUS "  Docs:  OFF (doxygen not found)")
endif()

# ====================================================================
# Build Summary
# ====================================================================
message(STATUS "--------------------------------------------")
message(STATUS "EoS v${PROJECT_VERSION} — Embedded OS Framework")
message(STATUS "  Platform: ${EOS_PLATFORM}")
message(STATUS "  Tests:    ${EOS_BUILD_TESTS}")
if(EOS_HAS_EBOOT)
    message(STATUS "  eBoot:    INTEGRATED")
endif()
if(EOS_HAS_EBUILD)
    message(STATUS "  ebuild:   INTEGRATED (layers available)")
endif()
message(STATUS "--------------------------------------------")
message(STATUS "")
message(STATUS "Quick start:")
message(STATUS "  cmake --build build                     # Build everything")
message(STATUS "  cmake -B build -DEOS_BUILD_TESTS=ON     # With tests")
message(STATUS "  cmake -B build -DEOS_WITH_EBOOT=OFF     # Without bootloader")
message(STATUS "  cmake -B build -DEOS_WITH_EBUILD=ON     # With ebuild layers")
message(STATUS "  cmake -B build -DEOS_WITH_ALL_LAYERS=ON # All layers (EAI+ENI+EIPC+eOSuite)")
message(STATUS "")
