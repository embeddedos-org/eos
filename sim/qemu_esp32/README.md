# EoS on the ESP32 (Xtensa LX6): QEMU and real boards

Runs the EoS kernel on the ESP32's PRO CPU, from the same image in Espressif's
QEMU `esp32` machine and on an ESP-WROOM-32 DevKit board. The boot ROM loads
the image straight from flash: no ESP-IDF, no second-stage bootloader. The
`QEMU ESP32 Boot Test` CI job runs the three tests below.

```sh
# Toolchain and QEMU: Espressif's releases (ESP-IDF's install.sh fetches them,
# or see the CI job for direct, checksum-pinned downloads).
pip install esptool pyyaml

EOS_EXTRA_CFLAGS=-DEOS_TEST_PAD_READBACK=0 \
  sim/qemu_esp32/build.sh /tmp/sched.elf sim/qemu_esp32/scheduler_test.c
sim/qemu_esp32/run.sh /tmp/sched.flash.bin
# scheduler: worker_a=40 worker_b=20 led_toggles=8 led_readback_errors=4 ticks=400 cycles_per_400ms=16001230
# SCHEDULER TEST PASSED

sim/qemu_esp32/build.sh /tmp/ctx.elf \
  sim/qemu_esp32/context_test.c sim/qemu_esp32/context_regcheck.S
sim/qemu_esp32/run.sh /tmp/ctx.flash.bin 30 "CONTEXT TEST PASSED"

sim/qemu_esp32/build.sh /tmp/fault.elf sim/qemu_esp32/fault_test.c
sim/qemu_esp32/run.sh /tmp/fault.flash.bin 30 "EOS FATAL code=7"
```

On a board (the default build enforces the LED readback):

```sh
sim/qemu_esp32/build.sh /tmp/sched.elf sim/qemu_esp32/scheduler_test.c
esptool --chip esp32 --port /dev/ttyUSB0 write-flash 0x1000 /tmp/sched.bin
sim/qemu_esp32/capture_serial.py /dev/ttyUSB0 12
```

Writing at 0x1000 replaces the board's second-stage bootloader. Read the whole
flash first (`esptool read-flash 0 0x400000 backup.bin`) and write it back at 0
to restore the board.

| File | Purpose |
|---|---|
| `sim.yaml` | Compiler/linker flags, sources, image and QEMU settings |
| `build.sh` | Builds `OUT.elf`, `OUT.bin` (flash at 0x1000) and `OUT.flash.bin` (4 MB, for QEMU) |
| `run.sh` | Boots a flash image in QEMU and checks for the expected output |
| `capture_serial.py` | Resets a board, records its UART0 output with host timestamps, checks the verdict and heartbeat spacing |
| `scheduler_test.c` | Starts the kernel; workers progress only through the tick and the context switch; blinks the LED through the HAL and reads the pad back. Its tasks block almost at once, so it does not show that a preempted task keeps its registers |
| `context_test.c`, `context_regcheck.S` | Three tasks that never block check a0..a15, SAR and a zero-overhead loop while the tick preempts them; any register the frame loses, or one restored from another task, fails |
| `fault_test.c` | A task returns; the port must report `EOS FATAL code=7` (`task_returned`, not any other fault) and run nothing after it |

Mutation check: with the SAR restore in `vectors.S` replaced by `nop`, or
LCOUNT restored as 0, `context_test` fails (error 2, error 1) where
`scheduler_test` still passed; with a task's return address set to 0,
`fault_test` fails (the fault is `code=2 exccause=20`, not `code=7`).

## The port

| Piece | Where |
|---|---|
| Vectors, level-1 handler, context restore | `kernel/src/arch/xtensa_lx6/vectors.S` |
| Stack frames, tick, dispatch, critical sections, IRQ table | `kernel/src/arch/xtensa_lx6/port.c` |
| Reset entry, linker script, watchdogs, console, fatal report | `boards/esp32/` |
| GPIO and UART0 HAL backend (`-DEOS_HAL_BACKEND_ESP32`) | `hal/src/hal_esp32/hal_esp32_backend.c` |

Design choices, and why:

- **CALL0 ABI (`-mabi=call0`).** With no register windows, a context is
  a0..a15 plus SAR and the loop registers, so the handler saves one 96-byte
  frame and never spills windows. The cost: windowed code cannot be linked
  in. That rules out the toolchain's newlib and libgcc (hence `-nostdlib` and
  `boards/esp32/freestanding.c`; the kernel needs only `memset` and `memcpy`)
  and Espressif's prebuilt Wi-Fi/BLE libraries.
- **Tick = CCOMPARE0 (CPU line 6), yield = software interrupt (line 7).** Both
  are level 1; they play SysTick's and PendSV's parts, and `eos_schedule()`
  is shared with the Cortex-M port unchanged.
- **Separate interrupt stack.** The frame goes on the task's stack; the C
  dispatcher runs on its own 2 KB stack, so a task only needs 96 bytes of
  headroom (the idle task has 256).
- **The dispatcher runs with `PS.EXCM` clear** (INTLEVEL 3 keeps level-1
  interrupts masked). With EXCM set the core does not loop back at `LEND`,
  so every zero-overhead `loop` GCC emits would run once. That was the first
  bug QEMU showed: `find_next_task()` scanned one slot and never switched.
- **FPU off (`CPENABLE = 0`).** The port does not save coprocessor state, so
  floating-point use faults instead of corrupting another task.
- **Faults stop the system.** Every vector the port does not expect reports
  `EOS FATAL code=… exccause=… epc1=…` on UART0 and halts with interrupts
  masked.

## Verified on hardware

HiLetGo ESP-WROOM-32 (ESP32-D0WD-V3 rev 3.1, 4 MB flash), 2026-11-23:

`scheduler_test.c` (the default, readback-enforcing build), recorded by
`capture_serial.py`:

```
[  0.024s] rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
[  0.024s] load:0x3fff0000,len:588
[  0.074s] load:0x40080400,len:5072
[  0.074s] entry 0x40080970
[  0.074s] EoS on ESP32 (Xtensa LX6): booting
[  0.074s] scheduler: starting kernel
[  0.475s] scheduler: worker_a=40 worker_b=20 led_toggles=8 led_readback_errors=0 ticks=400 cycles_per_400ms=16000876
[  0.476s] SCHEDULER TEST PASSED
[ 11.468s] heartbeat tick=11400 led_toggles=228 led_readback_errors=0
verdict: PASS; heartbeats: 11
heartbeat spacing: mean 1.0039s min 1.0037s max 1.0044s over 10 gaps
```

`context_test.c` on the same board:

```
context: task0 rounds=252 error=0
context: task1 rounds=252 error=0
context: task2 rounds=251 error=0
CONTEXT TEST PASSED
```

## Limits

- One core. The APP CPU stays in reset.
- A preempted task carries a 96-byte frame on its stack, against 68 bytes on
  Cortex-M. `EOS_MIN_STACK_SIZE` (128) still admits a task, but leaves it only
  32 bytes of its own; the tests use 512.
- No Wi-Fi or Bluetooth (they need ESP-IDF's windowed binary libraries).
- No watchdog: the two the ROM arms are switched off at boot, and nothing
  re-arms one. A hung task stays hung.
- The CPU stays on the 40 MHz crystal the ROM leaves it on. On the board above
  1000 ticks took about 1.004 s of host time (0.4% slow) in every run; the
  cause (crystal tolerance or otherwise) was not investigated.
- Only level-1 CPU interrupt lines can carry handlers
  (`eos_arch_irq_register()`); routing a peripheral to one through the
  interrupt matrix is not implemented. `eos_arch_irq_set_priority()` does
  nothing, because Xtensa fixes each line's level in hardware.
- The HAL backend implements GPIO and UART0 only; UART0 stays at the 115200
  8N1 the ROM set. Other HAL calls return -1. It refuses GPIO1/3 (the
  console), GPIO6-11 (the module's flash), outputs on the input-only GPIO34-39,
  and pulls on GPIO34-39, which have none.
- QEMU's GPIO model does not reflect outputs in `GPIO_IN`, hence
  `-DEOS_TEST_PAD_READBACK=0` there.
