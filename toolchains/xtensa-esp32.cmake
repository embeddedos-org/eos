# Xtensa LX6 (ESP32) Cross-Compile Toolchain
#
# Usage (builds the ESP32 flash image; see sim/qemu_esp32/CMakeLists.txt):
#   cmake -S sim/qemu_esp32 -B build/esp32 -G Ninja \
#       -DCMAKE_TOOLCHAIN_FILE=$PWD/toolchains/xtensa-esp32.cmake \
#       -DEOS_ESP32_APP_SOURCES=sim/qemu_esp32/scheduler_test.c
#   cmake --build build/esp32
#
# The compiler is found on PATH, in $EOS_XTENSA_BIN, or in Espressif's
# default install directory. No -mabi flag is set: the port uses the windowed
# ABI, GCC's default for the ESP32, and kernel/src/arch/xtensa_lx6/port.c
# refuses -mabi=call0. All other flags come from sim/qemu_esp32/sim.yaml.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR xtensa)

find_program(EOS_XTENSA_GCC xtensa-esp32-elf-gcc
    HINTS ENV EOS_XTENSA_BIN
          "$ENV{HOME}/.espressif/tools/xtensa-esp-elf/esp-14.2.0_20241119/xtensa-esp-elf/bin")
if(NOT EOS_XTENSA_GCC)
    message(FATAL_ERROR "xtensa-esp32-elf-gcc not found: put it on PATH or set EOS_XTENSA_BIN")
endif()
get_filename_component(_xtensa_bin "${EOS_XTENSA_GCC}" DIRECTORY)

set(CMAKE_C_COMPILER   "${EOS_XTENSA_GCC}")
set(CMAKE_ASM_COMPILER "${EOS_XTENSA_GCC}")
set(CMAKE_OBJCOPY "${_xtensa_bin}/xtensa-esp32-elf-objcopy")
set(CMAKE_SIZE    "${_xtensa_bin}/xtensa-esp32-elf-size")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_CROSSCOMPILING TRUE)
