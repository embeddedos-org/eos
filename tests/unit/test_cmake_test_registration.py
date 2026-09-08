"""Regression tests for the CTest registrations in tests/CMakeLists.txt.

Every C suite in tests/ has to be named in tests/CMakeLists.txt to be compiled
and run at all. Nothing else notices when one is left out: the suite stops
building, ctest reports one fewer test, and the run still goes green.

That is how test_mpu_validate.c stopped running. It landed together with the
MPU fix it guards, and a later merge kept the file while dropping the
add_executable() that built it -- so the six cases covering regions the MPU
cannot program were compiled by nothing, and no run said so.

These parse tests/CMakeLists.txt statically, so no cmake or compiler is needed.
"""

import re
from pathlib import Path

TESTS_DIR = Path(__file__).resolve().parent.parent
CMAKELISTS = TESTS_DIR / "CMakeLists.txt"

ADD_EXECUTABLE_RE = re.compile(r"add_executable\(\s*(\w+)\s+([^)]*?)\)", re.S)
ADD_TEST_RE = re.compile(r"add_test\(\s*NAME\s+(\w+)\s+COMMAND\s+(\w+)")

#: Suites that are deliberately not built, and why. A name may only sit here
#: with a reason that is about the suite itself -- "it is broken" is not one,
#: because a broken suite is exactly what these tests exist to surface.
NOT_BUILT = {
    "test_net_integration.c":
        "asserts stub semantics: test_accept_no_client() expects "
        "eos_net_accept() to return EOS_SOCKET_INVALID, but the POSIX "
        "implementation blocks, so the suite hangs rather than fails",
    "test_firmware.c":
        "links eos_systems, whose firmware.c calls eos_backend_find(); "
        "backends/ is in no add_subdirectory() and its 12 sources no longer "
        "compile against the current headers, so nothing can resolve it",
    "test_profiles.c":
        "cannot run a case: main() accepts only the literal strings "
        '"products", "./products" and "../products", and products/ holds '
        "headers, not the *.yaml the scan looks for -- it reports 0/0 and "
        "exits 0, which is a pass that tested nothing",
    "test_performance_benchmarks.c":
        "asserts a wall-clock threshold (latency_ns < 100.0) measured by a "
        "busy loop, which is a property of the runner rather than of the code",
    "test_emulation_simulation.c":
        "asserts a literal against itself -- mock_val == 0x55AA55AA one line "
        "after mock_val is assigned that value -- and never reads the "
        "register pointer it declares, so it cannot fail",
}


def _cmake_text():
    return CMAKELISTS.read_text(encoding="utf-8")


def _c_suites():
    """Every C suite file in tests/, by file name."""
    return sorted(p.name for p in TESTS_DIR.glob("test_*.c"))


def _registered_sources():
    """Source file names named by an add_executable() in tests/CMakeLists.txt."""
    sources = set()
    for _target, source_list in ADD_EXECUTABLE_RE.findall(_cmake_text()):
        for source in source_list.split():
            sources.add(Path(source).name)
    return sources


def test_every_c_suite_is_built_or_listed():
    suites = _c_suites()
    assert suites, "expected to find test_*.c suites in tests/"

    registered = _registered_sources()
    missing = [n for n in suites if n not in registered and n not in NOT_BUILT]

    assert not missing, (
        "these suites exist in tests/ but no add_executable() in "
        "tests/CMakeLists.txt builds them, so they never run: "
        f"{missing}. Register them, or add each to NOT_BUILT with the reason."
    )


def test_every_built_suite_is_registered_with_ctest():
    text = _cmake_text()
    targets = {t for t, _s in ADD_EXECUTABLE_RE.findall(text)}
    commands = {c for _n, c in ADD_TEST_RE.findall(text)}

    unregistered = sorted(targets - commands)
    assert not unregistered, (
        "these test executables are built but never added to ctest, so a "
        f"failure in them cannot fail the build: {unregistered}"
    )


def test_not_built_list_has_no_stale_entries():
    """A suite that has been registered must leave the exclusion list."""
    registered = _registered_sources()
    stale = sorted(n for n in NOT_BUILT if n in registered)
    assert not stale, (
        f"these are built now and should be dropped from NOT_BUILT: {stale}"
    )


def test_not_built_names_still_exist():
    """An entry naming a file that is gone hides the next real omission."""
    present = set(_c_suites())
    gone = sorted(n for n in NOT_BUILT if n not in present)
    assert not gone, f"NOT_BUILT names files that no longer exist: {gone}"


def test_mpu_validate_suite_is_registered():
    """Pin the specific suite whose registration a merge dropped."""
    assert "test_mpu_validate.c" in _registered_sources(), (
        "test_mpu_validate.c is not built by tests/CMakeLists.txt -- the MPU "
        "region-programmability regression suite would silently stop running"
    )


# ---------------------------------------------------------------------------
# A suite can be built and registered and still not run what it contains.
#
# The checks above answer "is this file compiled?". They cannot answer "is this
# test called?", because a test function that nothing invokes is valid C. It
# compiles, it links, ctest runs its suite, and the suite passes -- having
# skipped it.
#
# tests/test_kernel.c:159 test_queue_send_waiter_overflow is the live instance:
# defined, never called from main(), so the waiter-table overflow fix it guards
# has had no executing test since it landed. eBoot had the same thing in
# tests/unit/test_ed25519.c, where a merge kept a test function and dropped its
# run_ call.
#
# Two declaration styles are in use here, and both are handled:
#   static void test_x(void) { ... }   called as test_x();
#   TEST(test_x) { ... }               called as run_test_x();
# ---------------------------------------------------------------------------

#: Test functions that are deliberately never called, and why.
UNCALLED_OK: dict = {}

TEST_MACRO_RE = re.compile(r"^TEST\(\s*(\w+)\s*\)", re.M)
TEST_FUNC_RE = re.compile(r"^static\s+void\s+(test_\w+)\s*\(\s*void\s*\)", re.M)


def _strip_comments(text: str) -> str:
    """Drop comments so a name mentioned only in prose does not read as a call."""
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", " ", text)


def _uncalled_in(path: Path):
    """Return test functions defined in `path` that nothing in it references.

    Counts bare identifier occurrences rather than `name(` so a function passed
    by address -- a callback, a task entry point -- is not reported as dead.
    """
    src = _strip_comments(path.read_text(encoding="utf-8", errors="replace"))
    dead = []
    for name in TEST_MACRO_RE.findall(src):
        # TEST(x) also defines run_x(); main() is expected to call that.
        if len(re.findall(rf"\brun_{re.escape(name)}\b", src)) == 0:
            dead.append(name)
    for name in TEST_FUNC_RE.findall(src):
        # One occurrence is the definition itself; a call adds a second.
        if len(re.findall(rf"\b{re.escape(name)}\b", src)) < 2:
            dead.append(name)
    return dead


def test_every_test_function_is_called():
    """A test that is compiled but never invoked is a silent gap in coverage."""
    findings = []
    for name in sorted(_registered_sources()):
        path = TESTS_DIR / name
        if not path.exists():
            continue
        for fn in _uncalled_in(path):
            if fn not in UNCALLED_OK:
                findings.append(f"{name}::{fn}")

    assert not findings, (
        "these test functions are defined in a suite that is built and "
        "registered, but nothing calls them, so they do not run and cannot "
        f"fail: {sorted(findings)}. Call them from main(), delete them, or "
        "add each to UNCALLED_OK with a reason."
    )


def test_uncalled_ok_has_no_stale_entries():
    """An exemption for a test that is now called hides the next real gap."""
    still_dead = set()
    for name in sorted(_registered_sources()):
        path = TESTS_DIR / name
        if path.exists():
            still_dead.update(_uncalled_in(path))
    stale = sorted(n for n in UNCALLED_OK if n not in still_dead)
    assert not stale, (
        f"these are called now and should leave UNCALLED_OK: {stale}"
    )
