# Exorcist IDE — Top 20 Development Environment Templates

**Purpose:**
წინასწარ კონფიგურირებული სამუშაო გარემოს პროფილები (Development Profiles), რომლებიც ავტომატურად ტვირთავს შესაბამის მოდულებს, პლაგინებს და ხელსაწყოებს კონკრეტული ტიპის პროექტებისთვის.

**Philosophy:**

```
IDE Core  = Container
Profiles  = Capability Packs
Modules   = Feature DLLs (plugins)
```

როცა მომხმარებელი ქმნის ან ხსნის პროექტს, Exorcist IDE ტვირთავს მხოლოდ იმ მოდულებს, რომლებიც კონკრეტულ პროფილს სჭირდება. გამოუყენებელი მოდულები არ იტვირთება — ნულოვანი მეხსიერება, ნულოვანი CPU.

---

## 1. Embedded / MCU Development

| | |
|---|---|
| **Target** | ESP32, ARM Cortex-M, AVR, RISC-V MCU |
| **Activation** | `platformio.ini`, `.ioc`, `Makefile` with `arm-none-eabi` |

### Modules

| Module | Purpose |
|--------|---------|
| Embedded Project Explorer | პროექტის სტრუქტურა MCU ტარგეტებით |
| Flash Tool | firmware upload (OpenOCD, esptool, avrdude) |
| Serial Monitor | UART/USB-Serial კომუნიკაცია |
| Binary Inspector | .elf / .hex / .bin ანალიზი |
| Peripheral Viewer | MCU რეგისტრები და პერიფერია (SVD-based) |

### Tools

* OpenOCD — on-chip debugging
* GDB — debug adapter (ARM, RISC-V)
* UART terminal — serial I/O

### Optional

* Register viewer — CPU/peripheral register state
* Memory map inspector — flash/RAM layout visualization
* Logic analyzer integration — signal timing

---

## 2. Embedded Linux / SBC

| | |
|---|---|
| **Target** | Raspberry Pi, Milk-V, BeagleBone, ARM Linux boards |
| **Activation** | `buildroot`, `yocto`, cross-compile toolchain detection |

### Modules

| Module | Purpose |
|--------|---------|
| Remote Deploy | binary upload + restart on target |
| SSH Terminal | integrated remote shell |
| System Monitor | CPU/RAM/disk on target device |
| Cross-Compiler Manager | toolchain selection (aarch64, armhf, riscv64) |

### Tools

* SSH — remote connection
* SCP/rsync — file transfer
* Remote GDB — cross-debug

---

## 3. C++ Native Development

| | |
|---|---|
| **Activation** | `CMakeLists.txt`, `Makefile`, `.cpp`/`.h` files |

### Modules

| Module | Purpose |
|--------|---------|
| C++ Workspace | project tree, include paths, compile_commands.json |
| Symbol Navigator | class/function/variable index (CodeGraph) |
| Header Explorer | include graph visualization |
| Build Manager | CMake configure/build/clean |

### Tools

* CMake + Ninja — build system
* GCC / Clang — compilers
* Clangd — language intelligence (LSP)

### Features

* Code navigation (F12, references, rename)
* Compile diagnostics (inline errors/warnings)
* Symbol indexing (CodeGraph SQLite)
* Header/source switching

---

## 4. Rust Development

| | |
|---|---|
| **Activation** | `Cargo.toml`, `.rs` files |

### Modules

| Module | Purpose |
|--------|---------|
| Cargo Integration | build, run, test, check, clippy |
| Crate Explorer | dependency tree browser |
| Rust Analyzer Integration | LSP client for rust-analyzer |

### Tools

* cargo — build/package manager
* rustc — compiler
* rust-analyzer — language server

### Features

* Dependency graph visualization
* Crate browser with docs
* Inline type hints
* Borrow checker diagnostics

---

## 5. Systems Programming

| | |
|---|---|
| **Target** | Kernels, drivers, bootloaders, low-level runtime |
| **Activation** | Kernel `Kconfig`, driver `Makefile`, bare-metal `linker.ld` |

### Modules

| Module | Purpose |
|--------|---------|
| Memory Viewer | hex editor with address mapping |
| Disassembler | inline disassembly (x86, ARM, RISC-V) |
| Register View | CPU register state during debug |
| Binary Explorer | ELF/PE section browser |
| Linker Script Editor | memory layout editing |

### Tools

* GDB with target remote
* objdump / readelf
* QEMU (optional emulation)

---

## 6. Game Development

| | |
|---|---|
| **Activation** | Engine project files, asset directories |

### Modules

| Module | Purpose |
|--------|---------|
| Asset Explorer | textures, models, audio browser |
| Scene Viewer | 3D/2D scene tree inspector |
| Shader Editor | GLSL/HLSL with preview |
| Performance Profiler | frame timing, draw calls |

### Tools

* C++ / scripting runtime
* Graphics debugger integration

### Optional

* Engine integrations (custom engine support via plugins)
* Animation timeline
* Physics debugger

---

## 7. Qt Application Development

| | |
|---|---|
| **Activation** | `CMakeLists.txt` with `find_package(Qt6)`, `.pro` files, `.ui` files |

### Modules

| Module | Purpose |
|--------|---------|
| Qt Project Explorer | QObject tree, resource browser |
| Signal/Slot Inspector | connection graph viewer |
| UI Designer Integration | .ui file preview |
| Qt Resource Viewer | .qrc resource browser |

### Tools

* CMake / qmake — build
* MOC automation — meta-object compiler
* Qt Designer — UI layout

### Features

* Signal/slot connection analysis
* QML support (optional)
* Qt documentation integration

---

## 8. Python Development

| | |
|---|---|
| **Activation** | `requirements.txt`, `pyproject.toml`, `setup.py`, `.py` files |

### Modules

| Module | Purpose |
|--------|---------|
| Virtualenv Manager | venv/conda environment switching |
| Package Explorer | installed packages browser |
| Python Debugger | breakpoints, stepping, REPL |
| Test Runner | pytest/unittest integration |

### Tools

* pip — package manager
* pytest — test framework
* Python LSP (Pylance/Pyright)

---

## 9. PHP Development

| | |
|---|---|
| **Activation** | `composer.json`, `.php` files |

### Modules

| Module | Purpose |
|--------|---------|
| PHP Project Explorer | namespace/class tree |
| API Tester | HTTP request builder |
| Database Browser | MySQL/PostgreSQL viewer |
| Composer Integration | dependency management |

### Tools

* Composer — package manager
* PHPUnit — testing
* Xdebug — debug adapter

---

## 10. Web Backend Development

| | |
|---|---|
| **Target** | Node.js, Go, Python, PHP, Rust backends |
| **Activation** | `package.json` (server), `go.mod`, `manage.py`, `composer.json` |

### Modules

| Module | Purpose |
|--------|---------|
| REST Client | HTTP request builder/runner |
| API Debugger | request/response inspector |
| Database Tools | SQL editor, query results, schema browser |
| Environment Manager | .env file editor, variable viewer |

### Tools

* Language-specific tools (loaded by sub-profile)
* Docker integration (optional)

---

## 11. Web Frontend Development

| | |
|---|---|
| **Activation** | `package.json` with frontend deps, `.html`/`.css`/`.jsx`/`.tsx` |

### Modules

| Module | Purpose |
|--------|---------|
| HTML Preview | live browser preview |
| CSS Inspector | style analysis |
| JS Console | browser dev console |
| Component Explorer | React/Vue/Svelte component tree |

### Tools

* npm / yarn / pnpm — package managers
* Bundlers (Vite, webpack)
* Browser DevTools bridge

---

## 12. Full Stack Development

| | |
|---|---|
| **Activation** | Combined frontend + backend project structure |

### Modules

Combines **Web Backend** (#10) + **Web Frontend** (#11) profiles, plus:

| Module | Purpose |
|--------|---------|
| API Testing | endpoint testing with live backend |
| Container Runner | Docker compose for full stack |
| Database Integration | schema + data management |

---

## 13. Data Engineering

| | |
|---|---|
| **Activation** | `.sql` files, database config, ETL scripts |

### Modules

| Module | Purpose |
|--------|---------|
| SQL Editor | syntax highlighting, autocomplete, execution |
| Query Profiler | EXPLAIN analysis, performance hints |
| Data Browser | table/view browser with inline editing |
| Schema Designer | visual schema editor |

### Tools

* PostgreSQL, MySQL, SQLite clients
* Data pipeline tools (dbt, Airflow)
* CSV/Parquet viewer

---

## 14. AI / Machine Learning

| | |
|---|---|
| **Activation** | `requirements.txt` with ML deps, `.ipynb`, model files |

### Modules

| Module | Purpose |
|--------|---------|
| Model Explorer | neural network architecture viewer |
| Dataset Viewer | data preview, statistics, sampling |
| Training Monitor | loss curves, metrics dashboard |
| Notebook Runner | Jupyter-compatible cell execution |

### Tools

* Python + pip
* CUDA toolkit detection
* TensorBoard integration

---

## 15. Blockchain Development

| | |
|---|---|
| **Activation** | `hardhat.config.js`, `foundry.toml`, `Anchor.toml` |

### Modules

| Module | Purpose |
|--------|---------|
| Wallet Explorer | account/balance viewer |
| Contract Inspector | ABI browser, state reader |
| Node Monitor | blockchain node status |
| Transaction Debugger | tx trace/step viewer |

### Tools

* Solidity / Vyper compiler
* Foundry / Hardhat
* Local chain runner (Anvil, Ganache)

---

## 16. Security Research

| | |
|---|---|
| **Activation** | Manual selection |

### Modules

| Module | Purpose |
|--------|---------|
| Packet Inspector | network traffic analyzer |
| Binary Analyzer | static analysis, strings, imports |
| Exploit Workspace | payload builder, shellcode tools |
| Fuzzer Integration | coverage-guided fuzzing |

### Tools

* GDB — debugging
* Disassembler — instruction analysis
* Hex editor — binary patching

---

## 17. Reverse Engineering

| | |
|---|---|
| **Activation** | Manual selection, binary file open |

### Modules

| Module | Purpose |
|--------|---------|
| Disassembly Viewer | interactive disassembly (x86, ARM, RISC-V) |
| Symbol Explorer | import/export/string tables |
| Binary Navigation | section/segment browser |
| Control Flow Graph | function CFG visualization |
| Decompiler View | pseudo-C output (optional) |

---

## 18. DevOps / Infrastructure

| | |
|---|---|
| **Activation** | `Dockerfile`, `docker-compose.yml`, `terraform/`, `ansible/`, `k8s/` |

### Modules

| Module | Purpose |
|--------|---------|
| Container Manager | Docker build/run/logs |
| Server Monitor | remote host status |
| Deployment Console | CI/CD pipeline viewer |
| Config Editor | YAML/TOML/HCL with schema validation |

### Tools

* Docker / Podman
* SSH
* Terraform / Ansible (optional)

---

## 19. Automation / Scripting

| | |
|---|---|
| **Activation** | `.lua` files, shell scripts, `Taskfile.yml` |

### Modules

| Module | Purpose |
|--------|---------|
| Lua Scripting Environment | LuaJIT REPL + script runner |
| Automation Console | task execution + output |
| Script Library | reusable script browser |

### Tools

* LuaJIT — fast Lua runtime
* Shell scripting (bash/PowerShell)
* Task runner integration

---

## 20. Lightweight Text Editing

| | |
|---|---|
| **Activation** | Default — when no project is loaded, or explicit selection |

### Modules

| Module | Purpose |
|--------|---------|
| Basic Text Editor | syntax highlighting, find/replace |
| Search | file search, text search |
| File Browser | simple directory tree |

### NOT loaded

* Language indexing (CodeGraph, LSP)
* Build system
* Debug adapter
* Git integration
* Terminal

### Goal

**Ultra-fast startup.** ნულოვანი overhead. ტექსტის რედაქტორი მხოლოდ.

---

## Profile Activation Model

პროფილები აქტიურდება სამი გზით:

| Method | Description | Example |
|--------|-------------|---------|
| **Project Detection** | ფაილების ავტოდეტექცია | `CMakeLists.txt` → C++ Profile |
| **File Detection** | გახსნილი ფაილის ტიპი | `.rs` → Rust Profile |
| **Manual Selection** | მომხმარებლის არჩევანი | Command Palette → "Switch Profile" |

### Activation Flow

```
Open Folder
  → Scan root files
  → Match profile rules
  → Load required modules
  → Activate UI contributions
```

### Example: C++ Profile Activation

```
Detect: CMakeLists.txt found
  → Load: C++ Module
  → Load: Terminal Module
  → Load: Search Module
  → Load: Git Module
  → Load: Build Module
  → Load: Debug Module (deferred until first debug)
  → Skip: Python, Rust, Web, etc.
```

---

## Module Lifecycle

თითოეული მოდული გადის 4 ფაზას:

### 1. Load

```
→ Load DLL/shared library
→ Register services in ServiceRegistry
→ Register commands in CommandService
→ Expose extension interfaces
```

### 2. Activate

```
→ Create dock panels (via IDockManager)
→ Create toolbar items (via IToolBarManager)
→ Add menu entries (via IMenuManager)
→ Add status bar items (via IStatusBarManager)
→ Bind to events and signals
```

### 3. Deactivate

```
→ Unbind events
→ Hide UI elements
→ Save state to settings
→ Release active resources
```

### 4. Unload

```
→ Release all memory
→ Destroy UI controls
→ Unregister services
→ Unregister commands
```

**Zero-cost guarantee:** Deactivated/unloaded modules consume 0 CPU, 0 RAM, 0 background threads.

---

## Architecture Alignment

```
┌─────────────────────────────────────────┐
│              Exorcist IDE               │
│                                         │
│  ┌───────────────────────────────────┐  │
│  │           Core (Container)        │  │
│  │  Editor │ Terminal │ Search │ Git │  │
│  │  IDockManager │ IMenuManager     │  │
│  │  IToolBarManager │ IStatusBar    │  │
│  │  IWorkspaceManager              │  │
│  └───────────────────────────────────┘  │
│                                         │
│  ┌───────────────────────────────────┐  │
│  │      Profile (Capability Pack)    │  │
│  │  profile.json → module list      │  │
│  │  activation rules                │  │
│  │  default settings                │  │
│  └───────────────────────────────────┘  │
│                                         │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐  │
│  │ Mod1 │ │ Mod2 │ │ Mod3 │ │ Mod4 │  │
│  │Plugin│ │Plugin│ │Plugin│ │Plugin│  │
│  └──────┘ └──────┘ └──────┘ └──────┘  │
└─────────────────────────────────────────┘
```

### Advantages

| Benefit | How |
|---------|-----|
| Minimal memory | მხოლოდ საჭირო მოდულები იტვირთება |
| Fast startup | 20-ივე პროფილი არ იტვირთება — მხოლოდ 1 |
| Clean architecture | Core = interfaces, Profiles = packs, Modules = plugins |
| Modular expansion | ახალი პროფილი = ახალი JSON + არსებული მოდულების კომბინაცია |
| User freedom | მომხმარებელი ირჩევს რა სჭირდება — არაფერი ზედმეტი |
