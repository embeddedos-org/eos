# SPDX-License-Identifier: MIT
# Copyright (c) 2026 EoS Project
"""A -specs= file must reach the link line exactly once.

`arm-none-eabi-gcc` fails outright when the same spec file is supplied twice:

    fatal error: nosys.specs: attempt to rename spec 'link_gcc_c_sequence'
                 to already defined spec 'nosys_link_gcc_c_sequence'

CMake links through the compiler driver, so CMAKE_C_FLAGS_INIT reaches the
link line too -- naming a spec there *and* in CMAKE_EXE_LINKER_FLAGS_INIT
supplies it twice.

Nothing in CI would notice. Every ARM toolchain here sets
CMAKE_TRY_COMPILE_TARGET_TYPE to STATIC_LIBRARY and no cross build links an
executable, so `Cross-compile ARM Cortex-M4` passed identically with the
duplicate present and absent -- it passed on the unfixed commit. The defect
was found by accident, when a PR briefly put cmd/eos into the cross build.
This is the check that would have caught it, and it needs no ARM toolchain.

Modelled on tests/unit/test_cmake_test_registration.py: parse the build files
and fail on the class of mistake rather than on one instance of it.
"""

import re
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
TOOLCHAINS = REPO / "toolchains"

# Variables CMake puts on the link line. C/CXX flags get there because CMake
# links through the compiler driver. ASM is here because CMake picks a
# target's linker language from its sources, so an executable built only from
# .S files links with CMAKE_ASM_FLAGS -- a spec named there and in
# CMAKE_EXE_LINKER_FLAGS_INIT is the same duplicate. No such target exists
# today; a bare-metal tree acquires them.
LINK_REACHING = (
    "CMAKE_C_FLAGS_INIT",
    "CMAKE_CXX_FLAGS_INIT",
    "CMAKE_ASM_FLAGS_INIT",
    "CMAKE_EXE_LINKER_FLAGS_INIT",
)

# The .yaml descriptors are the second authority on these flags: the eos CLI
# parses them into EosToolchain.cflags/ldflags (toolchains/src/toolchain.c),
# a separate consumer from CMake. Nothing combines the two, so a .yaml naming
# a spec in cflags is not a live duplicate -- but it is the same mistake in
# the other description of the same toolchain, and nothing was checking it.
YAML_LINK_REACHING = ("cflags", "ldflags")

SPECS_RE = re.compile(r"--?specs=(\S+)")

# set(VAR "value") and set(VAR value), the value possibly spanning lines.
SET_RE = re.compile(r'set\(\s*(\w+)\s+("(?:[^"\\]|\\.)*"|[^)\n]+)\s*\)', re.S)
# string(APPEND VAR " value") -- the ordinary way to add to these variables,
# which the first version of this file could not see at all.
APPEND_RE = re.compile(r'string\(\s*APPEND\s+(\w+)\s+("(?:[^"\\]|\\.)*"|[^)\n]+)\s*\)', re.S)
REF_RE = re.compile(r"\$\{(\w+)\}")


def _unquote(value):
    value = value.strip()
    if len(value) >= 2 and value[0] == '"' and value[-1] == '"':
        return value[1:-1]
    return value


def _resolve(value, variables, depth=0):
    """Expand ${VAR} against what the file has set so far.

    arm-cortex-m4.cmake and arm-none-eabi-stm32f4.cmake both build their flag
    strings from ${CPU_FLAGS}. Without this, putting -specs= into CPU_FLAGS
    hides it from every check below while it still reaches two link-line
    variables.
    """
    if depth > 8:
        return value
    def sub(match):
        return _resolve(variables.get(match.group(1), ""), variables, depth + 1)
    return REF_RE.sub(sub, value)


def _specs_per_variable(text):
    """{variable: [spec files]} for the variables that reach a link line.

    The file is walked in order so that ${...} resolves against the values in
    force at that point, which is what CMake does.
    """
    variables = {}
    found = {}

    events = [(m.start(), "set", m) for m in SET_RE.finditer(text)]
    events += [(m.start(), "append", m) for m in APPEND_RE.finditer(text)]
    events.sort()

    for _pos, kind, match in events:
        var = match.group(1)
        value = _resolve(_unquote(match.group(2)), variables)
        variables[var] = value if kind == "set" else (variables.get(var, "") + " " + value)
        if var in LINK_REACHING:
            specs = SPECS_RE.findall(variables[var])
            if specs:
                found[var] = specs
            else:
                found.pop(var, None)
    return found


def _yaml_specs_per_key(text):
    """{key: [spec files]} from a .yaml toolchain descriptor.

    Read line-wise rather than with a YAML parser so the test has no
    dependency the pytest job does not already install.
    """
    found = {}
    for line in text.splitlines():
        line = line.strip()
        if line.startswith("#") or ":" not in line:
            continue
        key, _, value = line.partition(":")
        key = key.strip()
        if key in YAML_LINK_REACHING:
            specs = SPECS_RE.findall(value)
            if specs:
                found[key] = specs
    return found


def _toolchain_files():
    files = sorted(TOOLCHAINS.glob("*.cmake"))
    assert files, f"no toolchain files found under {TOOLCHAINS}"
    return files


def _descriptor_files():
    files = sorted(TOOLCHAINS.glob("*.yaml"))
    assert files, f"no toolchain descriptors found under {TOOLCHAINS}"
    return files


def test_no_spec_file_reaches_the_link_line_twice():
    offenders = []
    for path in _toolchain_files():
        per_var = _specs_per_variable(path.read_text())
        seen = {}
        for var, specs in per_var.items():
            for spec in specs:
                seen.setdefault(spec, []).append(var)
        for spec, variables in seen.items():
            if len(variables) > 1:
                offenders.append(
                    f"{path.name}: -specs={spec} is set in "
                    f"{' and '.join(sorted(variables))}, so it reaches the "
                    f"link line more than once"
                )
    assert not offenders, (
        "arm-none-eabi-gcc rejects a duplicated spec file:\n  "
        + "\n  ".join(offenders)
    )


def test_specs_are_declared_on_the_linker_flags_not_the_compiler_flags():
    """Where a toolchain names a spec at all, it names it on the link line.

    Putting it in CMAKE_C_FLAGS_INIT happens to work today only because CMake
    links through the driver. It is the arrangement that turns into a
    duplicate the moment someone adds the linker entry that looks missing.
    """
    offenders = []
    for path in _toolchain_files():
        per_var = _specs_per_variable(path.read_text())
        for var in ("CMAKE_C_FLAGS_INIT", "CMAKE_CXX_FLAGS_INIT",
                    "CMAKE_ASM_FLAGS_INIT"):
            for spec in per_var.get(var, []):
                offenders.append(f"{path.name}: -specs={spec} is set in {var}")
    assert not offenders, (
        "declare spec files in CMAKE_EXE_LINKER_FLAGS_INIT:\n  "
        + "\n  ".join(offenders)
    )


def test_descriptors_declare_specs_on_ldflags_only():
    """The .yaml side already had this right; nothing was holding it there."""
    offenders = []
    for path in _descriptor_files():
        per_key = _yaml_specs_per_key(path.read_text())
        for spec in per_key.get("cflags", []):
            offenders.append(f"{path.name}: -specs={spec} is in cflags")
    assert not offenders, (
        "a spec file is a link-time option; declare it in ldflags:\n  "
        + "\n  ".join(offenders)
    )


def test_a_spec_hidden_in_an_interpolated_variable_is_still_seen():
    """The parser must follow ${...}, or the check is trivially bypassed.

    Both ARM Cortex-M4 toolchains build their flags from ${CPU_FLAGS}. If a
    spec were added there it would reach CMAKE_C_FLAGS_INIT and
    CMAKE_EXE_LINKER_FLAGS_INIT while a literal-only scan saw neither.
    """
    text = (
        'set(CPU_FLAGS "-mcpu=cortex-m4 -specs=nosys.specs")\n'
        'set(CMAKE_C_FLAGS_INIT "${CPU_FLAGS} -ffunction-sections")\n'
        'set(CMAKE_EXE_LINKER_FLAGS_INIT "${CPU_FLAGS} -Wl,--gc-sections")\n'
    )
    per_var = _specs_per_variable(text)
    assert per_var.get("CMAKE_C_FLAGS_INIT") == ["nosys.specs"]
    assert per_var.get("CMAKE_EXE_LINKER_FLAGS_INIT") == ["nosys.specs"]


def test_a_spec_added_with_string_append_is_still_seen():
    """string(APPEND ...) is the ordinary way to add to these variables."""
    text = (
        'set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections")\n'
        'string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT " -specs=nosys.specs")\n'
        'set(CMAKE_C_FLAGS_INIT "-specs=nosys.specs")\n'
    )
    per_var = _specs_per_variable(text)
    assert per_var.get("CMAKE_EXE_LINKER_FLAGS_INIT") == ["nosys.specs"]
    assert per_var.get("CMAKE_C_FLAGS_INIT") == ["nosys.specs"]


def test_a_multi_line_set_is_still_seen():
    """arm-none-eabi-stm32f4.cmake writes its linker flags across three lines.

    An earlier revision of this parser required the closing paren on the same
    line, so that file's specs were invisible to every check above while the
    suite stayed green -- the failure mode this whole test exists to prevent,
    one level up.
    """
    text = (
        'set(CPU_FLAGS "-mcpu=cortex-m4")\n'
        'set(CMAKE_EXE_LINKER_FLAGS_INIT\n'
        '    "${CPU_FLAGS} -specs=nano.specs -specs=nosys.specs"\n'
        ')\n'
    )
    per_var = _specs_per_variable(text)
    assert per_var.get("CMAKE_EXE_LINKER_FLAGS_INIT") == [
        "nano.specs", "nosys.specs"]


def test_every_arm_toolchain_is_actually_parsed():
    """A parser that silently sees nothing would pass every check above."""
    seen = {
        path.name: _specs_per_variable(path.read_text())
        for path in _toolchain_files()
    }
    for name in ("arm-cortex-m4.cmake", "arm-none-eabi-r5.cmake",
                 "arm-none-eabi-stm32f4.cmake"):
        assert seen.get(name), (
            f"{name} declares a spec file but the parser found none in it; "
            f"the checks above are passing vacuously for this file"
        )
