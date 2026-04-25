# Chapter 31: eVera — Voice-First Multi-Agent AI Assistant

*Author: Srikanth Patchava & EmbeddedOS Contributors*

---

## 31.1 Introduction

**eVera** is a voice-first, multi-agent AI assistant that serves as the
primary user interface for the EoS ecosystem. With **24+ specialized agents**
and **183+ tools**, eVera can control devices, automate workflows, answer
questions, and manage entire systems through natural language.

eVera runs on Windows, macOS, Linux, Android, and iOS.

---

## 31.2 Multi-Agent Architecture

eVera uses a multi-agent design where specialized agents handle different
domains. A central orchestrator routes user requests to the appropriate
agent based on intent classification.

### Agent Categories

| Category          | Agents                                      |
|-------------------|---------------------------------------------|
| **System**        | Device control, settings, diagnostics       |
| **Productivity**  | Calendar, email, tasks, notes               |
| **Development**   | Code generation, debugging, deployment      |
| **Data**          | Database queries, analytics, visualization  |
| **Communication** | Messaging, calls, notifications             |
| **IoT/Home**      | Smart home, sensors, automation rules       |
| **Creative**      | Image generation, music, design             |
| **Research**      | Web search, summarization, fact-checking    |

---

## 31.3 Voice Interface

eVera is voice-first by design:

```
User (voice): "Hey eVera, turn on the living room lights
               and set temperature to 72"

eVera:
  1. Speech-to-text (Whisper / on-device)
  2. Intent classification → IoT agent
  3. Tool call: smart_home.set_device("lights", "living_room", "on")
  4. Tool call: smart_home.set_temperature("living_room", 72)
  5. Text-to-speech response: "Done. Lights are on and
     thermostat set to 72 degrees."
```

### Speech Backends

| Backend       | Type     | Latency | Privacy  |
|---------------|----------|---------|----------|
| Whisper       | On-device| Low     | Full     |
| Google STT    | Cloud    | Medium  | Partial  |
| Azure Speech  | Cloud    | Medium  | Partial  |

---

## 31.4 Tool System

eVera exposes 183+ tools that agents can call:

| Tool Category       | Count | Examples                           |
|----------------------|-------|------------------------------------|
| File System          | 15    | read, write, search, compress      |
| Network              | 12    | HTTP, MQTT, WebSocket, ping        |
| Database             | 10    | SQL query, document ops, KV ops    |
| Smart Home           | 20    | Lights, thermostat, locks, cameras |
| Calendar             | 8     | Create event, check availability   |
| Email                | 10    | Send, read, search, draft          |
| Code                 | 15    | Generate, review, test, deploy     |
| System               | 12    | Reboot, update, monitor, backup    |
| Media                | 10    | Play music, capture photo, record  |
| Search               | 8     | Web, local files, knowledge base   |
| Math/Science         | 10    | Calculate, convert, plot           |

---

## 31.5 Quick Start

```bash
git clone https://github.com/embeddedos-org/eVera.git
cd eVera
pip install -e ".[all]"

# Start eVera
evera start

# Start with voice mode
evera start --voice

# Ask a question
evera ask "What is the CPU temperature?"

# Run an automation
evera run automation/morning_routine.yaml
```

---

## 31.6 Automation Rules

eVera supports YAML-based automation rules:

```yaml
name: morning_routine
trigger:
  time: "07:00"
  days: [mon, tue, wed, thu, fri]
actions:
  - agent: iot
    tool: smart_home.set_device
    args: { device: "coffee_maker", action: "on" }
  - agent: iot
    tool: smart_home.set_temperature
    args: { zone: "house", temp: 72 }
  - agent: productivity
    tool: calendar.get_today
    output: today_events
  - agent: communication
    tool: tts.speak
    args: { text: "Good morning. You have {{ today_events.count }} events today." }
```

---

## 31.7 Platform Support

| Platform       | Interface          | Backend          |
|----------------|--------------------|------------------|
| Windows        | Electron desktop   | Python server    |
| macOS          | Electron desktop   | Python server    |
| Linux          | Electron desktop   | Python server    |
| Android        | React Native       | On-device        |
| iOS            | React Native       | On-device        |
| Web            | React webapp       | Cloud/self-host  |
| EoS (embedded) | LVGL + voice       | On-device (eAI)  |

---

## 31.8 Security

- **Hardened by default** — input sanitization, rate limiting
- **Permission system** — tools require explicit user permission
- **Conversation encryption** — all data encrypted at rest
- **On-device option** — full local processing with Ollama/eAI
- **Audit trail** — all agent actions logged

---

## 31.9 LLM Backends

| Backend        | Type      | Models                    |
|----------------|-----------|---------------------------|
| Ollama         | Local     | Llama 3, Mistral, Phi     |
| OpenAI         | Cloud     | GPT-4o, GPT-4            |
| eAI            | Embedded  | Quantized on-device models|
| Anthropic      | Cloud     | Claude                    |

---

## 31.10 Summary

eVera is the conversational AI layer that makes the entire EoS ecosystem
accessible through natural language.

**Key takeaways:**

- 24+ specialized agents covering all user-facing domains
- 183+ tools for device control, automation, and productivity
- Voice-first with on-device speech processing
- YAML-based automation rules
- Cross-platform: desktop, mobile, web, embedded
- Multiple LLM backends (local and cloud)

---

*Next: Chapter 32 — eStocks Trading System*
