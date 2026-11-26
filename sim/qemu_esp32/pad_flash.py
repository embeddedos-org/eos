#!/usr/bin/env python3
"""pad_flash.py IN.bin OUT.flash.bin OFFSET FLASH_SIZE (e.g. 4MB)

Erased flash with IN.bin at OFFSET: the whole-flash image QEMU boots, the
same bytes build.sh writes.
"""
import sys

data = open(sys.argv[1], "rb").read()
off = int(sys.argv[3], 0)
# Not str.removesuffix(): that is Python 3.9, and CONTRIBUTING.md puts the
# floor at 3.8.
size_arg = sys.argv[4]
if size_arg.endswith("MB"):
    size_arg = size_arg[:-len("MB")]
size = int(size_arg) << 20
if off + len(data) > size:
    sys.exit(f"pad_flash.py: {sys.argv[1]} ({len(data)} bytes) does not fit in {sys.argv[4]} at {off:#x}")
open(sys.argv[2], "wb").write(b"\xff" * off + data + b"\xff" * (size - off - len(data)))
