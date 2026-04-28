#!/bin/bash
# EoS ARM Cortex-M4 cross-compilation build script
# Produces: eos_system_test.elf and eos_system_test.bin
set -e

CC=arm-none-eabi-gcc
OBJCOPY=arm-none-eabi-objcopy
SIZE=arm-none-eabi-size

EOS_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${EOS_ROOT}/_build_arm"

echo "═══ EoS ARM Cortex-M4 Build ═══"
echo "Root: ${EOS_ROOT}"
echo "Output: ${BUILD_DIR}"

mkdir -p "${BUILD_DIR}"

# CPU flags
CFLAGS="-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16"
CFLAGS="${CFLAGS} -std=c11 -Wall -Os -g"
CFLAGS="${CFLAGS} -ffunction-sections -fdata-sections -fno-common"
CFLAGS="${CFLAGS} -DEOS_MCU_STM32F4 -D__ARM_ARCH=7"
CFLAGS="${CFLAGS} -I${EOS_ROOT}/kernel/include -I${EOS_ROOT}/include"
CFLAGS="${CFLAGS} -specs=nosys.specs"

LDFLAGS="-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16"
LDFLAGS="${LDFLAGS} -T${EOS_ROOT}/boards/stm32f407vg.ld"
LDFLAGS="${LDFLAGS} -Wl,--gc-sections -specs=nosys.specs -specs=nano.specs"
LDFLAGS="${LDFLAGS} -lc -lnosys"

# Source files
SRCS=(
    "${EOS_ROOT}/boards/startup_stm32f407.S"
    "${EOS_ROOT}/kernel/src/task.c"
    "${EOS_ROOT}/kernel/src/sync.c"
    "${EOS_ROOT}/kernel/src/ipc.c"
    "${EOS_ROOT}/kernel/src/mem/heap.c"
    "${EOS_ROOT}/kernel/src/arch/arm_cm/context_switch.S"
    "${EOS_ROOT}/kernel/src/arch/arm_cm/port.c"
    "${EOS_ROOT}/kernel/src/arch/arm_cm/systick.c"
    "${EOS_ROOT}/kernel/src/arch/arm_cm/nvic.c"
    "${EOS_ROOT}/tests/main_system_test.c"
)

echo ""
echo "Compiling ${#SRCS[@]} source files..."

OBJS=()
for src in "${SRCS[@]}"; do
    name=$(basename "${src}" | sed 's/\.[cS]$/.o/')
    obj="${BUILD_DIR}/${name}"
    echo "  CC  $(basename ${src})"
    ${CC} ${CFLAGS} -c "${src}" -o "${obj}" 2>&1
    OBJS+=("${obj}")
done

echo ""
echo "Linking..."
${CC} ${LDFLAGS} "${OBJS[@]}" -o "${BUILD_DIR}/eos_system_test.elf"
echo "  LD  eos_system_test.elf"

echo ""
echo "Generating binary..."
${OBJCOPY} -O binary "${BUILD_DIR}/eos_system_test.elf" "${BUILD_DIR}/eos_system_test.bin"
echo "  BIN eos_system_test.bin"

echo ""
echo "Size:"
${SIZE} "${BUILD_DIR}/eos_system_test.elf"

echo ""
BIN_SIZE=$(stat -c%s "${BUILD_DIR}/eos_system_test.bin" 2>/dev/null || echo "?")
echo "Binary: ${BUILD_DIR}/eos_system_test.bin (${BIN_SIZE} bytes)"
echo "ELF:    ${BUILD_DIR}/eos_system_test.elf"
echo ""
echo "═══ Build complete ═══"
echo ""
echo "To flash: openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c 'program ${BUILD_DIR}/eos_system_test.elf verify reset exit'"
echo "To QEMU:  qemu-system-arm -M lm3s6965evb -nographic -semihosting -kernel ${BUILD_DIR}/eos_system_test.elf"
