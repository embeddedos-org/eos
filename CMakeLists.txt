cmake_minimum_required(VERSION 3.16)
project(EoS
    VERSION 0.5.0
    DESCRIPTION "EoS — Multi-platform embedded OS framework with runtime services"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W3)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra)
endif()

option(EOS_BUILD_TESTS "Build unit tests" OFF)
set(EOS_PLATFORM "linux" CACHE STRING "Target platform (linux|rtos)")
set(EOS_PRODUCT "" CACHE STRING "Target product profile (robot|vacuum|industrial|voice|aerospace|satellite|mobile|watch|adapter|gateway|medical|automotive|drone|iot|hmi|wearable|pos|printer|cockpit|banking|crypto_hw|telecom|diagnostic|telemedicine|ground_control|space_comm|plc|autonomous|infotainment|thermostat|security_cam|smart_home|xr_headset|computer|smart_tv|gaming|server|ai_edge|router|ev|fitness)")

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

# ====================================================================
# Test feature flags
# ====================================================================
# Module sources are wrapped in #if EOS_ENABLE_<MODULE>, and
# include/eos/eos_config.h defaults every one of those to 0 unless a product
# profile sets it. Without a product, those translation units compile to
# nothing and the unit tests that exercise them fail to link.
#
# A test defining the flag for itself is not enough — the flag has to reach
# the *library* translation unit too, so it is set here, before any
# add_library() call, rather than in tests/CMakeLists.txt.
#
# This list is exactly the set the suite in tests/ covers. It does not turn on
# every EOS_ENABLE_* flag: the others have no test and enabling them would
# build code this gate cannot verify.
if(EOS_BUILD_TESTS AND NOT EOS_PRODUCT)
    add_compile_definitions(
        EOS_ENABLE_MULTICORE=1
        EOS_ENABLE_NET=1
        EOS_ENABLE_POWER=1
        EOS_ENABLE_OTA=1
        EOS_ENABLE_SENSOR=1
        EOS_ENABLE_FILESYSTEM=1
        EOS_ENABLE_MOTOR_CTRL=1
    )
    message(STATUS "  Test features: multicore net power ota sensor filesystem motor_ctrl")
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

# Which backend source is compiled, and which one eos_hal_init() falls back to,
# have to be the same decision. hal_common.c used to make it independently with
# `#ifdef __linux__`, which disagrees with this condition on any non-Linux host
# build -- macOS compiles hal_linux.c here and then referenced
# eos_hal_rtos_register(), which is not in the library:
#
#   Undefined symbols: "_eos_hal_rtos_register", referenced from _eos_hal_init
#
# The build system knows which file it compiled, so it says so.
if((EOS_PLATFORM STREQUAL "linux" OR NOT CMAKE_CROSSCOMPILING) AND UNIX)
    target_sources(eos_hal PRIVATE hal/src/hal_linux.c)
    target_compile_definitions(eos_hal PRIVATE EOS_HAL_BACKEND_HOST)
elseif(WIN32)
    target_sources(eos_hal PRIVATE hal/src/hal_win32.c)
    target_compile_definitions(eos_hal PRIVATE EOS_HAL_BACKEND_WIN32)
else()
    target_sources(eos_hal PRIVATE hal/src/hal_rtos.c)
    target_compile_definitions(eos_hal PRIVATE EOS_HAL_BACKEND_RTOS)
endif()

target_include_directories(eos_hal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/hal/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ====================================================================
# Kernel — Lightweight RTOS kernel + Multicore
# ====================================================================
add_library(eos_kernel STATIC
    kernel/src/task.c
    kernel/src/sync.c
    kernel/src/ipc.c
    kernel/src/multicore.c
    kernel/src/mem/heap.c
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
# services/pkg is the host-side .eapp package manager: it lists directories,
# spawns processes and waits on them, so it needs <dirent.h>, <unistd.h> and
# <sys/wait.h>. Bare-metal newlib has none of those -- <dirent.h> is a hard
# #error there -- so it is host-only, gated the same way net_posix.c is.
if(NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(services/pkg)
endif()
add_subdirectory(pkg)


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

# ADR-014 point 3 — on a host OS the eNet API maps onto POSIX sockets.
#
# Gated on CMAKE_CROSSCOMPILING alone, deliberately not on the
# `EOS_PLATFORM STREQUAL "linux" OR ...` form used above for hal_linux.c:
# EOS_PLATFORM defaults to "linux" and a cross toolchain file does not change
# it, so that condition is true even for a Cortex-M build. hal_linux.c happens
# to survive that because it uses only portable headers; net_posix.c needs
# <netdb.h> and <sys/socket.h>, which bare-metal newlib does not have.
if(NOT CMAKE_CROSSCOMPILING AND UNIX)
    target_sources(eos_net PRIVATE net/src/net_posix.c)
    target_compile_definitions(eos_net PRIVATE EOS_NET_POSIX=1)
endif()

target_include_directories(eos_net PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(eos_net PUBLIC eos_hal)

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

message(STATUS "--------------------------------------------")
message(STATUS "EoS v${PROJECT_VERSION} — Embedded OS Framework")
message(STATUS "  Platform: ${EOS_PLATFORM}")
message(STATUS "  Tests:    ${EOS_BUILD_TESTS}")
message(STATUS "--------------------------------------------")
