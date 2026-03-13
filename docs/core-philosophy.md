# Exorcist IDE — Core Philosophy

## Overview

Exorcist is designed around a simple principle:

**The IDE must serve the developer, not the other way around.**

Many modern IDEs become heavy, unpredictable, and intrusive due to uncontrolled plugin ecosystems and background services.
Exorcist solves this by separating the system into two clearly defined layers:

1. **Core IDE**
2. **Core Plugins**

This architecture ensures:

* predictable performance
* low memory usage
* immediate usability
* modular extensibility

---

## 1. Core IDE

### Definition

The **Core IDE** contains only the functionality that is universally useful to all developers and does not interfere with any workflow.

Core components must satisfy these rules:

* Always relevant
* Always lightweight
* Always predictable
* Never intrusive

If a feature does not meet these criteria, it **does not belong in the Core IDE**.

### Core Responsibilities

The Core IDE provides the fundamental development environment:

#### Editor

* text editing
* tabs
* syntax highlighting
* navigation

#### Workspace

* open folder / project
* project tree
* file operations

#### Navigation

* search in files
* symbol navigation
* go to definition / references

#### Interface

* docking system
* panels
* status bar
* layout control

#### Runtime Infrastructure

* plugin manager
* agent runtime
* tool runtime
* configuration system

#### Developer Utilities

* integrated terminal
* output/log panels
* notes / todo system

The **Core IDE must remain minimal and stable.**

---

## 2. Core Plugins

### Definition

**Core Plugins** are official plugins bundled with the IDE that provide extended functionality frequently required by developers.

However, they are:

* optional
* dynamically loaded
* fully disableable

A Core Plugin must follow the rule:

**Bundled ≠ Active**

Just because a plugin is installed does not mean it runs.

### Core Plugin Examples

#### Language Packs

* C/C++
* Rust
* Python
* PHP
* JavaScript

#### Tooling Packs

* CMake
* Qt development tools
* debugger integrations
* test runners

#### Integrations

* GitHub tools
* database tools
* container tools

#### Advanced Tools

* AI agent tools
* notebook support
* analysis tools

---

## 3. Plugin Activation Model

Plugins are activated in three ways:

### Manual Activation

The developer explicitly enables a plugin.

### Project Detection

Plugins activate automatically based on project files.

Example:

* `Cargo.toml` → enable Rust plugin
* `CMakeLists.txt` → enable CMake plugin

### Contextual Activation

Plugins load only when their functionality is used.

Example:
Debugger plugin loads only when debugging begins.

---

## 4. Performance Principles

Exorcist follows strict performance rules.

Plugins that are disabled must consume:

* **0 CPU**
* **0 background threads**
* **0 RAM**

Inactive plugins remain dormant until required.

This ensures the IDE remains lightweight regardless of installed plugins.

---

## 5. Built-in Ecosystem Strategy

Exorcist ships with a rich set of official plugins to eliminate common developer friction.

Developers should not need to search for essential functionality.

Installation flow:

```
Install IDE
Open project
Start working
```

Marketplace exploration is optional, not required.

---

## 6. Design Principles

Exorcist follows these core design principles:

### Minimal Core

The core must stay small, stable, and fast.

### Modular Expansion

New functionality belongs in plugins.

### Predictable Behavior

No hidden background processes or unpredictable loading.

### Developer Control

Developers choose what runs in their environment.

### Performance First

Responsiveness and stability take priority over feature count.

---

## 7. Long-Term Vision

Exorcist aims to become a modern development environment built around:

* native performance
* modular architecture
* developer-controlled tooling
* AI-assisted workflows

The architecture ensures the IDE can grow without sacrificing performance or simplicity.

---

**Exorcist IDE:
Powerful when needed.
Invisible when not.**
