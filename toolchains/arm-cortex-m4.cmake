# EoS ARM Cortex-M4 Cross-Compilation Toolchain File
# Usage: cmake -B build-arm -DCMAKE_TOOLCHAIN_FILE=toolchains/arm-cortex-m4.cmake

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m4)

# Toolchain prefix
set(TOOLCHAIN_PREFIX arm-none-eabi-)

# Find compiler
find_program(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
find_program(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
find_program(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}objcopy)
find_program(CMAKE_OBJDUMP ${TOOLCHAIN_PREFIX}objdump)
find_program(CMAKE_SIZE ${TOOLCHAIN_PREFIX}size)

# CPU flags
set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")
# -specs=nosys.specs belongs on the link line only, and is set in
# CMAKE_EXE_LINKER_FLAGS_INIT below. Having it here as well passed it to the
# compiler driver twice whenever an executable was linked, and a specs file
# may only be read once -- the second read tries to re-rename a spec it has
# already renamed:
#
#   arm-none-eabi-gcc: fatal error: nosys.specs: attempt to rename spec
#   'link_gcc_c_sequence' to already defined spec 'nosys_link_gcc_c_sequence'
#
# It stayed hidden because CMAKE_TRY_COMPILE_TARGET_TYPE is STATIC_LIBRARY and
# nothing else in this build links an executable, so the duplicate never
# reached a link line. arm-none-eabi-stm32f4.cmake already keeps the specs on
# the linker flags alone; this matches it.
set(CMAKE_C_FLAGS_INIT "${CPU_FLAGS} -ffunction-sections -fdata-sections -fno-common")
set(CMAKE_ASM_FLAGS_INIT "${CPU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections -specs=nosys.specs -specs=nano.specs")

# Don't try to compile test programs (bare-metal can't run them)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Defines for EoS
set(EOS_ARCH "arm" CACHE STRING "")
set(EOS_CORE "cortex-m4" CACHE STRING "")
set(EOS_MCU_STM32F4 ON CACHE BOOL "")
