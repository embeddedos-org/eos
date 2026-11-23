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
| `context_test.c`, `context_regcheck.S` | Three tasks that never block check a0..a15, SAR and a zero-overhead loop, and run 24-deep recursions through CALL4, CALL8 and CALL12 (longer than the 16-window register file), while the tick preempts them; any register the frame or a window spill loses, one restored from another task, or a task starved of its share of the CPU fails |
| `periph_test.c` | ADC (with its eFuse calibration), DAC, PWM and UART1/2 through loopbacks inside the chip (board only; see below) |
| `fault_test.c` | A task returns; the port must report `EOS FATAL code=7` (`task_returned`, not any other fault) and run nothing after it |

Mutation checks (each run in QEMU): with no window spill in the interrupt
handler, or the 16-byte base save area gap removed, `context_test` fails
(a fatal exception; tasks starved to 8-12 rounds against about 7000); a
wrong register in the CALL4, CALL8 or CALL12 overflow or underflow handler
fails it with error 5, a fatal exception, or error 6. Under the earlier CALL0
port, a missing SAR or LCOUNT restore failed it where `scheduler_test`
still passed; with a task's return address set to 0, `fault_test` fails
(the fault is `code=2 exccause=20`, not `code=7`).

## The port

| Piece | Where |
|---|---|
| Vectors, level-1 handler, context restore | `kernel/src/arch/xtensa_lx6/vectors.S` |
| Stack frames, tick, dispatch, critical sections, IRQ table | `kernel/src/arch/xtensa_lx6/port.c` |
| Reset entry, linker script, watchdogs, console, fatal report | `boards/esp32/` |
| GPIO and UART0 HAL backend (`-DEOS_HAL_BACKEND_ESP32`) | `hal/src/hal_esp32/hal_esp32_backend.c` |

Design choices, and why:

- **Windowed ABI**, as the boot ROM, the toolchain's newlib and libgcc and
  Espressif's binary libraries use (the port was first built with CALL0,
  which ruled all of those out). The level-1 handler saves the interrupted
  function's window (a0..a15) in a 96-byte frame and spills every older
  window to the save areas the ABI reserves on the task's own stack; the
  task's callers come back through underflow exceptions as it returns into
  them. The frame sits 16 bytes below the task's stack pointer, leaving the
  caller's base save area free. The six window overflow/underflow handlers
  are the ISA's standard sequences; an alloca exception (MOVSP) is handled by
  the same spill.
- **Tick = CCOMPARE0 (CPU line 6), yield = software interrupt (line 7).** Both
  are level 1; they play SysTick's and PendSV's parts, and `eos_schedule()`
  is shared with the Cortex-M port unchanged.
- **Separate interrupt stack.** The frame goes on the task's stack; the C
  dispatcher runs on its own 2 KB stack, so a task only needs 112 bytes of
  headroom beyond its own frames (the idle task has 256).
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

`context_test.c` on the same board (windowed port, with the deep-recursion checks):

```
context: task0 rounds=240 error=0
context: task1 rounds=240 error=0
context: task2 rounds=240 error=0
CONTEXT TEST PASSED
```

## Peripheral drivers

`hal/src/hal_esp32/` implements the extended HAL's ADC, DAC and PWM
(`-DEOS_ENABLE_ADC/DAC/PWM=1`) and UART1/2 in the backend. The portable API
has no pin or unit arguments, so the ESP32 numbering is:

| API | ESP32 |
|---|---|
| ADC channel 0-7 | ADC1 channel 0-7: GPIO36, 37, 38, 39, 32, 33, 34, 35 |
| ADC channel 10-19 | ADC2 channel 0-9: GPIO4, 0, 2, 15, 13, 12, 14, 27, 25, 26 |
| DAC channel 0, 1 | GPIO25, GPIO26, 8-bit |
| PWM channel 0-3 | LEDC high-speed channel n on its own timer n, on `EOS_BOARD_PWM_GPIOS[n]` (default GPIO2, 4, 5, 13) |
| UART 1 / 2 | GPIO19 TX, 18 RX / GPIO17 TX, 16 RX (`EOS_BOARD_UART*_GPIO`) |

- ADC: 9-12 bits, 12 dB attenuation, one software-started conversion per
  read. `eos_adc_read_mv()` uses the chip's eFuse calibration
  (`hal_esp32_adc_cal.c`, ported from ESP-IDF's line fitting, Apache-2.0):
  two-point values if burned, else the measured Vref, else 1100 mV;
  `eos_esp32_adc_cal_source()` says which. `tests/test_esp32_adc_cal.c`
  checks it against ESP-IDF's own output. One deliberate difference: ESP-IDF
  v5.4 uses Vref 0 in its 12 dB table on two-point chips (readings above
  2880 then fall as the input rises); the port uses the chip's Vref. A
  conversion that does not finish in 1 ms returns `0xFFFFFFFF`.
- PWM: 1 Hz to 8.388 MHz (2^23 Hz: at least 1 bit of duty resolution, with
  the divider computed in 32 bits); the finest resolution up to 20 bits that fits.
  Channels 4-7 are refused: they would have to share a timer, and changing
  one channel's frequency would change another's.
- UART1/2: 300 baud to 5 Mbaud, 5-8 data bits, none/even/odd parity, 1 or 2
  stop bits; RX has a pull-up.
- Using a pin for ADC or DAC moves it to the RTC mux; GPIO and the
  peripheral routes take it back.

`periph_test.c` on the board (QEMU does not model these peripherals; CI only
builds the test):

```
CHECK adc calibration source efuse-vref vref 1107mV  PASS
CHECK dac0->adc18 raw 0/912/1882/2847/4095 1650mV->1658mV  PASS
CHECK dac1->adc19 raw 0/910/1891/2837/4095 1650mV->1666mV  PASS
CHECK adc1 ch4 (GPIO32) pull-up/pull-down up 4095 down 0  PASS
CHECK pwm ch0 init 1 kHz 25% -> 200 edges/200ms duty x10 248  PASS
CHECK pwm set_duty 75% -> duty x10 750  PASS
CHECK pwm set_freq 5 kHz -> 1001 edges/200ms duty x10 750  PASS
CHECK pwm stop -> 0 edges  PASS
CHECK pwm rejects channel 4, 0 Hz and duty > 100%  PASS
CHECK pwm frequency range: 1 Hz and 8388608 Hz accepted, 8388609 Hz refused  PASS
CHECK uart1+2 loopback, 5-8 data bits x none/even/odd x 1/2 stop at 115200 -> 48/48  PASS
CHECK uart1+2 loopback, 5-8 data bits x none/even/odd x 1/2 stop at 921600 -> 48/48  PASS
CHECK uart rejects port 3, 4 data bits and 10 Mbaud  PASS
CHECK GPIO25 digital again after the DAC -> 0 wrong of 20  PASS
PERIPH TEST PASSED
```

ESP32 UART behaviours the driver works around, each found on the board and
matched against ESP-IDF's `uart_ll.h`:

- RX FIFO is read through the peripheral-bus address, never the AHB alias
  (ESP-IDF: "Read fifo, we use APB address"); through the alias, received
  bytes came back wrong.
- Neither FIFO reset bit is used: `RXFIFO_RST` does not empty the RX FIFO,
  and ESP-IDF toggles `TXFIFO_RST` only on UART0. With both toggled, every
  second reconfiguration of UART2 received garbage. The RX FIFO is drained
  by reading until its count and pointers agree.
- After a reconfiguration or a loopback switch the driver waits two frame
  times for the receiver to go idle before draining it.

## Limits

- One core. The APP CPU stays in reset.
- A preempted task carries a 112-byte frame (96 plus the caller's 16-byte
  save area) on its stack, against 68 bytes on Cortex-M, and windowed frames
  are larger than CALL0 ones. `EOS_MIN_STACK_SIZE` (128) still admits a task,
  but leaves it 16 bytes of its own; the tests use 512 to 3072.
- No watchdog: the two the ROM arms are switched off at boot, and nothing
  re-arms one. A hung task stays hung.
- The CPU stays on the 40 MHz crystal the ROM leaves it on. On the board above
  1000 ticks took about 1.004 s of host time (0.4% slow) in every run; the
  cause (crystal tolerance or otherwise) was not investigated.
- Only level-1 CPU interrupt lines can carry handlers
  (`eos_arch_irq_register()`); routing a peripheral to one through the
  interrupt matrix is not implemented. `eos_arch_irq_set_priority()` does
  nothing, because Xtensa fixes each line's level in hardware.
- The HAL implements GPIO, UART0-2, ADC, DAC and PWM (below). UART0 stays at
  the 115200 8N1 the ROM set. SPI, I2C, timers, Wi-Fi and Bluetooth calls
  return -1. GPIO refuses GPIO1/3 (the console), GPIO6-11 (the module's
  flash), outputs on the input-only GPIO34-39, and pulls on GPIO34-39, which
  have none.
- QEMU's GPIO model does not reflect outputs in `GPIO_IN`, hence
  `-DEOS_TEST_PAD_READBACK=0` there.
