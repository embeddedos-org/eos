---
adr: 16
title: Board and feature description via devicetree and Kconfig
status: Proposed
date: 2026-08-28
deciders: Architecture Council (§33), Build & Tooling maintainers
source: EmbeddedOS Platform Architecture & Ecosystem Design Document v0.9, §7.1, §9, §18, §31
---

# ADR-016 — Board and feature description via devicetree and Kconfig

## Context

Two bespoke description formats exist today, and both are load-bearing.

**Board description — `boards/*.yaml`.** A format only this project reads. Nothing a
vendor publishes can be imported into it, so every board is authored by hand. See ADR-015
for what that has produced.

**Feature selection — `EOS_ENABLE_*` preprocessor defines.** The top-level `CMakeLists.txt`
carries a comment explaining that these must be set before any `add_library()` call,
because a test defining a flag for itself is not enough — the flag has to reach the library
translation unit too. That comment is a description of a configuration system that does not
exist yet.

**Observed:** `core/src/config.c`, the hand-written parser for the project's own YAML
subset, is the source of two duplicate open PRs (#61, #65) for the same out-of-bounds
write. Its regression test **segfaults** on unfixed `master` (**Verified** — the test from
PR #61 run against `origin/master` exits 139). A bespoke format costs a bespoke parser, and
a bespoke parser costs memory-safety defects.

## Decision

1. **Devicetree source is the board description format.** DTS in, compiled to DTB,
   consumed via ADR-013's libfdt. Vendor-published DTS for an SoC or eval board becomes
   importable rather than transcribable.
2. **`boards/*.yaml` becomes a generated artifact**, derived from DTS, not hand-maintained
   truth. It may continue to exist for tooling that reads it.
3. **Kconfig replaces the `EOS_ENABLE_*` defines**, using `kconfiglib` at build time.
   `ebuild configure` and EoStudio's board configurator render the Kconfig tree rather than
   each modelling features their own way — which is §4's "do not duplicate build systems
   between eBuild and EoStudio" applied to configuration.
4. **`core/src/config.c` keeps its current scope** (project/package manifests) and is not
   extended. PR #61 should still merge: the OOB write is real today and this ADR does not
   land tomorrow.

## Consequences

- Board bring-up changes from authoring to importing. This is the single largest lever on
  multi-vendor reach in this ADR set.
- Contributors who know Zephyr find a familiar configuration model, which lowers the cost
  of contributing a board.
- A build-time Python dependency (`kconfiglib`) enters the toolchain. `ebuild doctor` must
  check for it and say so plainly when it is missing.
- The 88 board profiles must be triaged: import-backed, hand-written, or placeholder.

## Open

- Whether EoS defines its own DT bindings or reuses Zephyr's where they match. Reuse is
  cheaper and interoperable; divergence may be unavoidable for EoS-specific nodes.
- Migration path for the boards already described in YAML. **Unknown** until the triage
  above is done.
