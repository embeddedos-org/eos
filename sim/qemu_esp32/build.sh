#!/usr/bin/env bash
# Build an EoS image for the ESP32 from sim.yaml plus application sources.
#   sim/qemu_esp32/build.sh OUT.elf app1.c [app2.c ...]
# Writes OUT.elf, OUT.bin (the image the boot ROM loads; flash it at 0x1000)
# and OUT.flash.bin (a whole 4 MB flash for QEMU).
#
# Run it in QEMU:  sim/qemu_esp32/run.sh OUT.flash.bin
# Flash a board:   esptool --chip esp32 --port /dev/ttyUSB0 write_flash 0x1000 OUT.bin
#
# Needs xtensa-esp32-elf-gcc (CC overrides it), esptool (ESPTOOL overrides
# it) and python3-yaml. EOS_EXTRA_CFLAGS adds compiler flags; the QEMU test
# uses it for -DEOS_TEST_PAD_READBACK=0 (see scheduler_test.c).
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
out=$1; shift
cc=${CC:-xtensa-esp32-elf-gcc}
esptool=${ESPTOOL:-esptool}
python3 - "$here/sim.yaml" "$root" "$out" "$cc" "$esptool" "$@" <<'PY'
import os, shlex, subprocess, sys, tempfile, yaml
cfg_path, root, out, cc, esptool, *apps = sys.argv[1:]
cfg = yaml.safe_load(open(cfg_path))
inc = [f"-I{os.path.join(root, d)}" for d in cfg["includes"]]
extra = shlex.split(os.environ.get("EOS_EXTRA_CFLAGS", ""))
srcs = [os.path.join(root, s) for s in cfg["sources"]] + [os.path.abspath(a) for a in apps]
objdir = tempfile.mkdtemp(prefix="eos-esp32-")
objs = []
for i, src in enumerate(srcs):
    obj = os.path.join(objdir, f"{i:02d}_{os.path.basename(src)}.o")
    subprocess.run([cc, *cfg["cflags"], *extra, *inc, "-c", src, "-o", obj], check=True)
    objs.append(obj)
subprocess.run([cc, *cfg["ldflags"], "-T", os.path.join(root, cfg["linker_script"]),
                *objs, *cfg["libs"], "-Wl,-Map," + out + ".map", "-o", out], check=True)

img = cfg["image"]
stem = out[:-4] if out.endswith(".elf") else out
app_bin, flash_bin = stem + ".bin", stem + ".flash.bin"
tool = shlex.split(esptool)
# Underscore spellings: esptool 4.x (Ubuntu's package) accepts only these,
# and 5.x still accepts them.
subprocess.run([*tool, "--chip", img["chip"], "elf2image",
                "--flash_mode", img["flash_mode"], "--flash_freq", img["flash_freq"],
                "--flash_size", img["flash_size"], "-o", app_bin, out], check=True)
# The whole-flash image for QEMU is erased flash with the image at its
# offset. Built here rather than with merge_bin, whose padding option differs
# between esptool 4.x and 5.x; the result is byte-identical.
sizes = {"1MB": 1, "2MB": 2, "4MB": 4, "8MB": 8, "16MB": 16}
flash = bytearray(b"\xff" * (sizes[img["flash_size"]] << 20))
data = open(app_bin, "rb").read()
off = int(str(img["offset"]), 0)
if off + len(data) > len(flash):
    sys.exit(f"build.sh: {app_bin} ({len(data)} bytes) does not fit in {img['flash_size']} at {off:#x}")
flash[off:off + len(data)] = data
open(flash_bin, "wb").write(flash)
PY
