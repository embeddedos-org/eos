# EosConfig options migration plan — the #148 build sequence

Companion to `docs/eosconfig-size-inventory.md` (where the bytes are)
and `docs/eosconfig-options-indirection.md` (the accessor-first
design, landed 2026-10-14). Step 2's go/no-go evidence pack —
accessor audit, lifetime position, predicted size, slice split — is
`docs/eosconfig-step2-scoping.md` (2026-10-21). The design note says *what* changes and
why a naive struct edit is unsafe; this plan is the *sequence* the
daily loop executes it in — each step sized for one commit, each
with its own verification gate, so the shrink lands incrementally
instead of as one cross-cutting rewrite.

Baseline (unchanged until Step 2): `sizeof(EosConfig)` is 3,953,236
bytes; the guard in `tests/test_config_size.c` holds the ceiling at
3,960,000 and may only ratchet **down**, in the same commit that
changes the storage it measures.

## Step 0a — accessors for `EosPackageConfig` (no layout change)

- Add `eos_package_option_count`, `eos_package_option_at`, and
  `eos_package_option_get` for `EosPackageConfig` in
  `core/include/eos/config.h` + `core/src/config.c`, per the design
  note's signatures.
- No consumer migration yet, no struct change.
- Gate: full CI green; new unit coverage for the three accessors
  (count, in-range `at`, out-of-range `at` returns NULL, `get` hit
  and miss) in the config tests.

## Step 0b — accessors for `EosPackage` (no layout change)

- Add the twin accessors for `EosPackage` in
  `include/eos/package.h` + `pkg/src/package.c`.
- `eos_package_set_from_config()` switches to the Step 0a accessors
  for its copy loop — the conversion boundary is accessor-mediated
  from here on, so Step 2 changes one side at a time.
- Gate: full CI green; package tests unchanged and passing.

## Step 0c — migrate direct readers, group by group

- Migrate the direct `options[i].key/.value` readers named in the
  design note — the build backends (`backends/src/*_backend.c`),
  then scheduler / cache-key code (`core/src/scheduler.c`,
  `tests/test_scheduler_cache_key.c`) — one consumer group per
  commit, each mechanical (array index → `option_at`, key lookup →
  `option_get`).
- Add the parser diagnostic the design note calls for: when
  `eos_config_load` drops an option at the `EOS_MAX_OPTIONS` cap,
  count it and report the count at load, so real option usage is
  measurable before Step 2.
- Gate per commit: full CI green; no behaviour change (the
  diagnostic is the only new output, and only on cap overflow).

## Step 2 — storage indirection (the shrink)

- Per the design note: a per-`EosConfig` string table (arena) owns
  interned option keys/values; `EosPackageConfig.options` becomes
  `{ uint32_t options_off; uint16_t option_count; }` with each entry
  stored as two interned offsets; the Step 0 accessors materialise
  the `EosKeyValue`-shaped view so migrated consumers do not change
  again. The identical treatment applies to `EosPackage` at the
  `eos_package_set_from_config()` boundary (its own table in
  `EosPackageSet`).
- This is the only step allowed to be larger than a daily slice if
  the two-struct coupling forces it — and the only step that moves
  the ceiling.
- Gate: full CI green **and** the ceiling in
  `tests/test_config_size.c` ratcheted down to the new measured
  `sizeof(EosConfig)` in the same commit. Expected effect at
  today's usage shape: options storage falls from 2,621,440 bytes
  (plus the `EosPackageSet` copy) to the size of the options
  actually declared.

## After Step 2

- Inventory step 2 (`deps[64][128]` interning, ~1.0 MB) reuses the
  same table and accessor pattern (`eos_package_dep_at`).
- Inventory step 3 (the `packages[128]` arena) is a separate
  design; it changes `EosConfig`'s own storage, not a member
  family's.
- `EOS_MAX_OPTIONS` stays as a *declared* limit: with indirection
  the cap no longer costs 640 bytes × 128 packages whether or not
  anyone uses it (design note, Non-goals).

## What this plan is not

- Not a commitment to land Step 2 in one day: Steps 0a–0c are the
  daily-sized slices, and any step that cannot be verified green
  stops the sequence rather than forcing the next one.
- No MCU/host struct split: the offset/count shape keeps that door
  open (accessor-only access is the rule that preserves it).
