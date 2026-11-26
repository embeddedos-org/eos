#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 EoS Project
"""Reset an ESP32 board into its flashed image and record its UART0 output.

    capture_serial.py /dev/ttyUSB0 [seconds]

Prints each line prefixed with the host time since reset, then checks the
scheduler test's verdict and the heartbeat spacing. The image prints a
heartbeat every 1000 kernel ticks, so on a board whose tick matches wall time
the heartbeats arrive 1.000 s apart. Exits 0 only if the board printed
"SCHEDULER TEST PASSED" and at least three heartbeats arrived 0.9-1.1 s
apart; 1 otherwise.

Needs pyserial (installed with esptool). Uses the DTR/RTS auto-reset circuit
of DevKit boards: RTS drives EN, DTR drives GPIO0.
"""

import itertools
import statistics
import sys
import time

import serial


def main() -> int:
    port = sys.argv[1]
    seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 12.0

    # A short read timeout: each line is stamped when a read returns, so the
    # timeout bounds the timestamp error (50 ms skewed one gap to 0.954 s).
    ser = serial.Serial(port, 115200, timeout=0.002)
    # Hold EN low with GPIO0 high (normal boot, not download mode), release.
    ser.dtr = False
    ser.rts = True
    time.sleep(0.1)
    ser.reset_input_buffer()
    ser.rts = False
    t0 = time.monotonic()

    lines = []
    buf = b""
    # The USB-serial bridge can still hold bytes from before the reset, which
    # arrive in the first few ms; record from the boot ROM's banner on.
    booted = False
    while time.monotonic() - t0 < seconds:
        buf += ser.read(256)
        while b"\n" in buf:
            raw, buf = buf.split(b"\n", 1)
            text = raw.decode("utf-8", "replace").rstrip("\r")
            if not booted:
                # "rst:0x1 (POWERON_RESET),boot:0x13 (...)"; stale bytes can
                # run into the front of it, so match its tail.
                if "_RESET),boot:" not in text:
                    continue
                booted = True
                text = text[text.rfind("(", 0, text.index("_RESET),boot:")) :]
            stamp = time.monotonic() - t0
            lines.append((stamp, text))
            print(f"[{stamp:7.3f}s] {text}", flush=True)
    ser.close()

    passed = any("SCHEDULER TEST PASSED" in t for _, t in lines)
    beats = [s for s, t in lines if t.startswith("heartbeat tick=")]
    gaps = [b - a for a, b in itertools.pairwise(beats)]
    print(f"verdict: {'PASS' if passed else 'missing'}; heartbeats: {len(beats)}")
    if gaps:
        print(
            f"heartbeat spacing: mean {statistics.mean(gaps):.4f}s "
            f"min {min(gaps):.4f}s max {max(gaps):.4f}s over {len(gaps)} gaps"
        )
    timing_ok = len(gaps) >= 2 and all(0.9 <= g <= 1.1 for g in gaps)
    return 0 if passed and timing_ok else 1


if __name__ == "__main__":
    sys.exit(main())
