#!/usr/bin/env bash
# Run every ESP32 test: the three QEMU tests, then each test image on a board.
#   sim/qemu_esp32/board_suite.sh OUTDIR [PORT]
# PORT defaults to /dev/ttyUSB0. Needs what build.sh and run.sh need, plus
# pyserial for capture_serial.py (PYTHON overrides python3). Writing at 0x1000
# replaces the board's bootloader: see README.md for backing up the flash.
# Prints one PASS/FAIL line per test; exits 1 if any failed.
set -u
out=$1
port=${2:-/dev/ttyUSB0}
here=$(cd "$(dirname "$0")" && pwd)
py=${PYTHON:-python3}
esptool=${ESPTOOL:-esptool}
mkdir -p "$out"
fail=0

# EXTRA: more compiler flags for the next test (the SMP test's).
EXTRA=""

qemu() {   # name expected-text sources...
    local n=$1 e=$2; shift 2
    if ! EOS_EXTRA_CFLAGS="-DEOS_TEST_PAD_READBACK=0 $EXTRA" "$here/build.sh" "$out/q_$n.elf" "$@" >"$out/q_$n.build" 2>&1; then
        echo "QEMU  $n: FAIL (build)"; fail=1; return
    fi
    if "$here/run.sh" "$out/q_$n.flash.bin" 90 "$e" >"$out/q_$n.log" 2>&1; then
        echo "QEMU  $n: PASS"
    else
        echo "QEMU  $n: FAIL"; fail=1
    fi
}

board() {  # name expected-text seconds sources...
    local n=$1 e=$2 t=$3; shift 3
    if ! EOS_EXTRA_CFLAGS="$EXTRA" "$here/build.sh" "$out/b_$n.elf" "$@" >"$out/b_$n.build" 2>&1; then
        echo "BOARD $n: FAIL (build)"; fail=1; return
    fi
    if ! $esptool --chip esp32 --port "$port" write_flash 0x1000 "$out/b_$n.bin" >"$out/b_$n.flash" 2>&1; then
        echo "BOARD $n: FAIL (flash)"; fail=1; return
    fi
    "$py" "$here/capture_serial.py" "$port" "$t" >"$out/b_$n.log" 2>&1
    if grep -q "$e" "$out/b_$n.log"; then
        echo "BOARD $n: PASS"
    else
        echo "BOARD $n: FAIL"; grep -E "FAIL|FATAL" "$out/b_$n.log" | head -5; fail=1
    fi
}

cd "$here/../.." || exit 1
s=sim/qemu_esp32
qemu sched "SCHEDULER TEST PASSED" $s/scheduler_test.c
qemu ctx "CONTEXT TEST PASSED" $s/context_test.c $s/context_regcheck.S
qemu fault "EOS FATAL code=7" $s/fault_test.c
EXTRA="-DEOS_SMP_CORES=2 -DEOS_TASK_STACK_POOL_SIZE=32768 -DEOS_MAX_TASKS=32"
qemu smp "SMP TEST PASSED" $s/smp_test.c $s/context_regcheck.S
EXTRA=""
board sched "SCHEDULER TEST PASSED" 12 $s/scheduler_test.c
grep -E "verdict|spacing" "$out/b_sched.log"
board ctx "CONTEXT TEST PASSED" 4 $s/context_test.c $s/context_regcheck.S
board fault "EOS FATAL code=7" 3 $s/fault_test.c
board periph "PERIPH TEST PASSED" 4 $s/periph_test.c
board bus "BUS TEST PASSED" 4 $s/bus_test.c
board rtc "RTC TEST PASSED" 10 $s/rtc_test.c
EXTRA="-DEOS_SMP_CORES=2 -DEOS_TASK_STACK_POOL_SIZE=32768 -DEOS_MAX_TASKS=32"
board smp "SMP TEST PASSED" 8 $s/smp_test.c $s/context_regcheck.S
EXTRA=""
exit $fail
