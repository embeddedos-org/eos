# SPDX-License-Identifier: MIT
# Copyright (c) 2026 EoS Project

"""The GDB stub is compiled out by default and has no authentication.

The stub (debug/src/gdb_stub.c) used to be compiled into libeos_debug.a in
every build with no flag anywhere: no product profile enabled it and no
production code called it, so it reached no shipped image -- but only
because nothing happened to link the archive. The first board port or
downstream consumer that linked eos_debug would have shipped an
unauthenticated remote stub with raw memory access.

The gate, pinned here in the source:
- EOS_ENABLE_GDB_STUB defaults to 0 in include/eos/eos_config.h, no
  product profile sets it, and gdb_stub.c is wrapped in the flag, so a
  production build contains no stub code at all.
- Test builds enable the flag (root CMakeLists.txt, EOS_BUILD_TESTS only)
  so test_debug can still exercise the stub. A production build must
  never define it.
- Where the stub is enabled, memory access is range-checked against
  explicitly registered ranges (default: none, deny all), and the header
  states plainly that there is no authentication.
"""

import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
STUB_C = REPO_ROOT / "debug" / "src" / "gdb_stub.c"
STUB_H = REPO_ROOT / "debug" / "include" / "eos" / "gdb_stub.h"
CONFIG_H = REPO_ROOT / "include" / "eos" / "eos_config.h"
ROOT_CMAKE = REPO_ROOT / "CMakeLists.txt"


def _text(path):
    text = path.read_text(encoding="utf-8")
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", " ", text)


def test_stub_defaults_off_in_eos_config():
    text = CONFIG_H.read_text(encoding="utf-8")
    m = re.search(r"#ifndef EOS_ENABLE_GDB_STUB\s*\n#define EOS_ENABLE_GDB_STUB\s+(\d)", text)
    assert m, "EOS_ENABLE_GDB_STUB has no default in include/eos/eos_config.h"
    assert m.group(1) == "0", "EOS_ENABLE_GDB_STUB must default to 0"


def test_stub_source_is_wrapped_in_the_flag():
    text = _text(STUB_C)
    assert '#include "eos/eos_config.h"' in text, \
        "gdb_stub.c does not include eos_config.h, so the gate has no default"
    assert re.search(r"#if EOS_ENABLE_GDB_STUB\b", text), \
        "gdb_stub.c is not wrapped in #if EOS_ENABLE_GDB_STUB"
    # Read raw here: comment-stripping would erase the /* ... */ marker
    # after the gate's #endif.
    raw = STUB_C.read_text(encoding="utf-8")
    endif_lines = [ln.strip() for ln in raw.splitlines()
                   if ln.strip().startswith("#endif")]
    assert endif_lines and "EOS_ENABLE_GDB_STUB" in endif_lines[-1], \
        "the gate's #endif is not the last directive in gdb_stub.c -- " \
        "code after it would compile unconditionally"


def test_no_product_profile_enables_the_stub():
    for profile in sorted((REPO_ROOT / "products").glob("*.h")):
        text = profile.read_text(encoding="utf-8")
        assert "EOS_ENABLE_GDB_STUB" not in text, \
            f"{profile.name} sets EOS_ENABLE_GDB_STUB -- the stub must stay " \
            "off in every product profile"


def test_stub_is_enabled_only_for_test_builds():
    text = ROOT_CMAKE.read_text(encoding="utf-8")
    hits = [m.start() for m in re.finditer(r"EOS_ENABLE_GDB_STUB=1", text)]
    assert hits, "no test build enables the stub -- test_debug cannot link it"
    for pos in hits:
        ifs = re.findall(r"if\(([^)]*)\)", text[:pos])
        assert ifs, "EOS_ENABLE_GDB_STUB=1 sits outside any if() block"
        assert ifs[-1].strip().startswith("EOS_BUILD_TESTS"), \
            "EOS_ENABLE_GDB_STUB=1 is enabled outside a test build -- " \
            "a production build must never define it"


def test_header_states_there_is_no_authentication():
    text = STUB_H.read_text(encoding="utf-8")
    assert "EOS_ENABLE_GDB_STUB" in text, \
        "gdb_stub.h does not document the gate"
    assert re.search(r"[Nn]o authentication", text), \
        "gdb_stub.h does not state that the stub has no authentication"


def test_memory_hooks_are_range_checked():
    text = _text(STUB_C)
    assert "eos_gdb_set_mem_ranges" in text, "no range table in gdb_stub.c"
    assert "eos_gdb_mem_allowed" in text, "no range check in gdb_stub.c"
    for fn in ("eos_gdb_read_mem", "eos_gdb_write_mem"):
        body = text[text.index(f"int {fn}"):]
        body = body[:body.index("\n}\n")]
        assert "eos_gdb_mem_allowed" in body, \
            f"{fn} does not call the range check"
        assert body.index("eos_gdb_mem_allowed") < body.index("memcpy"), \
            f"{fn} copies memory before checking the range"
    header = STUB_H.read_text(encoding="utf-8")
    assert "eos_gdb_set_mem_ranges" in header
    assert "eos_gdb_mem_allowed" in header
