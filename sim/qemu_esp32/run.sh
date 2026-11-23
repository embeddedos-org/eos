#!/usr/bin/env bash
# Boot an EoS flash image on Espressif's QEMU esp32 machine and check what the
# guest prints.
#   sim/qemu_esp32/run.sh OUT.flash.bin [timeout_seconds] [expected_text]
# expected_text defaults to "SCHEDULER TEST PASSED". Exits 0 when the guest
# prints it and nothing listed under "bad" below, 1 otherwise.
#
# The guest never halts (the ESP32 has no semihosting exit), so QEMU is stopped
# once the expected text, a failure marker or the timeout arrives; after the
# expected text it runs two seconds longer, so a failure printed just after it
# is still caught. QEMU overrides the binary.
set -uo pipefail
flash=$1
limit=${2:-60}
expect=${3:-"SCHEDULER TEST PASSED"}
qemu=${QEMU:-qemu-system-xtensa}
log=$(mktemp)
trap 'rm -f "$log"' EXIT

# Failure markers. A test that expects a fatal report only treats the other
# markers as failures.
bad=("TEST FAILED" "RAN AFTER FAULT")
[[ "$expect" == "EOS FATAL"* ]] || bad+=("EOS FATAL")

"$qemu" -machine esp32 -nographic -no-reboot \
    -drive "file=$flash,if=mtd,format=raw" >"$log" 2>&1 &
pid=$!

seen_bad() {
    local m
    for m in "${bad[@]}"; do grep -qF -- "$m" "$log" && return 0; done
    return 1
}

verdict=""
for _ in $(seq $((limit * 10))); do
    if seen_bad; then verdict=fail; break; fi
    if grep -qF -- "$expect" "$log"; then verdict=pass; break; fi
    kill -0 "$pid" 2>/dev/null || break
    sleep 0.1
done
if [ "$verdict" = pass ]; then
    sleep 2
    seen_bad && verdict=fail
fi
kill "$pid" 2>/dev/null
wait "$pid" 2>/dev/null
tr -d '\r' <"$log"
if [ "$verdict" = pass ]; then exit 0; fi
echo "qemu_esp32: expected \"$expect\" not confirmed (${verdict:-timeout after ${limit}s})" >&2
exit 1
