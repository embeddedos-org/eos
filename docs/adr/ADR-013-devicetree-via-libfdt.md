---
adr: 13
title: Flattened device tree parsing via libfdt
status: Proposed
date: 2026-08-28
deciders: Architecture Council (§33), Kernel & Drivers maintainers
source: EmbeddedOS Platform Architecture & Ecosystem Design Document v0.9, §7.1, §26
---

# ADR-013 — Flattened device tree parsing via libfdt

## Context

`drivers/devicetree/devicetree.c` is a hand-written FDT walker. A device tree blob is
untrusted input: it arrives from a prior boot stage or a flash region, and every offset and
length inside it is chosen by whoever produced it.

**Observed:** `eos` PR #50 is currently re-deriving, by hand, the bounds checks that a
standard FDT library has had for years — validated `totalsize`, per-token bounds, NUL
termination inside the blob, a nesting-depth cap. The PR is careful and well documented.
It is also work the project should not have to do, review, or maintain.

Every Linux, U-Boot, Zephyr and Barebox target already produces and consumes FDT blobs.
The format is not ours; the parser should not be either.

## Decision

1. **Adopt `libfdt`** (from the `dtc` project) as the FDT parser for both `eos` and
   `eBoot`, taking the **BSD-2-Clause** arm of its dual licence. **Inferred** — confirm the
   dual-licence grant at import and record it per ADR-017.
2. `drivers/devicetree/devicetree.h` stays as the EoS-facing API; the `.c` becomes a shim
   over libfdt and the hand-written walker is deleted.
3. **The test corpus from PR #50 is kept and retargeted.** Its truncation sweep and
   single-byte-corruption sweep are valuable regardless of what sits underneath, and they
   become the acceptance test for the shim. The fuzz target it enables stays enabled.

## Consequences

- PR #50's parser changes become unnecessary. Its tests and its `tests/fuzz` wiring do not
  — that wiring fixes a directory no `add_subdirectory` had ever included, which is worth
  merging on its own regardless of this ADR's outcome.
- libfdt is read-oriented and allocation-free, which suits the Nano profile. Footprint is
  **Unknown** here and must be measured alongside ADR-012's crypto budget.
- Consuming vendor-published DTS files becomes possible, which is the precondition for
  ADR-016.

## Open

- Whether `eBoot` needs the full libfdt or only `fdt_ro.c`.
- Whether to also adopt `dtc` itself as a build-time dependency for compiling DTS → DTB, or
  to require a pre-built blob. ADR-016 depends on the answer.
