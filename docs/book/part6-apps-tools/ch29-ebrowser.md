# Chapter 29: eBrowser — Lightweight Embedded Web Browser

*Author: Srikanth Patchava & EmbeddedOS Contributors*

---

## 29.1 Introduction

**eBrowser** is a lightweight web browser designed for embedded and IoT
devices. Built in C/C++ with SDL2 and LVGL, it provides web browsing
capability on resource-constrained systems where full browsers like
Chromium are impractical.

---

## 29.2 Quick Start

No prerequisites needed. The setup scripts auto-detect your OS and install
everything (compiler, CMake, SDL2, LVGL):

```bash
# Linux / macOS
git clone --recursive https://github.com/embeddedos-org/eBrowser.git
cd eBrowser
chmod +x setup.sh
./setup.sh                        # builds and opens the browser
./setup.sh https://example.com    # opens directly to a URL
```

```batch
:: Windows
git clone --recursive https://github.com/embeddedos-org/eBrowser.git
cd eBrowser
setup.bat
```

The scripts automatically install GCC/Clang, CMake, SDL2, and LVGL v9.2,
then build and launch eBrowser.

---

## 29.3 Features

| Feature              | Description                              |
|----------------------|------------------------------------------|
| **HTML Rendering**   | Basic HTML5 parsing and layout           |
| **CSS Support**      | Core CSS properties and selectors        |
| **JavaScript**       | Lightweight JS engine for interactivity  |
| **Tab Management**   | Multiple tabs with tab bar               |
| **Bookmarks**        | Save and manage bookmarks                |
| **History**          | Browsing history with search             |
| **Downloads**        | File download manager                    |
| **LVGL UI**          | Native LVGL widgets for browser chrome   |
| **Touch Support**    | Touch-optimized for embedded displays    |
| **Keyboard Nav**     | Full keyboard navigation support         |

---

## 29.4 Architecture

```
┌────────────────────────────────────────┐
│            eBrowser UI (LVGL)          │
│  Address bar, tabs, bookmarks bar      │
├────────────────────────────────────────┤
│          Rendering Engine              │
│  HTML parser, CSS engine, layout       │
├────────────────────────────────────────┤
│          Network Stack                 │
│  HTTP/HTTPS, TLS, DNS, caching        │
├────────────────────────────────────────┤
│          Platform Layer                │
│  SDL2 (desktop) / EoS HAL (embedded)  │
└────────────────────────────────────────┘
```

---

## 29.5 Building from Source

```bash
# Standard build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./ebrowser

# With custom SDL2 path
cmake .. -DSDL2_DIR=/opt/sdl2

# Cross-compile for embedded
cmake .. -DCMAKE_TOOLCHAIN_FILE=toolchains/aarch64-linux-gnu.cmake
```

---

## 29.6 Embedded Deployment

For embedded targets, eBrowser uses the EoS display HAL instead of SDL2:

```bash
cmake .. -DEBROWSER_BACKEND=eos \
  -DCMAKE_TOOLCHAIN_FILE=toolchains/arm-none-eabi-stm32f4.cmake \
  -DEOS_PRODUCT=hmi
```

This produces a firmware image that renders directly to a framebuffer
or SPI/parallel display.

### Memory Requirements

| Configuration    | Flash    | RAM      |
|------------------|----------|----------|
| Minimal (HTML)   | 512 KB   | 256 KB   |
| Standard         | 1.5 MB   | 1 MB     |
| Full (JS + CSS)  | 4 MB     | 4 MB     |

---

## 29.7 Configuration

eBrowser is configured via a YAML file or command-line flags:

```yaml
# ebrowser.yaml
display:
  width: 800
  height: 480
  fullscreen: false

network:
  proxy: null
  timeout_seconds: 30
  max_connections: 6

rendering:
  font_size: 16
  dark_mode: false

cache:
  max_size_mb: 50
  location: /tmp/ebrowser_cache
```

---

## 29.8 Platform Support

| Platform         | Backend    | Status |
|------------------|------------|--------|
| Linux x86_64     | SDL2       | Stable |
| Linux ARM64      | SDL2       | Stable |
| Windows          | SDL2       | Stable |
| macOS            | SDL2       | Stable |
| EoS (embedded)   | Framebuffer| Stable |
| Raspberry Pi     | SDL2/DRM   | Stable |

---

## 29.9 Summary

eBrowser brings web browsing to embedded devices where traditional browsers
cannot run.

**Key takeaways:**

- Lightweight C/C++ browser with SDL2 and LVGL
- Zero-prerequisite setup with auto-installing scripts
- Cross-platform: desktop and embedded targets
- Touch-optimized UI for embedded displays
- Configurable memory footprint (256 KB to 4 MB RAM)

---

*Next: Chapter 30 — eOffice Suite*
