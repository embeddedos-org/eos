# ---------------------------------------------------------------
# CMake Toolchain — RISC-V 64-bit Linux cross-compilation
# Compiler: riscv64-linux-gnu-gcc
# Targets:  SiFive, StarFive boards (Linux)
# ---------------------------------------------------------------

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

set(CMAKE_C_COMPILER   riscv64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER riscv64-linux-gnu-g++)
set(CMAKE_AR           riscv64-linux-gnu-ar)
set(CMAKE_RANLIB       riscv64-linux-gnu-ranlib)
set(CMAKE_STRIP        riscv64-linux-gnu-strip)
set(CMAKE_OBJCOPY      riscv64-linux-gnu-objcopy)

set(CMAKE_C_FLAGS_INIT   "-march=rv64gc -mabi=lp64d -ffunction-sections -fdata-sections")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections")

set(CMAKE_FIND_ROOT_PATH /usr/riscv64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
