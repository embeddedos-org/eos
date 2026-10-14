# EosConfig options indirection — design for the #148 first shrink step

Companion to `docs/eosconfig-size-inventory.md`. The inventory named
package `options` as indirection step 1 (2,621,440 of the 3,953,236
bytes). This note records why that step cannot land as a naive struct
edit, and the accessor-first sequence that makes it shippable in
daily-sized commits. The size guard
(`tests/test_config_size.c`, ceiling 3,960,000) is unchanged by this
note; the ceiling ratchets down in the commit that changes the storage.

## Why the naive edit is unsafe (verified 2026-10-14)

- **Two structs embed the array.** `EosPackageConfig`
  (`core/include/eos/config.h`) and `EosPackage`
  (`include/eos/package.h`) both carry
  `EosKeyValue options[EOS_MAX_OPTIONS]` plus `deps[64]`, and
  `eos_package_set_from_config()` copies one into the other.
  `EosPackageSet` embeds 128 `EosPackage` — the same ~4 MB problem a
  second time. Any storage change must cover both structs and the
  conversion boundary, or the shrink is cancelled by the copy.
- **The parser's cap is load-bearing and invisible.**
  `eos_config_load` (`core/src/config.c`) zeroes
  `packages[pkg_idx].options` wholesale at every `[package]` header
  (because `option_count` is re-initialised there) and drops options
  beyond the cap silently. Real configs therefore cannot be audited
  for "how many options are actually used" from behaviour — a config
  that declares 40 options loads as 32 with no diagnostic.
- **Consumers iterate the array directly.** Eleven build backends
  (`backends/src/*_backend.c`), the scheduler / cache-key code
  (`core/src/scheduler.c`, `tests/test_scheduler_cache_key.c`), and
  `pkg/src/package.c` read `options[i].key/.value` inline. There is
  no accessor to hide a storage change behind today.

## Step 0 (prerequisite, behaviour-preserving): an options accessor

Introduce, in `core/include/eos/config.h` + `core/src/config.c`:

```c
int eos_package_option_count(const EosPackageConfig *pkg);
const EosKeyValue *eos_package_option_at(const EosPackageConfig *pkg, int i);
const char *eos_package_option_get(const EosPackageConfig *pkg, const char *key);
```

with the `EosPackage` twins in `include/eos/package.h` /
`pkg/src/package.c`. Migrate the backends, scheduler, and package
code to the accessors in the same step (mechanical, one commit per
consumer group if needed). No struct layout changes; the size guard
must pass at the unchanged ceiling. This step also fixes the
auditability gap: the parser gains a diagnostic when an option is
dropped at the cap (count them, report at load), so real option
usage becomes measurable before any cap or storage decision.

## Step 1: storage indirection (the actual shrink)

- Add a per-`EosConfig` string table (arena): a growable
  `char *` buffer owned by the config, freed with it; option keys
  and values are interned into it.
- `EosPackageConfig.options` becomes `{ uint32_t options_off;
  uint16_t option_count; }` — offset/count into the table, with each
  entry stored as two interned offsets (key, value). The accessor
  from step 0 materialises an `EosKeyValue`-shaped view, so migrated
  consumers do not change again.
- Apply the identical treatment to `EosPackage` at the
  `eos_package_set_from_config()` boundary: the set interns into its
  own table instead of copying 32 fixed slots per package.
- Expected effect at today's usage shape: options storage falls
  from 2,621,440 bytes (packages) plus the `EosPackageSet` copy to
  the size of the options actually declared — the guard ceiling is
  ratcheted to the new measured `sizeof(EosConfig)` in the same
  commit, per the ratchet rule.

## Step 2 and beyond

`deps[64][128]` interning (inventory step 2) reuses the same table
and accessor pattern (`eos_package_dep_at`). The `packages[128]`
arena allocation (inventory step 3) is a separate design; it
changes `EosConfig`'s own storage, not a member family's.

## Non-goals for this design

- No change to `EOS_MAX_OPTIONS` as a *declared* limit: with
  indirection the cap stops costing 640 bytes × 128 packages whether
  or not anyone uses it, so the silent-truncation question becomes a
  parser-diagnostics question (step 0), not a sizing one.
- No MCU/host struct split yet: the table is pointer-based, so a
  future fixed-arena MCU variant can reuse the offset/count shape
  with a static table — noted here so the offsets are never assumed
  to be host pointers by consumers (accessor-only access is the rule
  that keeps that door open).
