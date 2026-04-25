# Chapter 33: Cross-Compilation for EoS

*Author: Srikanth Patchava & EmbeddedOS Contributors*

---

## 33.1 Introduction

EoS supports cross-compilation for multiple architectures from a single
development host. This chapter covers the toolchain infrastructure, CMake
integration, and practical workflows for targeting ARM, RISC-V, and other
architectures.

---

## 33.2 Supported Toolchains

| Toolchain File                       | Architecture   | Targets                   |
|--------------------------------------|----------------|---------------------------|
| `arm-none-eabi-stm32f4.cmake`        | ARM Cortex-M4  | STM32F4 Discovery         |
| `arm-none-eabi-r5.cmake`             | ARM Cortex-R5  | Safety MCUs (TMS570)      |
| `arm-linux-gnueabihf.cmake`          | ARM Cortex-A   | RPi (32-bit), BeagleBone  |
| `aarch64-linux-gnu.cmake`            | AArch64        | RPi 4/5, Jetson, i.MX8   |
| `riscv64-linux-gnu.cmake`            | RISC-V 64      | SiFive, StarFive          |

---

## 33.3 Toolchain File Anatomy

A CMake toolchain file sets compiler paths, CPU flags, and sysroot
locations. Here is the STM32F4 toolchain:

```cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m4)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Cortex-M4 with hardware FPU
set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")

set(CMAKE_C_FLAGS_INIT
    "${CPU_FLAGS} -fdata-sections -ffunction-sections -fno-common -fstack-protector-strong"
)

set(CMAKE_EXE_LINKER_FLAGS_INIT
    "${CPU_FLAGS} -Wl,--gc-sections -specs=nano.specs -specs=nosys.specs -lnosys"
)

set(CMAKE_CROSSCOMPILING TRUE)
set(EOS_PLATFORM "rtos" CACHE STRING "Target platform" FORCE)
```

### Key Elements

| Setting                      | Purpose                                    |
|------------------------------|--------------------------------------------|
| `CMAKE_SYSTEM_NAME Generic`  | Indicates bare-metal (no OS)               |
| `CPU_FLAGS`                  | Architecture-specific compiler flags       |
| `-ffunction-sections`        | Enable linker garbage collection           |
| `-specs=nano.specs`          | Use newlib-nano for smaller footprint      |
| `-fstack-protector-strong`   | Stack buffer overflow detection            |
| `EOS_PLATFORM "rtos"`        | Selects EoS RTOS configuration             |

---

## 33.4 AArch64 Linux Toolchain

For application processors running Linux:

```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

set(CMAKE_C_FLAGS_INIT   "-ffunction-sections -fdata-sections")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections")

set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

Key difference from bare-metal: `CMAKE_SYSTEM_NAME` is `Linux` and
the sysroot is set to the cross-compilation environment.

---

## 33.5 RISC-V Toolchain

```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

set(CMAKE_C_COMPILER   riscv64-linux-gnu-gcc)
set(CMAKE_C_FLAGS_INIT "-march=rv64gc -mabi=lp64d -ffunction-sections -fdata-sections")
```

The `-march=rv64gc` flag enables the general-purpose RISC-V ISA with
compressed instructions. The `-mabi=lp64d` flag uses the LP64 ABI with
hardware double-precision float.

---

## 33.6 Cross-Compilation Workflow

### Step 1: Install the Toolchain

```bash
# ARM bare-metal
sudo apt install gcc-arm-none-eabi

# AArch64 Linux
sudo apt install gcc-aarch64-linux-gnu

# RISC-V
sudo apt install gcc-riscv64-linux-gnu
```

### Step 2: Configure with Toolchain File

```bash
cmake -B build-stm32 \
  -DCMAKE_TOOLCHAIN_FILE=toolchains/arm-none-eabi-stm32f4.cmake \
  -DEOS_PRODUCT=iot
```

### Step 3: Build

```bash
cmake --build build-stm32
```

### Step 4: Flash

```bash
# Using OpenOCD
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
  -c "program build-stm32/eos_firmware.bin 0x08000000 verify reset exit"

# Using ebuild
ebuild flash --board stm32f4 --image build-stm32/eos_firmware.bin
```

---

## 33.7 Multi-Architecture CI

The EoS CI pipeline cross-compiles for all supported architectures:

```yaml
strategy:
  matrix:
    include:
      - arch: arm-cortex-m4
        toolchain: arm-none-eabi-stm32f4.cmake
        product: iot
      - arch: aarch64
        toolchain: aarch64-linux-gnu.cmake
        product: gateway
      - arch: riscv64
        toolchain: riscv64-linux-gnu.cmake
        product: robot
```

---

## 33.8 Linker Scripts

Each target requires a linker script defining the memory layout:

```ld
/* STM32F407 Memory Layout */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 192K
    CCM   (rwx) : ORIGIN = 0x10000000, LENGTH = 64K
}

SECTIONS
{
    .text : {
        KEEP(*(.isr_vector))
        *(.text*)
        *(.rodata*)
    } > FLASH

    .data : {
        _sdata = .;
        *(.data*)
        _edata = .;
    } > SRAM AT > FLASH

    .bss : {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        _ebss = .;
    } > SRAM

    _estack = ORIGIN(SRAM) + LENGTH(SRAM);
}
```

---

## 33.9 Binary Size Optimization

| Technique                   | Savings    | Flag/Setting                    |
|-----------------------------|------------|---------------------------------|
| `-Os` optimization          | 15-30%     | `CMAKE_C_FLAGS_RELEASE "-Os"`   |
| Section garbage collection  | 10-20%     | `-ffunction-sections` + `-Wl,--gc-sections` |
| Newlib-nano                 | 50-80 KB   | `-specs=nano.specs`             |
| Link-time optimization      | 5-15%     | `-flto`                         |
| Strip debug symbols         | 20-50%    | `arm-none-eabi-strip`           |

---

## 33.10 Debugging Cross-Compiled Firmware

```bash
# Start GDB server
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg

# Connect GDB
arm-none-eabi-gdb build-stm32/eos_firmware.elf
(gdb) target remote :3333
(gdb) monitor reset halt
(gdb) load
(gdb) break main
(gdb) continue
```

---

## 33.11 Summary

EoS provides a clean, CMake-based cross-compilation infrastructure that
supports bare-metal MCUs, Linux application processors, and RISC-V targets.

**Key takeaways:**

- CMake toolchain files for ARM Cortex-M/A/R, AArch64, and RISC-V
- Bare-metal and Linux cross-compilation from a single build system
- Linker scripts define memory layout per target
- Binary size optimization techniques for constrained devices
- Multi-architecture CI ensures all targets compile cleanly

---

*Next: Chapter 34 — Testing and Quality*
