#!/bin/bash
set -e
cd /home/spatchava/embeddedos-org/eos
CC=arm-none-eabi-gcc
OBJCOPY=arm-none-eabi-objcopy
SIZE=arm-none-eabi-size
BD=_build_qemu
mkdir -p "$BD"

CF="-mcpu=cortex-m3 -mthumb -std=c11 -Wall -Os -g -ffunction-sections -fdata-sections -fno-common -specs=nosys.specs -Ikernel/include -Iinclude -DEOS_MCU_STM32F4 -DEOS_NO_FPU"
LF="-mcpu=cortex-m3 -mthumb -Tboards/lm3s6965evb.ld -Wl,--gc-sections -specs=nosys.specs -specs=rdimon.specs -lc -lrdimon"

echo "=== QEMU Cortex-M3 Build ==="
$CC $CF -c boards/startup_stm32f407.S -o "$BD/startup.o"
echo "  startup OK"
$CC $CF -c kernel/src/task.c -o "$BD/task.o"
echo "  task OK"
$CC $CF -c kernel/src/sync.c -o "$BD/sync.o"
echo "  sync OK"
$CC $CF -c kernel/src/ipc.c -o "$BD/ipc.o"
echo "  ipc OK"
$CC $CF -c kernel/src/mem/heap.c -o "$BD/heap.o"
echo "  heap OK"
$CC $CF -c kernel/src/arch/arm_cm/nvic.c -o "$BD/nvic.o"
echo "  nvic OK"
$CC $CF -c kernel/src/arch/arm_cm/port.c -o "$BD/port.o"
echo "  port OK"
$CC $CF -c tests/main_system_test.c -o "$BD/main.o"
echo "  main OK"

echo "Linking..."
$CC $LF "$BD/startup.o" "$BD/task.o" "$BD/sync.o" "$BD/ipc.o" "$BD/heap.o" "$BD/nvic.o" "$BD/port.o" "$BD/main.o" -o "$BD/eos_qemu.elf"
echo "  link OK"

$OBJCOPY -O binary "$BD/eos_qemu.elf" "$BD/eos_qemu.bin"
$SIZE "$BD/eos_qemu.elf"
echo ""
echo "=== Running on QEMU ==="
timeout 5 qemu-system-arm -M lm3s6965evb -nographic -semihosting -kernel "$BD/eos_qemu.elf" 2>&1 || true
echo ""
echo "=== Done ==="
