# ---------------------------------------------------------------
# CMake Toolchain — ARM Linux hard-float cross-compilation
# Compiler: arm-linux-gnueabihf-gcc
# Targets:  Raspberry Pi 3, BeagleBone
# ---------------------------------------------------------------

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER   arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
set(CMAKE_AR           arm-linux-gnueabihf-ar)
set(CMAKE_RANLIB       arm-linux-gnueabihf-ranlib)
set(CMAKE_STRIP        arm-linux-gnueabihf-strip)
set(CMAKE_OBJCOPY      arm-linux-gnueabihf-objcopy)

set(CMAKE_C_FLAGS_INIT   "-mfloat-abi=hard -mfpu=vfpv3-d16 -ffunction-sections -fdata-sections")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections")

set(CMAKE_FIND_ROOT_PATH /usr/arm-linux-gnueabihf)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
