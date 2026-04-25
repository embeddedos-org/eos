# Chapter 20: eAI — Embedded AI Layer

*Author: Srikanth Patchava & EmbeddedOS Contributors*

---

## 20.1 Introduction

Artificial intelligence is no longer confined to the cloud. Modern embedded
systems — from autonomous vehicles to medical devices — demand on-device
inference with zero cloud dependency, deterministic latency, and safety
guarantees that cloud-based solutions cannot provide.

**eAI** is a C11 embedded AI framework that brings LLM inference, autonomous
agents, and adaptive learning directly to resource-constrained devices. It
runs entirely on-device with no cloud requirement, while supporting optional
hybrid connectivity for model updates and federated learning.

---

## 20.2 Design Principles

| Principle               | Description                                               |
|-------------------------|-----------------------------------------------------------|
| **Offline-first**       | Full autonomy without cloud connectivity                  |
| **Resource-aware**      | Quantized models, power-aware scheduling, memory-conscious|
| **Security by default** | 8-layer defense architecture from boot to runtime         |
| **Two-tier design**     | Choose the right footprint for your hardware              |

---

## 20.3 Two-Tier Architecture

eAI ships as two distinct product tiers sharing a common foundation:

### EAI-Min — Lightweight Runtime

For **MCUs, SBCs, and battery-powered edge devices** (Cortex-M7, RPi,
nRF5340):

| Module         | Description                                    |
|----------------|------------------------------------------------|
| **Runtime**    | Single-backend inference engine                |
| **Quantizer**  | INT4/INT8 model quantization                   |
| **Scheduler**  | Power-aware task scheduling                    |
| **HAL Bridge** | Direct integration with EoS HAL                |

Typical footprint: 256 KB Flash, 64 KB RAM minimum.

### EAI-Full — Complete AI Platform

For **edge servers, gateways, and application processors** (Jetson, i.MX8,
x86):

| Module             | Description                                  |
|--------------------|----------------------------------------------|
| **Multi-backend**  | GGML, TensorRT, ONNX Runtime, custom engines |
| **Agent Framework**| Autonomous agent loop with tool calling       |
| **Memory System**  | Context management with vector store          |
| **RAG Pipeline**   | Retrieval-augmented generation                |
| **Fine-tuning**    | On-device model adaptation                   |
| **Federation**     | Federated learning across device fleets      |

---

## 20.4 Inference Engine

The inference engine is the core of eAI. It supports multiple backends
through a unified API:

```c
#include "eai/inference.h"

// Load a quantized model
eai_model_t *model = eai_model_load("model.q4", EAI_BACKEND_GGML);

// Run inference
eai_result_t result;
eai_infer(model, "Analyze sensor reading: temp=72.3F", &result);

printf("Response: %s\n", result.text);
printf("Latency: %u ms\n", result.latency_ms);
printf("Tokens/s: %.1f\n", result.tokens_per_second);

eai_model_free(model);
```

### Supported Backends

| Backend       | Precision    | Hardware           | Use Case            |
|---------------|--------------|--------------------|---------------------|
| **GGML**      | Q4, Q8, F16  | CPU (any arch)     | General-purpose     |
| **TensorRT**  | INT8, FP16   | NVIDIA GPU/DLA     | High-throughput     |
| **ONNX**      | FP32, INT8   | CPU, NPU           | Model portability   |
| **Custom**    | Configurable | FPGA, DSP          | Specialized         |

### Quantization Pipeline

```bash
# Quantize a model for deployment
eai-quantize \
  --input model.gguf \
  --output model.q4 \
  --method q4_k_m \
  --calibration-data samples.jsonl
```

eAI supports progressive quantization: start with Q8 for accuracy, then
move to Q4 for deployment on smaller devices.

---

## 20.5 Agent Framework

EAI-Full includes an autonomous agent framework for complex, multi-step
tasks:

```
┌──────────────────────────────────────────────┐
│              Agent Loop                       │
│                                              │
│  ┌──────┐   ┌──────────┐   ┌──────────────┐ │
│  │ Plan │──►│ Execute  │──►│ Evaluate     │ │
│  │      │   │ (Tools)  │   │ (Feedback)   │ │
│  └──────┘   └──────────┘   └──────┬───────┘ │
│      ▲                            │          │
│      └────────────────────────────┘          │
└──────────────────────────────────────────────┘
```

### Tool Integration

Agents can call tools to interact with the physical world:

| Tool Category     | Examples                                      |
|-------------------|-----------------------------------------------|
| **Sensor Reading**| Temperature, humidity, accelerometer, GPS     |
| **Actuator Control** | Motor, servo, relay, LED                  |
| **Communication** | MQTT publish, HTTP request, BLE broadcast     |
| **Data Processing**| FFT, filtering, anomaly detection            |
| **System**        | Reboot, update, configure, log                |

### Agent Configuration

```yaml
# agent.yaml
name: "sensor-monitor"
model: "model.q4"
tools:
  - sensor_read
  - actuator_control
  - mqtt_publish
loop:
  max_iterations: 10
  timeout_seconds: 30
  eval_threshold: 0.8
memory:
  type: "sliding_window"
  context_tokens: 2048
```

---

## 20.6 Security Architecture

eAI implements an 8-layer defense-in-depth architecture:

| Layer | Name                 | Protection                                |
|-------|----------------------|-------------------------------------------|
| 1     | **Secure Boot**      | Verified boot chain (via eBoot)           |
| 2     | **Model Integrity**  | SHA-256 verification before loading       |
| 3     | **Memory Isolation** | Separate heaps for model and application  |
| 4     | **Input Validation** | Prompt sanitization and length limits     |
| 5     | **Output Filtering** | Response safety classification            |
| 6     | **Access Control**   | Capability-based API authorization        |
| 7     | **Audit Logging**    | All inference requests logged             |
| 8     | **Update Security**  | Signed model updates via OTA              |

---

## 20.7 Power-Aware Scheduling

On battery-powered devices, eAI manages inference scheduling to balance
responsiveness with power consumption:

```c
eai_power_config_t power_cfg = {
    .mode = EAI_POWER_ADAPTIVE,
    .battery_threshold_low = 20,    // percent
    .battery_threshold_critical = 5,
    .idle_timeout_ms = 5000,
    .max_inference_rate = 10,       // per minute
};
eai_power_configure(&power_cfg);
```

### Power Modes

| Mode         | Behavior                                          |
|--------------|---------------------------------------------------|
| **Full**     | Unrestricted inference, maximum performance       |
| **Balanced** | Throttled inference rate, medium power             |
| **Low**      | Only critical inferences, model stays loaded      |
| **Critical** | Model unloaded, only cached responses served      |

---

## 20.8 Integration with EoS

eAI integrates directly with the EoS HAL for sensor access and actuator
control:

```c
#include "eai/hal_bridge.h"
#include "eos/hal.h"

// Read sensor data through EoS HAL
float temperature = eos_sensor_read(SENSOR_TEMPERATURE);

// Feed to eAI inference
char prompt[128];
snprintf(prompt, sizeof(prompt),
         "Temperature is %.1f°C. Is this within normal range?",
         temperature);

eai_result_t result;
eai_infer(model, prompt, &result);

// Act on the result through EoS HAL
if (result.classification == EAI_CLASS_ANOMALY) {
    eos_gpio_write(ALARM_PIN, true);
    eos_mqtt_publish("alerts/temperature", result.text);
}
```

---

## 20.9 Model Management

### Model Formats

| Format     | Extension | Description                              |
|------------|-----------|------------------------------------------|
| GGUF       | `.gguf`   | GGML universal format                    |
| EAI Q4     | `.q4`     | eAI quantized INT4                       |
| EAI Q8     | `.q8`     | eAI quantized INT8                       |
| ONNX       | `.onnx`   | Open Neural Network Exchange             |
| TensorRT   | `.engine` | NVIDIA TensorRT compiled                 |

### Over-the-Air Model Updates

```c
eai_ota_config_t ota = {
    .url = "https://models.example.com/v2/sensor-monitor.q4",
    .verify_signature = true,
    .public_key = embedded_public_key,
    .max_download_size = 50 * 1024 * 1024,
    .apply_on_idle = true,
};
eai_ota_update(&ota);
```

---

## 20.10 Building eAI

```bash
git clone https://github.com/embeddedos-org/eai.git
cd eai

# Build EAI-Min (lightweight)
cmake -B build -DEAI_TIER=min
cmake --build build

# Build EAI-Full (complete)
cmake -B build -DEAI_TIER=full -DEAI_BACKEND_GGML=ON
cmake --build build

# Cross-compile for ARM
cmake -B build-arm \
  -DCMAKE_TOOLCHAIN_FILE=toolchains/aarch64-linux-gnu.cmake \
  -DEAI_TIER=full
cmake --build build-arm

# Run tests
ctest --test-dir build --output-on-failure
```

---

## 20.11 Compliance

eAI aligns with multiple safety and quality standards:

| Standard          | Relevance                                     |
|-------------------|-----------------------------------------------|
| ISO/IEC 25000     | Software quality model                        |
| ISO/IEC 15288     | Systems engineering lifecycle                 |
| ISO/IEC 20243     | Supply chain integrity                        |
| IEC 61508         | Functional safety (industrial)                |
| ISO 26262         | Functional safety (automotive)                |
| DO-178C           | Software assurance (aerospace)                |

---

## 20.12 Summary

eAI brings production-grade AI inference to embedded devices without cloud
dependency. Its two-tier architecture (EAI-Min for MCUs, EAI-Full for edge
servers) ensures the right AI footprint for every device class.

**Key takeaways:**

- C11 framework — runs on everything from Cortex-M7 to Jetson Orin
- Multiple inference backends (GGML, TensorRT, ONNX)
- Autonomous agent framework with tool calling
- 8-layer security architecture
- Power-aware scheduling for battery devices
- Zero cloud dependency with optional hybrid connectivity

---

*Next: Chapter 21 — eNI Neural Interface*
