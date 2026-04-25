# Chapter 21: eNI — Embedded Neural Interface

*Author: Srikanth Patchava & EmbeddedOS Contributors*

---

## 21.1 Introduction

Brain-computer interfaces (BCIs) represent the next frontier of human-machine
interaction. The **eNI** (Embedded Neural Interface) provides a standardized,
vendor-neutral framework to integrate BCIs, neural decoders, and assistive
input systems into the EoS platform.

eNI bridges the gap between raw neural signals and actionable embedded system
commands — enabling applications from medical prosthetics to direct neural
control of drones and vehicles.

---

## 21.2 Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                    ENI Neural Interface                       │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐   │
│  │   Neuralink   │  │  Simulator   │  │  Generic/Custom  │   │
│  │  1024ch 30kHz │  │  Test data   │  │  Vendor-agnostic │   │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────────┘   │
│         │                  │                  │               │
│  ┌──────▼──────────────────▼──────────────────▼──────────┐   │
│  │              Provider Abstraction Layer                │   │
│  ├───────────────────────────────────────────────────────┤   │
│  │              Core Processing Pipeline                 │   │
│  │  Signal → Filter → FFT → Feature Extract → Classify  │   │
│  ├───────────────────────────────────────────────────────┤   │
│  │              Output: Commands / Decoded Intent        │   │
│  └───────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────┘
```

### Providers

eNI uses a provider abstraction to support multiple BCI hardware backends:

| Provider       | Channels | Sample Rate | Description                    |
|----------------|----------|-------------|--------------------------------|
| **Neuralink**  | 1024     | 30 kHz      | Implanted neural threads       |
| **Simulator**  | 64-1024  | 256 Hz      | Synthetic test data generation |
| **Generic**    | Variable | Variable    | Vendor-agnostic BCI devices    |

---

## 21.3 Core Processing Pipeline

The signal processing pipeline transforms raw neural data into actionable
commands through four stages.

### Stage 1: Signal Acquisition

Raw samples are read from the provider at the hardware sample rate. eNI
supports up to `ENI_MAX_CHANNELS` (1024) simultaneous channels.

### Stage 2: Digital Signal Processing

The DSP module applies configurable filter chains:

- **Bandpass filtering** — isolate EEG frequency bands
- **Notch filtering** — remove power-line interference (50/60 Hz)
- **FFT analysis** — frequency domain decomposition (up to 512-point FFT)

### EEG Frequency Bands

| Band      | Frequency Range | Associated State                |
|-----------|----------------|---------------------------------|
| **Delta** | 0.5 – 4.0 Hz  | Deep sleep                      |
| **Theta** | 4.0 – 8.0 Hz  | Drowsiness, meditation          |
| **Alpha** | 8.0 – 13.0 Hz | Relaxed, eyes closed            |
| **Beta**  | 13.0 – 30.0 Hz| Active thinking, focus          |
| **Gamma** | 30.0 – 100.0 Hz| High-level cognition, perception|

### Stage 3: Feature Extraction

Features extracted from the processed signals include:
- Band power ratios (alpha/beta for attention scoring)
- Event-related potentials (ERPs)
- Spike sorting (for invasive BCIs like Neuralink)
- Coherence between channel pairs

### Stage 4: Classification

The classifier maps features to discrete commands or continuous control
signals. eNI supports multiple classification backends:

- Rule-based thresholding (for simple use cases)
- Linear discriminant analysis (LDA)
- Neural network classifiers (via eAI integration)

---

## 21.4 Python SDK

eNI includes a Python SDK for rapid prototyping and research. The SDK
provides ctypes bindings to the native `libeni_common` library, with
fallback to pure-Python synthetic data generation.

### ENIProvider Class

```python
from eni.core import ENIProvider, EEG_BANDS

# Create a provider (simulator for development)
provider = ENIProvider(provider_type="simulator")
provider.connect()

# Read neural data
data = provider.read_data(num_samples=256)
print(f"Shape: {data.shape}")  # (256, 64)

# Compute band powers
powers = provider.compute_band_powers(data, sample_rate=256)
print(f"Alpha power: {powers['alpha']:.3f}")
print(f"Beta power: {powers['beta']:.3f}")

# Get attention score (beta/alpha ratio)
attention = powers['beta'] / powers['alpha']
print(f"Attention score: {attention:.2f}")

provider.disconnect()
```

### Key SDK Constants

```python
ENI_OK = 0
ENI_MAX_CHANNELS = 1024
ENI_DSP_MAX_FFT_SIZE = 512

EEG_BANDS = {
    "delta": (0.5, 4.0),
    "theta": (4.0, 8.0),
    "alpha": (8.0, 13.0),
    "beta":  (13.0, 30.0),
    "gamma": (30.0, 100.0),
}
```

### Library Loading

The Python SDK automatically searches for the native library in standard
locations. Platform-specific library names are resolved (`libeni_common.so`
on Linux, `libeni_common.dylib` on macOS, `eni_common.dll` on Windows).
If the native library is not found, the provider falls back to generating
synthetic neural data using NumPy — enabling development without hardware.

---

## 21.5 Native C API

The C API is designed for direct integration in embedded firmware:

```c
#include "eni/core.h"

// Initialize provider
eni_provider_t *provider = eni_provider_create(ENI_PROVIDER_NEURALINK);
eni_provider_connect(provider);

// Read data
float buffer[ENI_MAX_CHANNELS * 256];
size_t samples = eni_provider_read(provider, buffer, 256);

// Process with DSP
eni_dsp_config_t dsp = {
    .filter_type = ENI_FILTER_BANDPASS,
    .low_freq = 8.0f,
    .high_freq = 30.0f,
    .sample_rate = 30000.0f,
};
eni_dsp_process(buffer, samples, &dsp);

// Classify
eni_command_t cmd;
eni_classify(buffer, samples, &cmd);

// Cleanup
eni_provider_disconnect(provider);
eni_provider_destroy(provider);
```

---

## 21.6 EIPC Integration

eNI communicates with eAI through the eIPC framework. This enables neural
signals to drive AI inference:

```
eNI (Neural Input) --[eIPC]--> eIPC Server --[eIPC]--> eAI (AI Engine)
```

Enable eIPC integration at build time:

```bash
cmake -B build -DENI_EIPC_ENABLED=ON
```

---

## 21.7 Build Configurations

eNI supports flexible build configurations:

```bash
# Full build (minimal + framework + Neuralink provider)
cmake -B build -DENI_BUILD_TESTS=ON
cmake --build build

# Minimal only (MCU targets)
cmake -B build -DENI_BUILD_MIN=ON -DENI_BUILD_FRAMEWORK=OFF

# Without Neuralink provider
cmake -B build -DENI_PROVIDER_NEURALINK=OFF

# Cross-compile for ARM
cmake -B build-arm \
  -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
  -DCMAKE_SYSTEM_NAME=Linux
cmake --build build-arm
```

---

## 21.8 Applications

| Domain            | Application                                     |
|-------------------|-------------------------------------------------|
| **Medical**       | Prosthetic limb control, neural rehabilitation  |
| **Assistive**     | Communication aids for locked-in patients       |
| **Drone Control** | Direct neural flight control via eNI to eAI     |
| **Vehicle**       | Attention monitoring, drowsiness detection      |
| **Gaming/XR**     | Thought-based interaction in VR/AR              |
| **Industrial**    | Hands-free equipment operation                  |

---

## 21.9 Simulator Mode

For development without BCI hardware, eNI includes a full-featured
simulator that generates statistically representative neural data:

```python
provider = ENIProvider(provider_type="simulator")
provider.connect()

# Simulator generates realistic synthetic EEG data
data = provider.read_data(num_samples=1024)

# Features: realistic frequency band distributions,
# configurable noise levels, optional event injection
```

The simulator enables full pipeline development and testing without
any BCI hardware.

---

## 21.10 Summary

eNI provides the neural interface layer that connects brain-computer
interfaces to the EoS embedded platform. Its provider abstraction supports
everything from research-grade Neuralink implants to consumer EEG headsets,
while the processing pipeline handles the complex signal processing needed
to extract actionable commands from raw neural data.

**Key takeaways:**

- Vendor-neutral BCI framework with Neuralink, Simulator, and Generic providers
- Complete DSP pipeline: filtering, FFT, feature extraction, classification
- Python SDK for rapid prototyping with NumPy integration
- Native C API for embedded deployment (as small as 32 KB Flash)
- eIPC integration for neural-to-AI communication
- Supports up to 1024 channels at 30 kHz sample rate

---

*Next: Chapter 22 — EoSim Simulation Platform*
