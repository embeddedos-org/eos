# EoS Hardware Schemas

Machine-readable hardware vocabulary used by ebuild's EoS AI analyzer.
These schemas define the **contract** between eos (hardware knowledge) and
ebuild (build orchestration).

## Files

| Schema | Purpose | Consumed By |
|---|---|---|
| `board.schema.yaml` | Board definition format — MCU, arch, core, memory, peripherals, features | ebuild `EosConfigGenerator` |
| `hal_map.yaml` | Maps 22 HAL interfaces to headers, APIs, enable flags, pin types | ebuild `EosHardwareAnalyzer` |
| `peripherals.yaml` | 20 peripheral types with use cases, common chips, product profile mapping | ebuild `EosHardwareAnalyzer` |

## How ebuild uses these schemas

```bash
# ebuild reads eos schemas for accurate hardware analysis
ebuild analyze --file board.kicad_sch --eos-schemas ../eos/schemas/

# ebuild validates generated board.yaml against board.schema.yaml
ebuild generate-boot boot.yaml --output-dir _generated/
```

## Integration flow

```
Hardware docs → ebuild EoS AI → reads eos/schemas/* → generates:
  ├── board.yaml        (eos board definition)
  ├── boot.yaml         (eboot flash layout)
  ├── build.yaml        (ebuild project config)
  └── eos_product_config.h (C header with EOS_ENABLE_* flags)
```

## Schema coverage

### board.schema.yaml
- **6 architectures:** arm, arm64, riscv32, riscv64, xtensa, x86_64
- **19 core types:** cortex-m0 through cortex-a72, lx6/lx7, rv32/rv64
- **10 vendors:** ST, Nordic, Espressif, NXP, TI, Microchip, RPi, SiFive, Intel, AMD
- **32 peripheral types:** uart through flash_ctrl
- **9 features:** fpu, dsp, mpu, trustzone, crypto_accelerator, etc.

### hal_map.yaml
- **22 HAL interfaces** with API prefix, header, enable flag, function list
- Maps EE concepts (UART TX/RX, SPI MOSI/MISO) to EoS HAL calls

### peripherals.yaml
- **20 peripheral types** with category, typical use cases, bus requirements
- **10 product profiles** mapped to required peripherals
- Common chip references for sensor, wireless, and storage ICs
