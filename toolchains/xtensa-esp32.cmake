# Xtensa LX6 (ESP32) Cross-Compile Toolchain
#
# Usage (builds the ESP32 flash image; see sim/qemu_esp32/CMakeLists.txt):
#   cmake -S sim/qemu_esp32 -B build/esp32 -G Ninja \
#       -DCMAKE_TOOLCHAIN_FILE=$PWD/toolchains/xtensa-esp32.cmake \
#       -DEOS_ESP32_APP_SOURCES=sim/qemu_esp32/scheduler_test.c
#   cmake --build build/esp32
#
# The compiler is looked for on PATH first, then in $EOS_XTENSA_BIN, then in
# $EOS_XTENSA_ROOT (or Espressif's default install root). Both the
# chip-specific name `xtensa-esp32-elf-gcc` and the unified
# `xtensa-esp-elf-gcc` that newer Espressif toolchains ship are accepted; the
# chip-specific one wins when both are present, because it defaults to the
# ESP32 configuration while the unified driver needs the target spelled out.
#
# No -mabi flag is set: the port uses the windowed ABI, GCC's default for the
# ESP32, and kernel/src/arch/xtensa_lx6/port.c refuses -mabi=call0. All other
# flags come from sim/qemu_esp32/sim.yaml.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR xtensa)

# Where Espressif's installer puts toolchains. Override with -DEOS_XTENSA_ROOT
# or $EOS_XTENSA_ROOT rather than editing this file; the glob keeps the
# version out of the path so a toolchain update does not need a commit here.
if(NOT EOS_XTENSA_ROOT)
    if(DEFINED ENV{EOS_XTENSA_ROOT})
        set(EOS_XTENSA_ROOT "$ENV{EOS_XTENSA_ROOT}")
    else()
        set(EOS_XTENSA_ROOT "$ENV{HOME}/.espressif/tools/xtensa-esp-elf")
    endif()
endif()
file(GLOB _xtensa_default_bins "${EOS_XTENSA_ROOT}/*/xtensa-esp-elf/bin")
list(SORT _xtensa_default_bins)
list(REVERSE _xtensa_default_bins)   # newest version first

# PATH before the hinted directories, which is the documented order and not
# what a single find_program() with HINTS would do -- HINTS are searched
# first, so a toolchain in the default install directory would silently beat
# one the caller deliberately put on PATH.
foreach(_name xtensa-esp32-elf-gcc xtensa-esp-elf-gcc)
    if(NOT EOS_XTENSA_GCC)
        find_program(EOS_XTENSA_GCC ${_name})
    endif()
endforeach()
foreach(_name xtensa-esp32-elf-gcc xtensa-esp-elf-gcc)
    if(NOT EOS_XTENSA_GCC)
        find_program(EOS_XTENSA_GCC ${_name}
            HINTS ENV EOS_XTENSA_BIN ${_xtensa_default_bins}
            NO_DEFAULT_PATH)
    endif()
endforeach()
if(NOT EOS_XTENSA_GCC)
    message(FATAL_ERROR
        "No Xtensa ESP32 compiler found. Looked for xtensa-esp32-elf-gcc and "
        "xtensa-esp-elf-gcc on PATH, in $EOS_XTENSA_BIN, and under "
        "${EOS_XTENSA_ROOT}/*/xtensa-esp-elf/bin. Put one on PATH, or set "
        "EOS_XTENSA_BIN to its bin directory, or EOS_XTENSA_ROOT to the "
        "toolchain install root.")
endif()
get_filename_component(_xtensa_bin "${EOS_XTENSA_GCC}" DIRECTORY)

# The ESP32 is little-endian, and the two drivers Espressif ships do not
# agree on the default: xtensa-esp32-elf-gcc targets the ESP32 core and
# predefines __XTENSA_EL__, while the unified xtensa-esp-elf-gcc defaults to
# a big-endian core and predefines __XTENSA_EB__. A big-endian build links
# and produces an ELF, but esptool rejects it ("does not appear to be an
# Xtensa or an RISCV ELF file") and the image would never boot. Catch it at
# configure time with the actual compiler rather than after a full build.
execute_process(COMMAND "${EOS_XTENSA_GCC}" -dM -E -x c /dev/null
    OUTPUT_VARIABLE _xtensa_predef ERROR_QUIET RESULT_VARIABLE _xtensa_predef_rc)
if(_xtensa_predef_rc EQUAL 0 AND NOT _xtensa_predef MATCHES "__XTENSA_EL__")
    message(FATAL_ERROR
        "${EOS_XTENSA_GCC} does not target little-endian Xtensa "
        "(no __XTENSA_EL__). The ESP32 is little-endian; this compiler would "
        "produce an image esptool rejects and the chip cannot boot. Use "
        "xtensa-esp32-elf-gcc, or point EOS_XTENSA_BIN at a bin directory "
        "that contains it.")
endif()

# Match the binutils prefix to the compiler that was actually found.
if(EOS_XTENSA_GCC MATCHES "xtensa-esp32-elf-gcc[^/]*$")
    set(_xtensa_prefix "xtensa-esp32-elf")
else()
    set(_xtensa_prefix "xtensa-esp-elf")
endif()

set(CMAKE_C_COMPILER   "${EOS_XTENSA_GCC}")
set(CMAKE_ASM_COMPILER "${EOS_XTENSA_GCC}")
set(CMAKE_OBJCOPY "${_xtensa_bin}/${_xtensa_prefix}-objcopy")
set(CMAKE_SIZE    "${_xtensa_bin}/${_xtensa_prefix}-size")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_CROSSCOMPILING TRUE)
