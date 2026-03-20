# Exorcist IDE — Development Profiles and Template Catalog

Canonical rollout/migration reference:
- [template-roadmap.md](template-roadmap.md)

This document separates three concepts that were previously mixed together:

- `shell`: the IDE container and abstract host surfaces
- `profile`: a workspace-activated capability pack
- `template`: a target development environment shape built from shared components and plugins

## Model

```text
IDE Shell        = container + interfaces + bridge services
Shared Layers    = reusable services + reusable components + base plugin helpers
Profiles         = activation manifests for a workspace
Templates        = full workbench outcomes assembled from profiles/plugins/components
Plugins          = the feature owners
```

The shell does not own feature workflows. It exposes `IHostServices`,
`IDockManager`, `IMenuManager`, `IToolBarManager`, `IStatusBarManager`,
`IWorkspaceManager`, component services, and profile services.

Implementation split in core:

- `SharedServicesBootstrap` owns workspace settings and profile activation services.
- `TemplateServicesBootstrap` owns the project template catalog and built-in template provider.

## Current vs Target

Current bundled profiles in `profiles/`:

| Profile | Status | Purpose |
|---|---|---|
| `cpp-native` | Implemented | Native C/C++ workbench |
| `embedded-linux` | Implemented scaffold | Cross-compiled Linux and SBC activation |
| `embedded-mcu` | Implemented scaffold | MCU-oriented activation |
| `lightweight` | Implemented | Minimal text-editing mode |
| `automation` | Implemented scaffold | Task-driven scripting activation |
| `devops` | Implemented scaffold | Infrastructure and deployment activation |
| `python` | Implemented | Python-oriented activation |
| `qt-app` | Implemented | Qt application activation |
| `rust` | Implemented | Rust-oriented activation |
| `web-frontend` | Implemented scaffold | Frontend web activation |

Target catalog:

- 20 workbench templates
- each template mapped to one or more profiles
- each profile activates plugins, shared components, dock defaults, menus, toolbars, and settings
- templates are considered complete only when their full plugin-owned UI and toolchain workflow work end-to-end

## Activation Contract

Profiles activate through:

| Method | Description |
|---|---|
| Project detection | root files or folders such as `CMakeLists.txt`, `Cargo.toml`, `pyproject.toml` |
| File detection | currently opened file types such as `.cpp`, `.rs`, `.py` |
| Manual switch | command palette or workspace settings |
| Contextual escalation | extra plugin activation only when a workflow starts, such as debug or deploy |

Activation flow:

```text
Open workspace
  -> detect profile score
  -> activate matching profile
  -> load required plugins
   -> force-activate profile-listed deferred plugins when a manual profile switch needs them
  -> restore plugin-owned docks/toolbars/menus
  -> apply profile defaults where workspace overrides do not exist
  -> defer optional plugins until first use
```

## Lifecycle Rules

For every profile/plugin combination:

1. `load`
   Load plugin binary, register services, commands, manifests, contribution metadata.
2. `activate`
   Create plugin-owned docks, menus, toolbars, status entries, event subscriptions.
3. `deactivate`
   Unbind events, hide UI, persist state, stop background work.
4. `unload`
   Release plugin memory and unregister feature ownership cleanly.

The shell remains alive throughout; feature ownership moves in and out through interfaces.

## Shared Building Blocks

Templates must be composed from these layers instead of copied per plugin:

| Layer | Responsibility |
|---|---|
| Shell | window/container lifecycle, docking rails, menu bar, toolbar rails, status bar, workspace/session orchestration |
| Shared services | settings, profiles, command routing, process helpers, diagnostics aggregation, task/build session primitives, logging |
| Shared components | project tree, terminal, serial monitor, output viewer, generic explorers, inspectors |
| Base plugin layer | `WorkbenchPluginBase`-style helpers for commands, menus, toolbars, docks, permissions, lifecycle |
| Domain plugins | language/build/debug/test/device/web/data/AI/etc. ownership |

## Top 20 Template Catalog

The following 20 templates are the target end state for the workbench.

### 1. Embedded / MCU

- Activation: `platformio.ini`, `.ioc`, embedded cross-toolchain files
- Required capabilities: project explorer, flash/upload, serial monitor, binary inspector, peripheral/register views
- Target plugins/components: embedded, build, debug, serial monitor, terminal, project tree
- Current bundled coverage: `org.exorcist.embedded-tools` provides embedded action entry points, recursive workspace detection and command inference for PlatformIO, ESP-IDF, Zephyr, pyOCD configs, OpenOCD configs, STM32CubeProgrammer flash scripts, and Makefile-driven MCU projects, including PlatformIO environment plus upload/monitor port and baud defaults, pyOCD target-aware flash commands, OpenOCD interface/target-aware summaries, and runnable debug-route commands in the embedded tools panel, with persisted per-workspace flash/monitor command overrides when developers need to replace inferred defaults. `org.exorcist.serial-monitor` provides the monitor dock, persisted port/baud/newline preferences, optional timestamped logs, live serial session controls when `Qt SerialPort` is available, and an external monitor launcher fallback. Advanced device flashing/debug adapters remain follow-up work.

### 2. Embedded Linux / SBC

- Activation: `buildroot`, `yocto`, remote board toolchains
- Required capabilities: remote deploy, SSH terminal, remote monitor, cross-compiler manager
- Target plugins/components: remote, build, debug, terminal, project tree
- Current bundled coverage: `org.exorcist.remote` provides the SSH/remote dock, can activate explicitly for the `embedded-linux` profile, and now also lazy-activates from Buildroot/Yocto/rootfs workspace markers; bundled profile detection covers Buildroot directories plus Yocto-style `bblayers.conf`, `local.conf`, `meta-*`, and recipe markers recursively. Remote deploy/debug workflow depth remains follow-up work.

### 3. C++ Native

- Activation: `CMakeLists.txt`, `Makefile`, `.cpp`, `.h`
- Required capabilities: code navigation, build, run, debug, tests, symbol graph, include graph
- Target plugins/components: cpp-language, build, debug, testing, codegraph, terminal, output, project tree

### 4. Rust

- Activation: `Cargo.toml`, `.rs`
- Required capabilities: cargo build/run/test/check, crate explorer, rust-analyzer, diagnostics
- Target plugins/components: rust language plugin, build/task integration, testing, terminal

### 5. Systems Programming

- Activation: `linker.ld`, kernel/driver build markers, low-level toolchains
- Required capabilities: memory view, disassembly, register view, binary explorer, linker-script tooling
- Target plugins/components: debug, binary tools, build, terminal, project tree

### 6. Game Development

- Activation: engine project markers, asset trees
- Required capabilities: asset explorer, scene inspector, shader editor, performance/profiling views
- Target plugins/components: asset tools, build, debug, test, project tree, terminal

### 7. Qt Application Development

- Activation: Qt `find_package`, `.pro`, `.ui`, `.qrc`
- Required capabilities: QObject/resource explorer, signal-slot inspection, UI preview/designer hooks
- Target plugins/components: cpp-language, build, debug, qt tools, project tree

### 8. Python

- Activation: `pyproject.toml`, `requirements.txt`, `.venv`, `.py`
- Required capabilities: interpreter/venv selection, package explorer, debugger, test runner
- Target plugins/components: python language plugin, terminal, testing, debug, project tree

### 9. PHP

- Activation: `composer.json`, `.php`
- Required capabilities: namespace/class explorer, composer, debugger, API/database tools
- Target plugins/components: php language plugin, terminal, testing, API/database tools

### 10. Web Backend

- Activation: `package.json`, `go.mod`, `manage.py`, `composer.json`
- Required capabilities: REST client, API debugger, environment manager, database tools
- Target plugins/components: backend language plugins, terminal, testing, database/API components

### 11. Web Frontend

- Activation: frontend `package.json`, `.html`, `.css`, `.jsx`, `.tsx`
- Required capabilities: preview, console, component explorer, bundler tasks
- Target plugins/components: web language plugin, terminal, testing, preview tools

### 12. Full Stack

- Activation: frontend + backend markers together
- Required capabilities: full stack runner, API testing, database integration, container orchestration
- Target plugins/components: backend + frontend packs, remote/container tools, testing

### 13. Data Engineering

- Activation: SQL roots, dbt/Airflow markers, ETL scripts
- Required capabilities: SQL editor, profiler, data browser, schema tools
- Target plugins/components: database plugin, terminal, data viewers

### 14. AI / Machine Learning

- Activation: notebook/model/data markers, ML requirements
- Required capabilities: environment switching, dataset/model views, notebook execution, training monitor
- Target plugins/components: python, notebook, terminal, data viewers, profiling

### 15. Blockchain

- Activation: `hardhat.config.*`, `foundry.toml`, `Anchor.toml`
- Required capabilities: contract explorer, node monitor, transaction/debug tools, wallet views
- Target plugins/components: blockchain plugin, build/run/debug, terminal

### 16. Security Research

- Activation: manual or lab workspace selection
- Required capabilities: packet inspection, binary analysis, exploit workspace, fuzzing integration
- Target plugins/components: binary tools, debug, terminal, remote, dataset/log viewers

### 17. Reverse Engineering

- Activation: binary-first workspace or manual selection
- Required capabilities: disassembly, symbol explorer, CFG, optional decompiler
- Target plugins/components: reverse-engineering plugin, binary tools, debug

### 18. DevOps / Infrastructure

- Activation: `Dockerfile`, compose, terraform, ansible, `k8s/`
- Required capabilities: container manager, deployment console, server monitor, config editor
- Target plugins/components: remote, container/devops plugin, terminal, logs/output

### 19. Automation / Scripting

- Activation: `.lua`, shell scripts, `Taskfile.yml`
- Required capabilities: script runner, task console, reusable script library
- Target plugins/components: Lua/plugin SDK, terminal, task runner

### 20. Lightweight Text Editing

- Activation: default fallback or explicit minimal mode
- Required capabilities: fast editor, file browser, search
- Must not load: heavy indexing, language servers, build/debug/test stacks unless explicitly requested
- Target plugins/components: shell + editor + search + project tree only

## Completion Criteria for a Template

A template is not complete because a JSON profile exists. It is complete only when:

- detection works
- required plugins activate correctly
- menus and toolbars are plugin-owned
- required shared components appear in the correct dock zones
- build/run/debug/test workflow works where applicable
- switching away deactivates unneeded capabilities cleanly
- workspace defaults and user overrides coexist correctly

## Rollout Strategy

The 20 templates should be delivered in waves, not independently:

| Wave | Templates |
|---|---|
| Wave 1 | 3 C++ Native, 7 Qt, 8 Python, 4 Rust, 20 Lightweight |
| Wave 2 | 1 Embedded MCU, 2 Embedded Linux, 18 DevOps, 19 Automation |
| Wave 3 | 10 Web Backend, 11 Web Frontend, 12 Full Stack, 13 Data Engineering |
| Wave 4 | 5 Systems, 6 Game Dev, 14 AI/ML, 15 Blockchain |
| Wave 5 | 16 Security Research, 17 Reverse Engineering, remaining polish and cross-template consistency |

## Standardization Requirements

- The shell must remain a container only.
- Shared UI must move into shared components instead of being reimplemented in plugins.
- Shared non-visual logic must move into shared services instead of being copied.
- Plugins own all visible feature workflows.
- Profiles select combinations of plugins/components; they do not contain feature logic.
- External third-party plugins must integrate through the same SDK and ownership rules as bundled plugins.
