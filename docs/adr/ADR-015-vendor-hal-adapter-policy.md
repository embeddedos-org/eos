---
adr: 15
title: Vendor HAL adapter policy
status: Proposed
date: 2026-08-28
deciders: Architecture Council (§33), HAL & BSP maintainers
source: EmbeddedOS Platform Architecture & Ecosystem Design Document v0.9, §3, §7.1, §22, §31
---

# ADR-015 — Vendor HAL adapter policy

## Context

**Observed:** `hal/src/hal_stm32f4/` contains hand-written GPIO, UART, SPI, I²C, ADC, RCC
and timer drivers — register-level code derived from a reference manual. It is the only
vendor family implemented to that depth.

**Observed:** `boards/` holds 88 YAML profiles, of which the large majority are named
`generic-<arch>` — `generic-arm7tdmi`, `generic-avr32`, `generic-c166`, and so on. Breadth
in the file listing; almost none of it is a real port.

The two observations have the same cause. Supporting a part currently means re-deriving a
vendor's peripheral programming by hand, so the marginal cost of each new part is measured
in weeks and nothing gets past the placeholder stage.

Silicon vendors already publish, maintain and test that layer: CMSIS-Core headers plus
STM32Cube HAL, nrfx, ESP-IDF, MCUXpresso, TI MCU+SDK.

## Decision

1. **The EoS HAL API does not change.** `hal/include/eos/hal.h` remains the contract that
   drivers and applications compile against.
2. **Below it, EoS adapts rather than reimplements.** Each supported family gets a thin
   adapter translating the EoS HAL onto CMSIS-Core plus that vendor's own SDK.
3. **Register-level drivers are written only where no vendor SDK exists**, and that
   exception is recorded in the board profile.
4. **The existing STM32F4 implementation is not deleted.** It becomes the reference against
   which the first STM32Cube adapter is differentially tested — the same peripheral, two
   implementations, compared on hardware. Once the adapter passes, the hand-written version
   is removed in a separate commit.
5. **A board profile may not claim support without a passing CI job.** `boards/*.yaml`
   entries without one are marked `status: placeholder` in the same PR that merges this
   ADR.

## Consequences

- Adding a vendor becomes adapter work rather than a port, which is what makes the §31
  goal of real reference boards reachable.
- Vendor SDK licences enter the dependency tree (Apache-2.0 and BSD-3 typically;
  **Inferred**, and some vendor SDKs carry use-restriction clauses that must be read
  individually). ADR-017 governs each import.
- Build complexity rises: each family brings its own headers and startup code, and `ebuild`
  must fetch them per target.
- The 88-board list will shrink, visibly, when placeholders are labelled. That is the
  intended outcome — an honest matrix is worth more to an evaluating vendor than a long one.

## Open

- Whether adapters live in `eos` or in per-vendor repositories. §22's split policy
  (ADR-002) says not to split before an independent release lifecycle exists.
- How vendor SDKs are pinned and fetched — ADR-017 and ADR-019 share this mechanism.
