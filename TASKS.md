<!-- generated: eos-ai-scaffold -->
# Tasks

Working ledger for `eos`. The planner writes entries; each owning role
updates its own row. Roles are in [AGENTS.md](./AGENTS.md), the workflow in
[ORCHESTRATION.md](./ORCHESTRATION.md), the gate in [VERIFY.md](./VERIFY.md).

Status is one of: `todo`, `in-progress`, `blocked`, `review`, `done`.

## Active

| ID | Task | Owner | Mode | Status | Depends on |
|----|------|-------|------|--------|------------|
| T-005 | Remove 1430 committed CMake build artifacts (`build_coverage` 691, `build_qemu_arm64` 371, `build_sim` 368) and gitignore them | architect | build | todo | none |
| T-006 | Decide the fate of `tests_backup/` (1 tracked file) | architect | build | todo | none |
| T-007 | Add coverage instrumentation, or drop `-DENABLE_COVERAGE` and the codecov upload from `ci.yml` | backend | build | todo | none |
| T-008 | Resolve `snprintf` truncation warnings in `systems/src/firmware.c` | backend | build | todo | none |

## Completed

| ID | Task | Owner | Verified by | Evidence |
|----|------|-------|-------------|----------|
| T-001 | Make `ci.yml` actually build the C tests | backend | reviewer | `CMakeLists.txt:18` declares `EOS_BUILD_TESTS`; `ci.yml` passed `-DBUILD_TESTS=ON`, a different variable. Reproduced CI's exact flags: configure reported `Tests: OFF`, build exit 0, `ctest` printed `No tests were found!!!` and **exited 0**. CI was green while running zero C tests. |
| T-002 | Fail CI when no tests are found | backend | reviewer | Added `--no-tests=error`. Against the old broken config `ctest` now exits **8**; against the fixed config it exits **0**. Both verified. |
| T-003 | Make the test build link | architect | reviewer | 7 of 19 targets failed to link (`test_power`, `test_multicore`, `test_net`, `test_ota`, `test_sensor`, `test_filesystem`, `test_motor_ctrl`) — module sources are wrapped in `#if EOS_ENABLE_<MODULE>` and `eos_config.h` defaults each to 0, so with no product profile they compiled to empty translation units. A test `#define`-ing the flag only affects its own TU. Flags now set before the first `add_library()`. Result: build exit 0, **19/19 tests passed**. |
| T-004 | Replace unverifiable README claims | docs | reviewer | Static `Status-Production Ready`, `Build-Passing`, `Coverage-100%` badges replaced with real workflow badges. "100% test coverage" removed — there is no coverage instrumentation in `CMakeLists.txt`, so no figure can be produced. The Zephyr/FreeRTOS/Linux benchmark claim removed — no comparative benchmark exists in this tree. |

---

## Task template

```markdown
### T-000 — <short title>

Owner: <role>
Mode: <see MODES.md>
Status: todo
Depends on: <task ids, or none>

Goal
: <one sentence: what is true afterwards that is not true now>

Acceptance criteria
: - <observable, checkable statement>
  - <observable, checkable statement>

Files in scope
: <paths the owner is expected to touch>

Out of scope
: <what this task deliberately does not change>

Risks
: <what could break, and what would reveal it>

Verification
: | Check | Command | Result |
  |-------|---------|--------|
  | <name> | `<command>` | `NOT RUN` |
```

## Verification commands for this repository

These commands were derived from the manifests at the repository root. Confirm one works before relying on it; a listed script may still be a stub.

| Check | Command | Default state |
|-------|---------|---------------|
| Build | `cmake --build build -j` | `NOT RUN` |
| Unit tests | `ctest --test-dir build --output-on-failure` | `NOT RUN` |

## Rules

- One task per unit of work that can be verified on its own.
- Acceptance criteria are written before work starts and are not edited to match
  what was built. If they were wrong, say so and rewrite them explicitly.
- A task reaches `done` only when the definition of done in
  [ORCHESTRATION.md](./ORCHESTRATION.md) is met and the verification commands
  were actually run.
- `blocked` requires a note naming what it is blocked on and who can unblock it.
