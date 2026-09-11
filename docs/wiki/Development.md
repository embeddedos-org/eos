# Development

## Contribution source of truth

[CONTRIBUTING](https://github.com/embeddedos-org/eos/blob/master/CONTRIBUTING.md)

Before proposing a change, also review the [README](https://github.com/embeddedos-org/eos/blob/master/README.md). Keep changes scoped, add tests appropriate to the affected behavior, and follow the repository's current automation and review requirements.

## Build and dependency inputs found

`CMakeLists.txt`, `Dockerfile`, `backends/CMakeLists.txt`, `cmd/eos/CMakeLists.txt`, `core/CMakeLists.txt`, `debug/CMakeLists.txt`, `drivers/devicetree/CMakeLists.txt`, `examples/ble-sensor/CMakeLists.txt`, `examples/blink-gpio/CMakeLists.txt`, `examples/hmi-panel/CMakeLists.txt`, `examples/multicore-amp/CMakeLists.txt`, `examples/multitask-rtos/CMakeLists.txt`, and 31 more.

## Tests found in the default-branch tree

`products/vbox_test.h`, `tests/CMakeLists.txt`, `tests/__init__.py`, `tests/assertions_enabled.h`, `tests/functional/__init__.py`, `tests/functional/test_functional_e2e.py`, `tests/functional/test_gps_driver.py`, `tests/fuzz/CMakeLists.txt`, `tests/fuzz/fuzz_aes.c`, `tests/fuzz/fuzz_devicetree.c`, `tests/fuzz/fuzz_ota_header.c`, `tests/fuzz/fuzz_sha256.c`, and 61 more.

## Documented test commands

These commands are reproduced from the inspected root README or contributing guide:

```bash
cmake -B build/host -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build/host --parallel
```

```bash
cmake -B build/arm -G Ninja \
```

```bash
cmake --build build/arm --parallel
```

```bash
cmake -B build/host -DEOS_BUILD_TESTS=ON -DEOS_PRODUCT=vbox_test
```

```bash
ctest --test-dir build/host --output-on-failure
```

```bash
cmake -B build -DEOS_BUILD_TESTS=ON -DEOS_PRODUCT=vbox_test
```

```bash
cmake --build build
```

## Verification baseline

This inventory comes from `master` at [`d14b62f62d2e`](https://github.com/embeddedos-org/eos/commit/d14b62f62d2ed7a0393d1d99e433949cdca76973) and found 73 test-related paths among 796 files. Re-check the source tree when that commit is no longer current.
