---
adr: 18
title: Test framework and machine-readable CI results
status: Proposed
date: 2026-08-28
deciders: Architecture Council (§33), QA maintainers
source: EmbeddedOS Platform Architecture & Ecosystem Design Document v0.9, §26, §31
---

# ADR-018 — Test framework and machine-readable CI results

## Context

**Observed:** the suite is hand-rolled `assert()` and `printf` per file, with a
`main()` that prints a hard-coded total such as `ALL KERNEL TESTS PASSED (9/9)`. Three
open PRs each edit that literal to a different number. A failing assertion aborts the
process and reports nothing structured.

**Verified — enumerated against `origin/master`, 28 Aug 2026:** **13 test files exist in
`eos/tests/` that no `CMakeLists.txt` registers**, so they are never compiled and never
run:

`main_system_test.c`, `test_board_configs.c`, `test_crypto_failclosed.c`,
`test_driver_recovery.c`, `test_e2e.c`, `test_emulation_simulation.c`, `test_firmware.c`,
`test_hal_stm32f4.c`, `test_net_integration.c`, `test_net_mock.c`,
`test_performance_benchmarks.c`, `test_profiles.c`, `test_stress.c`.

Three of those — `test_e2e.c`, `test_net_integration.c`, `test_hal_stm32f4.c` — *are*
compiled, by hand-written `gcc` lines in `build.yml`, outside CMake entirely. So the
project has two parallel test systems and neither knows about the other.

`tests/fuzz/` is likewise never added by any `add_subdirectory` (**Verified** — zero
references in either `CMakeLists.txt` on `master`), so four fuzz targets have never been
built.

Dark tests are worse than absent ones: they make coverage look larger than it is. Note
that `test_crypto_failclosed.c` is among them — the test that pins the secure-boot stub
fail-closed behaviour ADR-011 documents is not run by `ctest`.

## Decision

1. **Adopt Unity** (MIT, **Inferred**) as the C test framework, with **CMock** where
   driver-level mocking is needed.
2. **Emit JUnit XML from ctest** so CI can report per-test results rather than a pass/fail
   process exit.
3. **Registration is enforced.** A CI check fails when a `tests/**/*.c` file matching the
   test-file convention is not referenced by a `CMakeLists.txt`. A test that is not run is
   a defect, and this makes it one the build catches.
4. **The 13 unregistered files are triaged before this ADR is Accepted**: registered and
   made to pass, or deleted with the reason recorded. They are not left as they are. The
   three compiled by `build.yml` move into CMake so there is one test system, not two.
5. **Migration is incremental.** New tests use Unity; existing files migrate as they are
   touched. A flag day across ~24 test binaries is not justified.

## Consequences

- Per-board and per-architecture CI matrices (§26) become reportable, because results
  become data rather than console text.
- Registering the 13 files may reveal failures. That is the point; ADR-012 needs to know
  what `test_crypto_failclosed.c` actually reports before it starts, not after.
- Unity enters `third_party/` under ADR-017 like any other import.

## Open

- Whether the fuzz targets run in CI on every PR or nightly. Nightly is the usual answer,
  but the devicetree target is cheap enough to run per-PR.
- Whether `run_all_tests.py` and its pytest suites fold into ctest or stay separate.
