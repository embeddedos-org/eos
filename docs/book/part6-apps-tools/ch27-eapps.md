# Chapter 27: eApps — Unified Marketplace and App Store

*Author: Srikanth Patchava & EmbeddedOS Contributors*

---

## 27.1 Introduction

**eApps** is the unified marketplace, monorepo, and automated app store for
the entire EoS ecosystem. It delivers **50 apps across 8 platform categories
totaling 188 platform targets** — from native C+LVGL embedded apps to Flutter
mobile apps to Chrome extensions.

---

## 27.2 App Categories

| Category              | Folder              | Count | Technologies                   |
|-----------------------|---------------------|-------|--------------------------------|
| **Native Apps**       | `apps/`             | 46    | C + LVGL (cross-platform)      |
| **Desktop Apps**      | `desktop-apps/`     | 1     | Electron, Python, C/SDL2       |
| **Mobile Apps**       | `mobile-apps/`      | 32    | Flutter (Android + iOS)        |
| **Web Apps**          | `web-apps/`         | 34    | HTML5/JS/WASM PWA              |
| **Browser Extensions**| `browser-extensions/`| 20   | WebExtensions Manifest V3      |
| **Dev Tools**         | `dev-tools/`        | 14    | VS Code, JetBrains, Vim        |
| **CLI Tools**         | `cli-tools/`        | 22    | Node.js, Python                |
| **Enterprise**        | `enterprise/`       | 16    | Docker, Helm, MSI, MDM        |

---

## 27.3 Native App Architecture

The 46 native apps share a common architecture built on C and LVGL
(Light and Versatile Graphics Library):

```
┌──────────────────────────────────────┐
│           LVGL UI Layer              │
│  Widgets, themes, animations         │
├──────────────────────────────────────┤
│        Application Logic             │
│  Business logic in C                 │
├──────────────────────────────────────┤
│          EoS HAL Layer               │
│  Display, touch, GPIO, storage       │
└──────────────────────────────────────┘
```

### Building Native Apps

```bash
cd apps/calculator
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
./calculator
```

### Cross-Compilation

```bash
# Build for STM32F4
cmake .. -DCMAKE_TOOLCHAIN_FILE=../../toolchains/arm-none-eabi-stm32f4.cmake
make

# Build for Raspberry Pi
cmake .. -DCMAKE_TOOLCHAIN_FILE=../../toolchains/aarch64-linux-gnu.cmake
make
```

---

## 27.4 Featured Native Apps

| App             | Description                              |
|-----------------|------------------------------------------|
| Calculator      | Scientific calculator with graphing      |
| File Manager    | File browser with preview                |
| Terminal        | VT100 terminal emulator                  |
| Text Editor     | Syntax-highlighted code editor           |
| Settings        | System configuration manager             |
| Gallery         | Image viewer with thumbnails             |
| Music Player    | Audio playback with visualizer           |
| Clock           | World clock, timer, stopwatch, alarm     |
| Weather         | Weather display with forecasts           |
| Camera          | Camera capture with filters              |
| Maps            | Offline map viewer with GPS              |
| Health          | Health metrics dashboard                 |
| Home Control    | Smart home device controller             |
| System Monitor  | CPU, memory, network status              |

---

## 27.5 Mobile Apps (Flutter)

The 32 mobile apps are built with Flutter for cross-platform deployment:

```bash
cd mobile-apps/ehealth
flutter pub get
flutter run          # Debug on connected device
flutter build apk    # Android release
flutter build ios    # iOS release
```

Mobile apps communicate with EoS devices over Bluetooth LE using a
shared protocol library.

---

## 27.6 Web Apps (PWA)

The 34 web apps are progressive web applications deployable to any browser:

```bash
cd web-apps/dashboard
npm install
npm run dev     # Development server
npm run build   # Production build
```

Web apps are automatically deployed to GitHub Pages as PWAs with
offline support via service workers.

---

## 27.7 Browser Extensions

20 browser extensions using WebExtensions Manifest V3:

```bash
cd browser-extensions/eos-connect
npm install
npm run build
# Load unpacked extension in Chrome/Firefox
```

---

## 27.8 Developer Tools

| Tool              | Platform    | Description                     |
|-------------------|-------------|---------------------------------|
| VS Code Extension | VS Code     | EoS syntax, debugging, flashing|
| JetBrains Plugin  | IntelliJ    | EoS project support            |
| Vim Plugin        | Vim/Neovim  | EoS syntax highlighting        |

---

## 27.9 CLI Tools

22 command-line tools for development and automation:

```bash
# Install via npm
npm install -g @eos/cli-tools

# Or via pip
pip install eos-cli-tools
```

---

## 27.10 Enterprise Deployment

Enterprise tools support managed deployment:

| Format       | Description                            |
|--------------|----------------------------------------|
| Docker       | Containerized services                 |
| Helm Charts  | Kubernetes deployment                  |
| MSI          | Windows managed installer              |
| MDM          | Mobile device management profiles      |

---

## 27.11 Live App Store

The eApps marketplace is hosted as a static GitHub Pages site:

```
https://embeddedos-org.github.io/eApps/
```

Features:
- Categorized app browsing
- Platform filtering
- One-click download
- Version history
- User ratings and reviews

---

## 27.12 CI/CD Pipeline

Every app has automated CI/CD:

- **Native apps:** CMake build on Linux, Windows, macOS
- **Mobile apps:** Flutter build + deploy to TestFlight/Play Store
- **Web apps:** Build + deploy to GitHub Pages
- **Extensions:** Package + publish to Chrome/Firefox stores
- **CLI tools:** npm publish + PyPI publish

---

## 27.13 Summary

eApps provides the complete application ecosystem for EoS — from embedded
native apps running on microcontrollers to enterprise Docker containers.

**Key takeaways:**

- 50 apps across 8 platform categories = 188 platform targets
- Native apps built with C + LVGL for embedded displays
- Flutter mobile apps for iOS and Android
- PWA web apps with offline support
- Automated CI/CD for all platforms
- Live app store at embeddedos-org.github.io/eApps

---

*Next: Chapter 28 — eDB Database*
