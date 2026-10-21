# EosConfig Step 2 scoping — the evidence pack for the storage move (#148)

Companion to `docs/eosconfig-options-migration-plan.md` (the sequence)
and `docs/eosconfig-options-indirection.md` (the design). This note is
the Step 2 go/no-go evidence, gathered at HEAD `50a51821` on
2026-10-21 by an exhaustive read of the tree — not a sample. It
changes no code and no layout; the baseline stands unchanged:
`sizeof(EosConfig)` = 3,953,236 B, ceiling 3,960,000 in
`tests/test_config_size.c`, ratchet-down-only, in the commit that
changes the storage.

## (a) Accessor-surface audit — exhaustive, zero missed consumers

Every `options[` / `->options` / `.options` hit in `*.c` / `*.h` at
HEAD, classified:

| Site | Class |
| --- | --- |
| `core/src/config.c:378, 384–385` | accessor implementation (`EosPackageConfig`) |
| `pkg/src/package.c:197, 203–204` | accessor implementation (`EosPackage`) |
| `core/src/config.c:330–331` | writer — the config parser's options store |
| `pkg/src/package.c:38` | writer — the `eos_package_set_from_config()` conversion copy (already accessor-mediated on the read side, Step 0b) |
| `systems/src/firmware.c:160–161` | writer — the firmware board option (`options[0]` = board) |
| `tests/test_scheduler_cache_key.c:47–51` | test fixture writer (`EosPackage`) |
| `tests/test_shell_cmd.c:238–246` | test fixture writers (`EosPackage`) |
| struct definitions | `core/include/eos/config.h:35, 65`, `include/eos/package.h:27`, `pkg/include/eos/package.h:23` |

No other site exists. **Step 2 is not blocked on a further Step 0c
reader migration** — Steps 0a/0b/0c plus the 2026-10-20 scheduler
cache-key landing completed the reader surface. What remains is
writers and fixtures, which Step 2 itself must give a storage path
(a setter/materialise helper, or table-interning calls at the same
sites).

Two findings the original plan did not carry:

1. **A third options array.** `EosRtosConfig` (`config.h:65`) holds
   its own `options[EOS_MAX_OPTIONS]`, and `EosConfig.system.rtos`
   is an array of 4 (`EOS_MAX_RTOS`) — 81,920 B inside `EosConfig`.
   At HEAD it has **no accessor, no writer, and no reader** anywhere
   in the tree: the parser never stores into it and nothing reads
   it. It is dead storage. Recommendation: remove it in its own
   small commit (with its own ceiling ratchet) *before* Step 2, or
   explicitly scope it out in the Step 2 commit message — do not
   silently carry it into the table design.
2. **Two divergent `package.h` copies.** `include/eos/package.h`
   (full: accessor declarations, `EosPackageSet`, registry API) and
   `pkg/include/eos/package.h` (reduced: no accessor declarations)
   declare the same structs with the same layout today, by
   coincidence of maintenance, not by construction. Step 2 changes
   the layout; both copies must change identically in the same
   commit, and the scoping recommendation is to note the divergence
   in the Step 2 commit body so a future reader is not surprised.

## (b) Lifetime / ownership — the real design fork

Facts at HEAD:

- There is **no free/dispose function** for `EosConfig`. The whole
  API is `eos_config_init` (a whole-struct `memset` plus defaults),
  the loader in `core/src/config.c`, and `eos_config_dump`.
  `EosConfig` is caller-owned, and the tests hold it as
  `static EosConfig` (8+ sites in `tests/test_config.c` alone).
- The parser stores options in place (`config.c:330–331`), capped
  at `EOS_MAX_OPTIONS` with the Step 0c dropped-option diagnostic
  after the load loop. There is no per-`[package]` wholesale zeroing
  of a package slot at HEAD — a reload into the same struct without
  `eos_config_init` would append onto stale counts under today's
  layout too; the table design inherits that contract unchanged.

Position: **heap arena, lazily allocated, owned by the struct.**
Reasoning:

- A fixed in-struct arena sized for the worst case (128 packages ×
  32 options) reintroduces the size problem Step 2 exists to kill;
  sized for the measured case it truncates — and silent truncation
  is the bug class #148 already caught once (the dropped-option
  diagnostic exists precisely because a cap dropped data quietly).
- A heap pointer in a `static EosConfig` starts NULL, which the
  design can define as "empty table": `eos_config_init`'s memset
  already produces exactly that state, so no new init contract is
  needed for the common path. Interning allocates/grows the arena
  on first use; exhaustion fails the load loudly (section (c)).
- The cost is a new obligation: a long-lived caller that reloads
  configs needs a dispose entry point, because re-running
  `eos_config_init` on a live struct would orphan the arena.
  Step 2 should add `eos_config_dispose` (name illustrative) and
  call it from the loader's entry when a table is already present.
  Test statics that never dispose leak one arena per process —
  acceptable, bounded, and visible; hiding it would not be.

Measured usage, honestly stated: **the tree contains no production
config files at all** — no `.eos` files exist in the repo. The only
declared options are the parser test fixtures in
`tests/test_config.c` (three `options:` blocks, a handful of short
key/value pairs each) and the synthetic fixtures in
`test_scheduler_cache_key.c` (parameterised key/value lengths for
cache-key truncation cases, not real usage). There is therefore
**no tree data from which to size a fixed arena**, which is the
second reason the heap position wins. If a future step wants a
fixed-arena variant for MCU targets, the instrumentation to size
it is a usage counter beside the existing dropped-option
diagnostic (total interned bytes per load, logged at the same
`EOS_INFO` line) — proposed here, not built here.

## (c) The `EosPackageSet` side

- `eos_package_set_from_config()` (`pkg/src/package.c`) memsets the
  set, then copies each package's options through the Step 0a
  accessors (line 38). Under Step 2 the set needs its own table,
  interned in config order, in the same loop.
- Failure path: on table exhaustion the conversion must **fail the
  whole conversion** with an error result — never silently
  truncate a package's option list. The function already returns
  an `EosResult`; Step 2 wires the exhaustion case into it.
- `EOS_MAX_OPTIONS` is **32** (`core/include/eos/types.h:32`), so a
  `uint16_t option_count` is not a constraint — the cap is two
  orders of magnitude below the type's range.

Predicted new size — **arithmetic from the struct definitions, a
prediction, not a measurement:**

- `EosKeyValue` = 128 + 512 = 640 B; one `options[32]` = 20,480 B.
- `EosConfig` holds 128 × 20,480 = 2,621,440 B of package options,
  plus 4 × 20,480 = 81,920 B of (dead) RTOS options:
  2,703,360 B of the 3,953,236 B baseline is options arrays (68%).
- Step 2 (package options only, RTOS array untouched): each
  `EosPackageConfig` trades 20,480 B for `{uint32_t options_off;
  uint16_t option_count;}` (~8 B with padding) → −2,620,416 B,
  plus one table pointer + size pair on `EosConfig` (~16 B).
  **Predicted `sizeof(EosConfig)` ≈ 1,332,800 B ± alignment
  padding.** Removing the dead RTOS array in its own commit takes
  a further 81,920 B off whichever side of Step 2 it lands.
- `EosPackageSet` shrinks by the same 2,621,440 B on its side; it
  is not part of `sizeof(EosConfig)` and has no size guard today —
  Step 2 may optionally add one, in the same commit.

## (d) Test surface

Fixtures that construct or write the structs directly — the Step 2
breakage list:

- `tests/test_scheduler_cache_key.c` — `set_option()` writes
  `pkg->options[i]` directly (its expectations are computed from
  accessor reads since the 2026-10-20 migration, so the assertions
  stand; only the fixture writer needs the new storage path).
- `tests/test_shell_cmd.c` — two `EosPackage` literals written via
  `options[0]` + `option_count = 1`.
- `tests/test_package.c` — stack `EosPackage` values throughout
  (no direct options writes found; conversion-path coverage).
- `tests/test_config.c` — `static EosConfig` fixtures and the
  parser `options:` blocks (the lifetime case of section (b)).
- `tests/test_config_size.c` — the guard. Ceiling procedure, per
  its header: the ceiling moves **down only**, in the same commit
  as the storage change, to the new measured `sizeof`. A Step 2
  commit that does not touch the ceiling, or raises it, fails the
  guard by design.

## (e) Slice decomposition + go/no-go

Step 2 decomposes; the two-struct coupling does not force one
large commit, because the conversion boundary is already
accessor-mediated on the read side (Step 0b):

- **2a — `EosPackageConfig` side.** String table on `EosConfig`;
  parser interns; the Step 0a accessors materialise the
  `EosKeyValue` view; `eos_package_set_from_config()` still copies
  into the *old* `EosPackage` inline layout via those accessors.
  Ceiling ratchets to the measured 2a size in this commit.
- **2b — `EosPackage` side.** The set's own table; conversion
  interns instead of copying; fixture writers in
  `test_scheduler_cache_key.c` / `test_shell_cmd.c` move to the
  storage path 2a introduced. No `EosConfig` change.
- Separately, before or after: the dead `EosRtosConfig` options
  removal (section (a), finding 1), one small commit with its own
  ratchet.

**Go**, with this checklist for the executing run:

1. Re-run the section (a) grep at the executing HEAD; any new
   direct reader is a Step 0c slice first, not a Step 2 passenger.
2. Land the RTOS dead-array decision explicitly (remove, or scope
   out in writing in the 2a commit body).
3. Change both `package.h` copies identically (2b), or neither.
4. Exhaustion fails loudly on both sides (loader + conversion);
   add a test that fills a table past capacity and asserts the
   error, beside the dropped-option diagnostic's existing test.
5. Ratchet `tests/test_config_size.c` in the 2a commit to the
   measured size; quote predicted-vs-measured in the commit body.
6. Full CI green on each slice; no slice touches `EOS_MAX_OPTIONS`.

Refs #148.
