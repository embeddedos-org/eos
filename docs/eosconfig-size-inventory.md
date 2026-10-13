# EosConfig size inventory — the #148 shrink record

`EosConfig` (`core/include/eos/config.h`) is the build system's central
configuration struct. Issue #148 measured it at **3,953,236 bytes**
(host, LP64, 2026-10-13 — reproduced exactly by
`tests/test_config_size.c`), with five static instances totalling
~19 MB of `.bss`. No MCU target in the §25.1 set can hold one instance,
let alone five. This note is the design record for shrinking it: where
the bytes are, and the order the follow-up redesign moves them to
arena / string-table storage.

## The guard

`tests/test_config_size.c` (ctest: `test_config_size`) prints
`sizeof(EosConfig)` and fails above a ceiling of **3,960,000** bytes —
today's measured size rounded up slightly. The ceiling is a ratchet:
it may only move **down** in future commits, as the redesign lands.

## Where the bytes are

| Member | Shape | Bytes | Share |
|---|---|---:|---:|
| `packages[128]` (`EosPackageConfig`) | 128 × 30,032 | 3,844,096 | 97.2% |
| — per-package `options[32]` (`EosKeyValue`) | 32 × 640 | 20,480 / pkg | |
| — per-package `deps[64]` (`char[128]`) | 64 × 128 | 8,192 / pkg | |
| — per-package `source` (`char[1024]`) | 1 × 1,024 | 1,024 / pkg | |
| `system` (`EosSystemConfig`) | incl. `rtos[4]` × 21,772 | 88,512 | 2.2% |
| `layers[32]` (`char[512]`) | 32 × 512 | 16,384 | 0.4% |
| `docs` (`EosDocsConfig`) | five `char[512]` paths + names | 2,572 | |
| `workspace` (`EosWorkspaceConfig`) | two `char[512]` paths + name | 1,152 | |
| `project` + `toolchain` | `char[128]` names | 512 | |

The struct is dominated by fixed-width string arrays: `EOS_MAX_PATH`
is 512 and `EOS_MAX_NAME` is 128, multiplied by the per-package arrays
(128 packages × 32 options × (128-byte key + 512-byte value) is
2,621,440 bytes of option storage alone).

## Indirection order for the redesign (biggest first)

1. **Package options** (`EosPackageConfig.options`, 2.6 MB): move to a
   shared string table + per-package offset/count pairs. Most packages
   set few or no options; the 32-slot inline array is nearly all waste.
2. **Package deps** (`deps[64][128]`, 1.0 MB): intern dependency names
   in the same string table; store per-package index ranges.
3. **The `packages` array itself** (3.8 MB total): allocate per actual
   `package_count` from an arena instead of embedding 128 slots in
   every `EosConfig` instance.
4. **Path arrays** (`layers`, workspace/docs paths, per-RTOS
   `entry`/`output`): intern paths in the string table; a path stored
   once instead of per slot.
5. **RTOS configs** (`system.rtos[4]`, 87 KB): same arena treatment,
   after the package work — it is two orders of magnitude smaller.

Each step lands with the ceiling ratcheted down to the new measured
size in the same commit, so the guard proves the shrink and locks it in.
