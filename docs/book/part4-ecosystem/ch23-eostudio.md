# Chapter 23: EoStudio — Cross-Platform Design Suite

*Author: Srikanth Patchava & EmbeddedOS Contributors*

## 23.1 Introduction

Embedded development requires more than just a code editor. From 3D modeling
of enclosures to PCB layout, from UI/UX prototyping to game design — modern
embedded products demand a comprehensive design toolkit.

**EoStudio** is a unified design tool suite for the EmbeddedOS ecosystem,
combining 12 specialized editors, 30+ code generators, and LLM-powered AI
assistance — all running on Windows, Linux, and EoS.

## 23.2 The Twelve Editors

| Editor              | CLI Flag       | Use Case                                |
|---------------------|----------------|-----------------------------------------|
| 3D Modeler          | `--editor 3d`  | Mesh modeling, materials, lighting      |
| CAD Designer        | `--editor cad` | Parametric design, assemblies           |
| Image Editor        | `--editor paint` | Layers, brushes, filters              |
| Game Editor         | `--editor game` | ECS, tilemaps, sprites                 |
| UI/UX Designer      | `--editor ui`  | Components, flows, prototyping         |
| Product Designer    | `--editor product` | BOM, 3D-print validation           |
| Interior Designer   | `--editor interior` | Floor plans, furniture             |
| UML Modeler         | `--editor uml` | Class, sequence, state diagrams        |
| Simulation Editor   | `--editor simulation` | Block diagrams, PID, signals     |
| Database Designer   | `--editor database` | ERD, schema, SQL/ORM codegen      |
| Hardware Editor     | `--editor hardware` | PCB layout, schematics, Gerber    |
| IDE                 | `--editor ide` | Code editing, debugging, Git           |

## 23.3 Quick Start

```bash
# Clone and install
git clone https://github.com/embeddedos-org/EoStudio.git
cd EoStudio
pip install -e ".[all]"

# Launch the design suite
EoStudio launch

# Create a project from template
EoStudio new --template todo-app -o ./my-app

# Generate React code from a design
EoStudio codegen my-app/todo-app.eostudio --framework react -o ./output

# Ask the AI design agent
EoStudio ask "Design a responsive dashboard with charts"
```

## 23.4 Code Generation

EoStudio can generate production-ready code from visual designs across
30+ frameworks:

| Target           | Frameworks                                         |
|------------------|----------------------------------------------------|
| **Mobile**       | Flutter, React Native, Kotlin (Android), Swift (iOS)|
| **Desktop**      | Electron, Tauri, tkinter, Qt, Compose Desktop      |
| **Web**          | React+FastAPI, Vue+Flask, Angular+Express, Svelte   |
| **Database**     | SQLite, PostgreSQL, MySQL, SQLAlchemy, Prisma       |
| **UML to Code**  | Python, Java, Kotlin, TypeScript, C++, C#           |
| **3D/CAD**       | OpenSCAD, STL, OBJ, glTF, DXF                      |
| **Game Engines** | Godot, Unity, Unreal                                |
| **Firmware**     | EoS, Baremetal, FreeRTOS, Zephyr                    |

### Code Generation Example

```bash
# Generate a Flutter mobile app from a UI design
EoStudio codegen dashboard.eostudio --framework flutter -o ./flutter_app

# Generate OpenSCAD from a CAD design
EoStudio codegen bracket.eostudio --framework openscad -o ./scad_output

# Generate EoS firmware from a hardware design
EoStudio codegen sensor_board.eostudio --framework eos -o ./firmware
```

## 23.5 AI and LLM Integration

EoStudio deeply integrates AI assistance across all editors:

| Feature            | Description                                       |
|--------------------|---------------------------------------------------|
| **Design Agent**   | Multi-domain Q&A, design brief generation         |
| **Smart Chat**     | Per-editor AI panel with context-aware prompts    |
| **AI Generator**   | Text-to-UI, text-to-3D, text-to-CAD generation   |
| **AI Simulator**   | Parameter suggestion, instability detection       |
| **Kids Tutor**     | Interactive lessons with quizzes                  |

### LLM Backend Configuration

EoStudio supports both local and cloud LLM backends:

```bash
# Option 1: Ollama (local, private)
ollama pull llama3
EoStudio ask "Design a login page"

# Option 2: OpenAI API
export EOSTUDIO_LLM_PROVIDER=openai
export OPENAI_API_KEY=sk-your-key
export EOSTUDIO_LLM_MODEL=gpt-4o
EoStudio ask --domain cad "Design an L-bracket with mounting holes"
```

## 23.6 Project Templates

EoStudio includes 5 built-in templates demonstrating real workflows:

| Template            | Workflow                         | Command                              |
|---------------------|----------------------------------|--------------------------------------|
| `todo-app`          | UI Designer to React/Flutter     | `EoStudio new --template todo-app`   |
| `mechanical-part`   | CAD Designer to OpenSCAD to STL  | `EoStudio new --template mechanical-part` |
| `game-platformer`   | Game Editor to Godot/Unity       | `EoStudio new --template game-platformer` |
| `iot-dashboard`     | Database + UI to Full-stack      | `EoStudio new --template iot-dashboard` |
| `simulation-pid`    | Simulation Editor to PID         | `EoStudio new --template simulation-pid` |

## 23.7 Architecture

```
eostudio/
+-- cli/               # Click CLI (10 commands)
+-- core/
|   +-- ai/            # LLMClient, DesignAgent, SmartChat, AIGenerator
|   +-- geometry/      # Vec2/3/4, Matrix4, Mesh, Bezier, NURBS, CSG
|   +-- rendering/     # Rasterizer, scene graph, camera, Phong lighting
|   +-- physics/       # Rigid body, collision, particles
|   +-- cad/           # Parametric design, constraints, assembly
|   +-- simulation/    # Block diagrams, PID, signals, ODE solver
|   +-- uml/           # 5 diagram types + code generation
|   +-- game/          # ECS, tilemap, sprites, scripting
|   +-- image/         # Layers, brushes, filters
|   +-- hardware/      # PCB, schematic, Gerber
|   +-- ui_flow/       # Component library, prototyping
|   +-- interior/      # Floor plans, furniture
+-- gui/
|   +-- editors/       # 12 visual editors
|   +-- widgets/       # Viewport, canvas, timeline, properties
|   +-- dialogs/       # Export, settings, AI chat
+-- codegen/           # 30+ framework code generators
+-- formats/           # .EoStudio, OBJ, STL, SVG, glTF, DXF
+-- plugins/           # Plugin system + EoSim integration
+-- templates/         # 5 project templates
```

## 23.8 Plugin System

EoStudio supports plugins for extending functionality:

```python
from eostudio.plugins.plugin_base import Plugin, PluginHook

class MyPlugin(Plugin):
    def activate(self, context):
        self._hooks[PluginHook.POST_CODEGEN] = self._on_codegen
        return super().activate(context)

    def _on_codegen(self, data):
        # Post-process generated code
        return {"processed": True}
```

Plugins can hook into lifecycle events:
- `PRE_CODEGEN` / `POST_CODEGEN` — modify code generation
- `PRE_EXPORT` / `POST_EXPORT` — modify file export
- `EDITOR_INIT` — add custom panels to editors
- `AI_RESPONSE` — filter or augment AI responses

## 23.9 Platform Support

| Platform       | Status | Backend       |
|----------------|--------|---------------|
| Windows 10/11  | Stable | tkinter       |
| Ubuntu 22.04+  | Stable | tkinter       |
| Linux (other)  | Stable | tkinter       |
| macOS          | Stable | macOS native  |
| EoS            | Stable | Framebuffer   |
| Browser        | Beta   | Web backend   |

## 23.10 Testing

```bash
pip install -e ".[dev,all]"
pytest -v
pytest --cov=eostudio --cov-report=term-missing
flake8 eostudio/ tests/ --max-line-length=120
mypy eostudio/ --ignore-missing-imports
```

Test coverage spans: AI modules, geometry, code generation, plugins,
simulation, file formats, and integration testing.

## 23.11 Summary

EoStudio consolidates the diverse design tools needed for embedded product
development into a single, AI-enhanced suite. From CAD modeling to firmware
code generation, from UI prototyping to PCB layout, EoStudio provides a
unified workflow that eliminates tool-switching overhead.

**Key takeaways:**

- 12 specialized editors in one unified application
- 30+ code generators targeting mobile, web, desktop, firmware
- LLM-powered AI assistance with Ollama and OpenAI backends
- Plugin system for extensibility
- Cross-platform: Windows, Linux, macOS, EoS, and browser
- 5 built-in project templates for rapid prototyping

*Next: Chapter 24 — eRadar360 Hardware Design*
