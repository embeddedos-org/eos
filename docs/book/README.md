# EmbeddedOS: The Complete Guide
## From Bootloader to Browser, Kernel to Space

**Author:** Srikanth Patchava & EmbeddedOS Contributors
**Version:** 1.0 (April 2026)
**License:** CC BY-SA 4.0

---

> *"One unified platform from bootloader to browser, from microcontroller to orbit."*

---

## About the Author

**Srikanth Patchava** is the creator and lead architect of EmbeddedOS, an open-source embedded operating system ecosystem spanning 18 repositories. With deep expertise in embedded systems, real-time operating systems, hardware design, AI inference, and full-stack development, Srikanth designed EoS to be a universal platform supporting 41 product categories across automotive, aerospace, medical, consumer, and industrial domains.

- GitHub: [github.com/embeddedos-org](https://github.com/embeddedos-org)
- Website: [embeddedos-org.github.io](https://embeddedos-org.github.io)

---

## About This Book

This book is the official, comprehensive reference for the entire EmbeddedOS ecosystem. It covers:

- **EoS** — the core embedded operating system (kernel, HAL, services, drivers)
- **eBoot** — the secure bootloader
- **ebuild** — the build system
- **eIPC** — inter-process communication
- **eAI** — embedded AI/ML inference
- **eNI** — neural interface & BCI
- **eApps** — application marketplace
- **EoSim** — multi-architecture simulation
- **EoStudio** — cross-platform design suite
- **eDB** — unified database
- **eBrowser** — embedded web browser
- **eOffice** — office suite
- **eVera** — AI virtual assistant
- **eStocks** — algorithmic trading
- **eHealth365** — wearable health monitoring hardware
- **ePAM** — personal air mobility hardware
- **eRadar360** — automotive radar hardware

Whether you are building a smartwatch, a medical device, an autonomous drone, or a space vehicle, this book provides everything you need.

---

## Table of Contents

### Part I — Foundations

1. [Introduction to EmbeddedOS](part1-foundations/ch01-introduction.md)
   - What is EmbeddedOS?
   - The 18-repository ecosystem
   - Architecture overview
   - Who should read this book

2. [Getting Started](part1-foundations/ch02-getting-started.md)
   - Prerequisites and tool installation
   - Building EoS from source
   - Your first "Hello World" on host, STM32, RPi4, nRF52
   - Understanding product profiles

3. [Architecture & Design Philosophy](part1-foundations/ch03-architecture.md)
   - Layered architecture (HAL → Kernel → Services → Apps)
   - The product profile system (41 profiles)
   - Cross-platform portability strategy
   - Standards compliance (ISO 26262, DO-178C, IEC 61508)

### Part II — Kernel & HAL

4. [Hardware Abstraction Layer](part2-kernel-hal/ch04-hal.md)
   - The 33 peripheral interfaces
   - GPIO, UART, SPI, I2C, Timer
   - Backend dispatch pattern
   - Writing a custom HAL backend

5. [Extended HAL — Advanced Peripherals](part2-kernel-hal/ch05-hal-extended.md)
   - ADC, DAC, PWM, CAN, USB
   - WiFi, BLE, Cellular, NFC
   - Camera, Audio, Display, GPU
   - IMU, GNSS, Radar, Motor
   - The vtable dispatch pattern

6. [RTOS Kernel](part2-kernel-hal/ch06-kernel.md)
   - Task management & scheduling
   - Mutex, semaphore, message queues
   - Software timers
   - Priority inversion protection
   - Real-time guarantees

7. [Multicore — SMP & AMP](part2-kernel-hal/ch07-multicore.md)
   - Core management & identification
   - Spinlocks (architecture-aware: ARM, x86, RISC-V)
   - Memory barriers & atomics
   - Inter-processor interrupts (IPI)
   - Shared memory regions
   - Task migration & affinity
   - Remote processor management

8. [Driver Framework](part2-kernel-hal/ch08-drivers.md)
   - Driver probe/remove lifecycle
   - Power management hooks
   - Device tree integration
   - Writing your first driver

### Part III — Services

9. [Cryptography](part3-services/ch09-crypto.md)
   - SHA-256, SHA-512 (full implementation)
   - AES encryption
   - RSA signing & verification (big-integer arithmetic)
   - ECDSA P-256 (elliptic curve)
   - CRC checksums

10. [Security](part3-services/ch10-security.md)
    - Keystore & key management
    - Access control lists (ACL)
    - Secure boot chain
    - Integrity monitoring
    - Audit logging

11. [Networking](part3-services/ch11-networking.md)
    - TCP/UDP sockets
    - HTTP client/server
    - MQTT client
    - mDNS service discovery
    - TLS integration

12. [Filesystem & Storage](part3-services/ch12-filesystem.md)
    - Flash filesystem
    - File operations API
    - Wear leveling
    - Secure storage

13. [OTA Updates](part3-services/ch13-ota.md)
    - Update lifecycle
    - Delta updates
    - Rollback protection
    - Secure update verification

14. [Sensor & Motor Services](part3-services/ch14-sensor-motor.md)
    - Sensor framework (register, read, calibrate)
    - Motor control with PID
    - Sensor fusion patterns

15. [Compatibility Layers](part3-services/ch15-compat.md)
    - POSIX (threads, sync, signals, I/O)
    - VxWorks (tasks, semaphores, watchdog, message queues)
    - Linux IPC (D-Bus bridge, shared memory)

16. [UI Framework](part3-services/ch16-ui.md)
    - LVGL integration
    - Touch input handling
    - Display rendering
    - Building a watchface

### Part IV — The Ecosystem

17. [eBoot — Secure Bootloader](part4-ecosystem/ch17-eboot.md)
    - Stage 0 / Stage 1 boot flow
    - Ed25519 signature verification
    - Board support (24+ boards)
    - Secure boot chain integration

18. [ebuild — Build System](part4-ecosystem/ch18-ebuild.md)
    - CLI commands
    - Build configuration (YAML)
    - Cross-compilation toolchains
    - SDK generation
    - CI/CD integration

19. [eIPC — Inter-Process Communication](part4-ecosystem/ch19-eipc.md)
    - The EIPC protocol (Go + C)
    - HMAC authentication
    - Service discovery
    - Pub/sub messaging
    - Chat & completion handlers

20. [eAI — Embedded AI](part4-ecosystem/ch20-eai.md)
    - AI framework architecture
    - Model loading & inference (EAIM binary format)
    - TFLite Micro integration
    - Secure boot for AI models
    - BCI device integration (OpenBCI)
    - On-device LLM (llama.cpp)

21. [eNI — Neural Interface](part4-ecosystem/ch21-eni.md)
    - EEG signal acquisition
    - DSP pipeline (Welch's PSD, band power)
    - ONNX model inference
    - Intent decoding
    - Wireless BCI (BlueZ BLE)
    - Python SDK

22. [EoSim — Simulation Platform](part4-ecosystem/ch22-eosim.md)
    - QEMU integration (QMP, GDB)
    - Native peripheral simulation
    - State bridge architecture
    - Multi-architecture support (52+ platforms)

23. [EoStudio — Design Suite](part4-ecosystem/ch23-eostudio.md)
    - Platform backends (Tkinter, Web, EoS)
    - 12 specialized editors
    - WebSocket remote rendering
    - LLM integration

### Part V — Hardware Products

24. [eRadar360 — Automotive Radar](part5-hardware/ch24-eradar360.md)
    - System architecture
    - 10-layer hybrid PCB design
    - Antenna design
    - Power sequencing
    - Manufacturing & bring-up

25. [eHealth365 — Health Monitoring](part5-hardware/ch25-ehealth365.md)
    - Two-device system (Smart Ring Pro + Smart Patch Pro)
    - Sensor specifications (PPG, CGM, sweat biosensor, bioimpedance)
    - 3D model specifications
    - Monthly blood cartridge system
    - 24-hour sensing schedule
    - Mobile app architecture
    - Battery optimization
    - Business plan & go-to-market

26. [ePAM — Personal Air Mobility](part5-hardware/ch26-epam.md)
    - Four-vehicle product line
    - Eco Car (solar-hybrid ground vehicle)
    - Urban Drone (eVTOL)
    - Space Shuttle (suborbital) + ULP-SSN avionics
    - Combo Unit (trans-atmospheric)
    - Power architecture (solar + H2 + battery + regen)
    - Tesla-like mobile app
    - Manufacturing cost analysis
    - Regulatory path (FAA, EASA, AST)

### Part VI — Applications & Tools

27. [eApps — Marketplace](part6-apps-tools/ch27-eapps.md)
    - 43 applications
    - App lifecycle management
    - Cross-platform deployment
    - Marketplace API

28. [eDB — Database](part6-apps-tools/ch28-edb.md)
    - Multi-model database architecture
    - CLI commands
    - Query engine
    - Embedded vs. server modes

29. [eBrowser — Web Browser](part6-apps-tools/ch29-ebrowser.md)
    - Browser engine architecture
    - Platform abstraction layer
    - HTML/CSS/JS rendering
    - Embedded device optimization

30. [eOffice — Office Suite](part6-apps-tools/ch30-eoffice.md)
    - 11 productivity applications
    - Desktop (Electron) architecture
    - Document format support

31. [eVera — AI Assistant](part6-apps-tools/ch31-evera.md)
    - 24+ specialized agents
    - 183+ tools
    - 3D avatar (Three.js)
    - Voice control (3 modes, 19 languages)
    - Plugin architecture
    - Cross-platform (Electron, React Native, web)

32. [eStocks — Trading System](part6-apps-tools/ch32-estocks.md)
    - 15 trading strategies
    - 7-layer risk management
    - Backtesting engine
    - Production safety controls
    - Platform integrations (TradingView, IBKR, thinkorswim)

### Part VII — Advanced Topics

33. [Cross-Compilation & Toolchains](part7-advanced/ch33-cross-compile.md)
    - AArch64, ARM hard-float, RISC-V
    - Custom toolchain files
    - Multi-architecture CI/CD

34. [Testing & Quality](part7-advanced/ch34-testing.md)
    - Unit testing framework
    - Fuzz testing
    - Sanitizers (ASan, UBSan)
    - Valgrind memory checking
    - CI/CD pipeline architecture

35. [Safety & Compliance](part7-advanced/ch35-safety.md)
    - ISO 26262 (automotive)
    - DO-178C (aerospace)
    - IEC 61508 (industrial)
    - IEC 62304 (medical)
    - Quality management system

36. [Contributing to EmbeddedOS](part7-advanced/ch36-contributing.md)
    - Development workflow
    - Coding standards
    - Pull request process
    - Community guidelines

### Appendices

- [A. API Quick Reference](appendices/appendix-a-api-quickref.md)
- [B. Product Profile Reference](appendices/appendix-b-profiles.md)
- [C. Pin Assignment Tables](appendices/appendix-c-pinouts.md)
- [D. Glossary](appendices/appendix-d-glossary.md)
- [E. Bibliography & Standards](appendices/appendix-e-bibliography.md)

---

## How to Build This Book

### Read Online
```
Visit: https://embeddedos-org.github.io/eos/book/
```

### Build Locally (mdBook)
```bash
# Install mdBook
cargo install mdbook

# Build the book
cd docs/book
mdbook build

# Serve locally with live reload
mdbook serve --open
```

### Generate PDF
```bash
# Install pandoc + LaTeX
sudo apt-get install pandoc texlive-xetex texlive-fonts-recommended

# Generate PDF from markdown
cd docs/book
./generate-pdf.sh
# Output: embeddedos-complete-guide.pdf
```

---

## Conventions Used in This Book

- `code` — function names, file paths, commands
- **Bold** — key terms on first introduction
- *Italic* — emphasis
- `eos_` prefix — all EoS public API functions
- Code blocks with filename headers show source file locations

---

*Copyright (c) 2026 Srikanth Patchava & EmbeddedOS Contributors. Licensed under CC BY-SA 4.0.*
