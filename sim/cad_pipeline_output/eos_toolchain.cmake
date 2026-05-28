# EoS Generated CMake Toolchain
# Board:  EoS Reference Board v1.0 rev 1.0
# CPU:    cortex-a57 (aarch64)
# Tool:   ebuild cad_pipeline.py
# DO NOT EDIT — regenerate from EoS Reference Board v1.0.kicad_pcb

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CROSS_COMPILE "aarch64-linux-gnu-")
set(CMAKE_C_COMPILER   ${CROSS_COMPILE}gcc)
set(CMAKE_CXX_COMPILER ${CROSS_COMPILE}g++)
set(CMAKE_ASM_COMPILER ${CROSS_COMPILE}as)
set(CMAKE_AR           ${CROSS_COMPILE}ar)
set(CMAKE_OBJCOPY      ${CROSS_COMPILE}objcopy)
set(CMAKE_OBJDUMP      ${CROSS_COMPILE}objdump)
set(CMAKE_SIZE         ${CROSS_COMPILE}size)

set(CPU_FLAGS "-march=armv8-a -mcpu=cortex-a57 -mabi=lp64")
set(CMAKE_C_FLAGS_INIT   "${CPU_FLAGS} -ffreestanding -fno-pic -nostdlib")
set(CMAKE_ASM_FLAGS_INIT "${CPU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-T ${CMAKE_CURRENT_SOURCE_DIR}/eos_generated.ld -nostdlib -static")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# EoS memory map (from EoS Reference Board v1.0.kicad_pcb)
set(EOS_FLASH_NOR_BASE  0x00000000)
set(EOS_SRAM_BOOT_BASE  0x20000000)
set(EOS_PERIPH_BASE_BASE  0x08000000)
set(EOS_UART0_PL011_BASE  0x09000000)
set(EOS_WDT_SP805_BASE  0x10000000)
set(EOS_GIC_DIST_BASE  0x08000000)
set(EOS_GIC_CPU_BASE  0x08010000)
set(EOS_RAM_LPDDR4_BASE  0x40000000)
set(EOS_HEAP_REGION_BASE  0x48000000)
set(EOS_OTA_SCRATCH_BASE  0x50000000)
set(EOS_FRAMEBUFFER_BASE  0x3c000000)
