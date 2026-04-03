# ARM Cortex-M4 Cross-Compile Toolchain for STM32F4
#
# Usage:
#   cmake -B build-stm32 \
#       -DCMAKE_TOOLCHAIN_FILE=toolchains/arm-none-eabi-stm32f4.cmake \
#       -DEOS_BOARD=stm32f407_discovery
#   cmake --build build-stm32

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m4)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP arm-none-eabi-objdump)
set(CMAKE_SIZE arm-none-eabi-size)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Cortex-M4 with hardware FPU
set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")

set(CMAKE_C_FLAGS_INIT
    "${CPU_FLAGS} -fdata-sections -ffunction-sections -fno-common -fstack-protector-strong"
)
set(CMAKE_ASM_FLAGS_INIT "${CPU_FLAGS}")

set(CMAKE_C_FLAGS_DEBUG "-Og -g3 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-Os -g -DNDEBUG")

set(CMAKE_EXE_LINKER_FLAGS_INIT
    "${CPU_FLAGS} -Wl,--gc-sections -specs=nano.specs -specs=nosys.specs -lnosys"
)

# Mark as cross-compiling
set(CMAKE_CROSSCOMPILING TRUE)
set(EOS_PLATFORM "rtos" CACHE STRING "Target platform" FORCE)
