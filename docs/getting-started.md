# 🚀 EoS — Getting Started

## Prerequisites

- **CMake** ≥ 3.16
- **C compiler** (GCC, Clang, or MSVC)
- **Ninja** (recommended) or Make

## Building EoS

```bash
# Clone the repository
git clone https://github.com/embeddedos-org/eos.git
cd eos

# Configure and build
cmake -B build -G Ninja
cmake --build build

# Run tests (optional)
cmake -B build -DEOS_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

## Your First Project

### 1. Create a project directory

```bash
mkdir my-embedded-project
cd my-embedded-project
```

### 2. Create `eos.yaml`

```yaml
project:
  name: my-project
  version: 0.1.0

workspace:
  backend: ninja
  build_dir: .eos/build

toolchain:
  target: aarch64-linux-gnu

packages:
  - name: zlib
    version: 1.2.13
    build:
      type: cmake

system:
  kernel:
    provider: kbuild
  rootfs:
    provider: eos
    init: busybox
    hostname: my-device
```

### 3. Build

```bash
# Build all packages
eos build

# Build complete OS image
eos system

# Preview what would happen (dry run)
eos system --dry-run
```

### 4. View project info

```bash
eos info
```

## CLI Commands

| Command | Description |
|---------|-------------|
| `eos build` | Build all packages |
| `eos system` | Build complete OS image |
| `eos add <pkg>` | Add a package |
| `eos clean` | Remove build artifacts |
| `eos info` | Show project details |

## Cross-Compilation

EoS supports three architectures out of the box:

- **ARM64** (`aarch64-linux-gnu`)
- **x86_64** (`x86_64-linux-gnu`)
- **RISC-V 64** (`riscv64-linux-gnu`)

Set your target in `eos.yaml`:

```yaml
toolchain:
  target: aarch64-linux-gnu
```

## Next Steps

- Read the [Architecture Guide](architecture.md)
- Read the [UI / LVGL Integration Guide](ui-guide.md)
- Explore the [demo-linux example](../examples/demo-linux/)
- Check the [board definitions](../boards/) for supported targets
