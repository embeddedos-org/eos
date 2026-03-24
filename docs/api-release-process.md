# API Documentation Release Process

This document defines how API documentation is generated, versioned, and published for every EoS and eboot release.

---

## Automated API Docs Pipeline

### Doxygen Configuration (already in place)

EoS already has a `Doxyfile` at `eos/Doxyfile`. Every public header uses Doxygen-style comments (`@file`, `@brief`, `@param`, `@return`).

### Generate API Docs

```bash
# Generate HTML API docs for eos
cd eos/
doxygen Doxyfile
# Output: docs/api/html/index.html

# Generate HTML API docs for eboot
cd eboot/
doxygen Doxyfile
# Output: docs/api/html/index.html
```

### What Gets Documented

Every public header is automatically included:

**eos headers (auto-documented):**

| Header | Module | Functions |
|--------|--------|-----------|
| `eos/hal.h` | Core HAL | 20+ GPIO/UART/SPI/I2C/Timer functions |
| `eos/hal_extended.h` | Extended HAL | 100+ functions across 20 peripherals |
| `eos/kernel.h` | RTOS Kernel | 25+ task/mutex/semaphore/queue functions |
| `eos/multicore.h` | Multicore | 25+ SMP/AMP/spinlock/IPI functions |
| `eos/crypto.h` | Crypto | 15+ SHA/AES/RSA/CRC functions |
| `eos/ota.h` | OTA | 15+ firmware update functions |
| `eos/posix_*.h` | POSIX compat | 20+ pthread/sem/mq functions |
| `eos/eos_config.h` | Configuration | 39 EOS_ENABLE_* flags |

**eboot headers (auto-documented):**

| Header | Module | Functions |
|--------|--------|-----------|
| `eos_bootctl.h` | Boot control | A/B slot management |
| `eos_image.h` | Image verify | Signature verification |
| `eos_fw_update.h` | Firmware update | Stream-based OTA pipeline |
| `eos_multicore.h` | Multicore boot | SMP/AMP/lockstep |
| `eos_board_config.h` | Board config | Declarative pin/memory/clock macros |
| `eos_crypto_boot.h` | Boot crypto | SHA-256, CRC-32 |
| + 18 more | | |

---

## Release Versioning

### Semantic Versioning (SemVer)

```
MAJOR.MINOR.PATCH

MAJOR — Breaking API changes (function signature changes, removed functions)
MINOR — New API additions (new functions, new peripherals, new modules)
PATCH — Bug fixes, documentation updates (no API changes)
```

### Version Tags

```bash
# Tag a release
git tag v1.0.0
git push origin v1.0.0
```

### What Triggers API Doc Generation

| Event | Action |
|-------|--------|
| Push tag `v*.*.*` | CI generates + publishes versioned API docs |
| Push to `main` | CI generates `latest` API docs (development) |
| Pull request | CI checks that all public functions have Doxygen comments |

---

## Release Checklist

For every release, this checklist must be completed:

### 1. API Completeness Check

```bash
# Verify all public functions have Doxygen comments
cd eos/
doxygen Doxyfile 2>&1 | grep -c "warning"
# Should be 0 warnings

cd eboot/
doxygen Doxyfile 2>&1 | grep -c "warning"
# Should be 0 warnings
```

### 2. API Changelog

Every release MUST include an API changelog section in `CHANGELOG.md`:

```markdown
## [1.1.0] — 2025-03-01

### API Changes

#### Added
- `eos_gpio_set_drive_strength()` — configure GPIO drive current
- `eos_ble_set_mtu()` — set BLE MTU size
- `EOS_ENABLE_RADAR` flag for radar peripheral

#### Changed
- `eos_uart_read()` — added `flags` parameter (backward compatible with default 0)

#### Deprecated
- `eos_spi_write_blocking()` — use `eos_spi_write()` instead (removed in v2.0.0)

#### Removed
- None (would require MAJOR version bump)
```

### 3. Migration Guide (for MAJOR versions)

For any MAJOR version bump, create a migration guide:

```markdown
# Migrating from v1.x to v2.0

## Breaking Changes

### `eos_uart_read()` signature changed
- Old: `int eos_uart_read(uint8_t port, uint8_t *data, size_t len, uint32_t timeout_ms)`
- New: `int eos_uart_read(uint8_t port, uint8_t *data, size_t len, uint32_t timeout_ms, uint32_t flags)`
- Fix: Add `0` as the last argument to all calls

### `EOS_ENABLE_GPS` renamed to `EOS_ENABLE_GNSS`
- Find/replace in your `eos_config.h` or product profile
```

### 4. Generate and Publish

```bash
# Generate versioned docs
VERSION=$(git describe --tags)
cd eos && doxygen Doxyfile
mv docs/api/html docs/api/$VERSION

cd ../eboot && doxygen Doxyfile
mv docs/api/html docs/api/$VERSION
```

---

## API Stability Guarantees

| API Category | Stability | Breaking Changes Allowed? |
|-------------|-----------|--------------------------|
| Core HAL (`eos/hal.h`) | **Stable** | Only in MAJOR versions |
| Extended HAL (`eos/hal_extended.h`) | **Stable** | Only in MAJOR versions |
| Kernel (`eos/kernel.h`) | **Stable** | Only in MAJOR versions |
| Multicore (`eos/multicore.h`) | **Stable** | Only in MAJOR versions |
| Crypto (`eos/crypto.h`) | **Stable** | Only in MAJOR versions |
| OTA (`eos/ota.h`) | **Stable** | Only in MAJOR versions |
| POSIX compat (`eos/posix_*.h`) | **Stable** | Only in MAJOR versions |
| Internal headers (`_internal.h`) | **Unstable** | Any version |
| eboot public headers | **Stable** | Only in MAJOR versions |
| ebuild CLI commands | **Stable** | Only in MAJOR versions |
| ebuild Python API | **Beta** | MINOR versions allowed |

### Deprecation Policy

1. Functions are deprecated in a MINOR release with `__attribute__((deprecated))`
2. Deprecated functions remain for at least 1 MAJOR cycle
3. Removed only in the next MAJOR release

```c
/* Example deprecation */
__attribute__((deprecated("Use eos_spi_write() instead")))
int eos_spi_write_blocking(uint8_t port, const uint8_t *data, size_t len);
```

---

## Hosted Documentation

API docs are published to:

| URL | Content |
|-----|---------|
| `/docs/latest/` | Development (main branch) |
| `/docs/v1.0.0/` | Version 1.0.0 release |
| `/docs/v1.1.0/` | Version 1.1.0 release |

Generated automatically by CI on each tagged release.
