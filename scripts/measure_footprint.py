#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 EoS Contributors
"""Measure flash and RAM footprint of built EoS artifacts.

ADR-012 selects Mbed TLS as the cryptographic provider and makes a measured
footprint budget a *blocking* prerequisite rather than a follow-up: Mbed TLS and
lwIP are materially larger than the hand-written code they replace, and the §21
Nano profile has to be able to say no.

"Measure, don't assume" needs something that measures. This is it, and it
deliberately lands before any import: the number that matters is the delta
between a build without the dependency and a build with it, which requires a
baseline taken first.

What it reports, per static library and in total:

    text   code + read-only data      -> flash
    data   initialised writable data  -> flash AND RAM
    bss    zero-initialised data      -> RAM only

flash = text + data, ram = data + bss. That accounting is why `size` alone is
not enough — `data` is charged to both, and reading only the "dec" column
understates RAM and overstates nothing.

Usage:
    scripts/measure_footprint.py --build-dir build/host
    scripts/measure_footprint.py --build-dir build/host --json baseline.json
    scripts/measure_footprint.py --build-dir build/host --compare baseline.json

Exit codes:
    0  measured (or compared within budget)
    1  a --budget ceiling was exceeded, or --compare found a regression
    2  script error (no build directory, no size tool)
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Optional

REPO_ROOT = Path(__file__).resolve().parent.parent


def find_size_tool(explicit: Optional[str]) -> str:
    """Locate a `size` binary. A cross build must be measured with its own."""
    if explicit:
        if not shutil.which(explicit):
            raise SystemExit(f"error: size tool '{explicit}' not found on PATH")
        return explicit
    for candidate in ("size", "llvm-size"):
        if shutil.which(candidate):
            return candidate
    raise SystemExit(
        "error: no 'size' tool found. Install binutils, or pass --size-tool "
        "(e.g. --size-tool arm-none-eabi-size for a Cortex-M build)."
    )


def measure_archive(size_tool: str, archive: Path) -> Dict[str, int]:
    """Sum section sizes across every object in one static library."""
    result = subprocess.run(
        [size_tool, "-t", str(archive)],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        return {"text": 0, "data": 0, "bss": 0, "error": result.stderr.strip()[:120]}

    lines = [ln for ln in result.stdout.splitlines() if ln.strip()]
    # `size -t` prints a totals row last, marked "(TOTALS)".
    totals = [ln for ln in lines if "TOTALS" in ln]
    row = totals[-1] if totals else lines[-1]
    parts = row.split()
    try:
        text, data, bss = int(parts[0]), int(parts[1]), int(parts[2])
    except (IndexError, ValueError):
        return {"text": 0, "data": 0, "bss": 0, "error": f"unparsed: {row[:80]}"}
    return {"text": text, "data": data, "bss": bss}


def collect(build_dir: Path, size_tool: str) -> dict:
    archives = sorted(build_dir.rglob("*.a"))
    if not archives:
        raise SystemExit(
            f"error: no static libraries under {build_dir}. Configure and build "
            "first, e.g. cmake -B build/host -G Ninja -DEOS_BUILD_TESTS=ON && "
            "cmake --build build/host"
        )

    components: Dict[str, Dict[str, int]] = {}
    for archive in archives:
        name = archive.stem
        if name.startswith("lib"):
            name = name[3:]
        components[name] = measure_archive(size_tool, archive)

    total = {
        key: sum(c.get(key, 0) for c in components.values())
        for key in ("text", "data", "bss")
    }
    total["flash"] = total["text"] + total["data"]
    total["ram"] = total["data"] + total["bss"]

    return {
        "size_tool": size_tool,
        "build_dir": str(build_dir),
        "components": components,
        "total": total,
    }


def render(report: dict) -> None:
    components = report["components"]
    width = max((len(n) for n in components), default=9)
    width = max(width, 9)

    print(f"{'component'.ljust(width)}  {'text':>9} {'data':>7} {'bss':>9} "
          f"{'flash':>9} {'ram':>9}")
    print("-" * (width + 48))

    for name in sorted(components, key=lambda n: -(components[n].get("text", 0)
                                                   + components[n].get("data", 0))):
        c = components[name]
        if "error" in c:
            print(f"{name.ljust(width)}  {'-':>9} {'-':>7} {'-':>9} "
                  f"{'-':>9} {'-':>9}   [{c['error']}]")
            continue
        flash = c["text"] + c["data"]
        ram = c["data"] + c["bss"]
        print(f"{name.ljust(width)}  {c['text']:>9,} {c['data']:>7,} {c['bss']:>9,} "
              f"{flash:>9,} {ram:>9,}")

    t = report["total"]
    print("-" * (width + 48))
    print(f"{'TOTAL'.ljust(width)}  {t['text']:>9,} {t['data']:>7,} {t['bss']:>9,} "
          f"{t['flash']:>9,} {t['ram']:>9,}")
    print()
    print("flash = text + data   ram = data + bss   "
          "(data is charged to both: it lives in flash and is copied to RAM)")


def compare(report: dict, baseline_path: Path) -> int:
    baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
    now, before = report["total"], baseline["total"]

    if report["size_tool"] != baseline.get("size_tool"):
        print(f"warning: baseline was taken with '{baseline.get('size_tool')}', "
              f"this run used '{report['size_tool']}'. Sizes are not comparable "
              "across toolchains.", file=sys.stderr)

    print(f"{'metric':<8} {'baseline':>12} {'now':>12} {'delta':>12} {'':>8}")
    print("-" * 56)
    regressed = False
    for key in ("flash", "ram"):
        b, n = before.get(key, 0), now.get(key, 0)
        delta = n - b
        pct = f"{(delta / b * 100):+.1f}%" if b else "n/a"
        print(f"{key:<8} {b:>12,} {n:>12,} {delta:>+12,} {pct:>8}")
        if delta > 0:
            regressed = True

    print()
    if regressed:
        print("Footprint grew. That is not automatically wrong — ADR-012 expects "
              "Mbed TLS to cost more than the code it replaces. It has to be "
              "recorded against the Nano budget, not discovered later.")
    else:
        print("No growth.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", type=Path, default=REPO_ROOT / "build" / "host",
                        help="directory containing built static libraries")
    parser.add_argument("--size-tool", default=None,
                        help="size binary to use (e.g. arm-none-eabi-size)")
    parser.add_argument("--json", type=Path, default=None,
                        help="write the report as JSON to this path")
    parser.add_argument("--compare", type=Path, default=None,
                        help="compare against a previously written JSON report")
    parser.add_argument("--budget-flash", type=int, default=None,
                        help="fail if total flash exceeds this many bytes")
    parser.add_argument("--budget-ram", type=int, default=None,
                        help="fail if total ram exceeds this many bytes")
    args = parser.parse_args()

    if not args.build_dir.is_dir():
        raise SystemExit(f"error: {args.build_dir} does not exist")

    report = collect(args.build_dir, find_size_tool(args.size_tool))
    render(report)

    if args.json:
        args.json.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(f"\nWrote {args.json}")

    status = 0
    if args.compare:
        print()
        compare(report, args.compare)

    for key, ceiling in (("flash", args.budget_flash), ("ram", args.budget_ram)):
        if ceiling is not None and report["total"][key] > ceiling:
            print(f"\nFAIL: {key} {report['total'][key]:,} exceeds budget {ceiling:,}")
            status = 1

    return status


if __name__ == "__main__":
    sys.exit(main())
