# Exorcist System Model

This is the canonical working document for Exorcist architecture, philosophy,
and layering.

## Core Rule

`IDE/MainWindow` is a container, not a feature owner.

The shell provides abstract surfaces, bridge access, and orchestration only:

- dock rails
- menu bar rails
- toolbar rails
- status bar rails
- workspace/session lifecycle
- service registry and bridge access
- plugin loading and profile activation

Feature workflows do not belong to the shell. The shell may host reusable surfaces
and abstract shared primitives, but concrete systems are owned by plugins.

## Strict Boundary

The IDE must behave like a container/host:

- core owns interfaces, contracts, bridge access, and reusable abstract basics
- core may host shared services/components only when they are generic and reusable
- core must not become the concrete owner of language, build, debug, test, VCS,
  remote, AI, or other end-user workflows
- plugins own those systems end to end: commands, menus, toolbars, docks,
  status items, runtime state, activation, and feature behavior
- being bundled does not make a feature core
- if a subsystem in `src/` is concrete and domain-owned, treat it as migration
  debt unless it is acting as a generic host surface

## Layered Architecture

```text
1. Shell / Container
   MainWindow, bootstrap adapters, runtime orchestration, bridge host surfaces

2. Shared Contracts and Bridge Services
   IHostServices, SDK interfaces, ServiceRegistry, profile/contracts/events

3. Shared Services
   reusable non-visual logic:
   settings, profiles, diagnostics aggregation, process helpers,
   task/build sessions, persistence, logging

4. Shared Components
   reusable visual building blocks:
   project tree, terminal, serial monitor, output/log viewers,
   generic explorers, inspectors, tool panels

5. Base Plugin Layer
   common plugin helpers for lifecycle, commands, menus, toolbars,
   docks, permissions, ownership conventions

6. Domain Plugins
   build, debug, testing, language packs, embedded, remote, web,
   data, AI providers, external third-party plugins
```

Dependencies flow downward through contracts only.

## Ownership Rules

- The shell must not own subsystem menus, toolbars, or workflow buttons.
- The shell must not own concrete feature runtimes or domain workflows.
- Every visible action belongs to a plugin or shared component owner.
- Every concrete feature system belongs to a plugin owner.
- Shared UI must not be copied across plugins.
- Shared non-visual logic must not be copied across plugins.
- Plugins communicate through interfaces, commands, services, and events.
- `MainWindow` must not grow new subsystem-specific state.

## What Lives Where

### Shell

- bootstrap and adapter wiring
- window/layout lifecycle
- profile activation orchestration
- plugin runtime and extension loading
- bridge access to shared cross-instance services
- abstract host surfaces only

### Shared Services

- profile manager
- workspace settings
- command routing
- diagnostics aggregation
- build/test session primitives
- logging and persistence helpers

### Shared Components

- terminal
- project tree
- serial monitor
- output/log panels
- generic explorers and inspectors

### Base Plugin Layer

- `WorkbenchPluginBase`
- common menu/toolbar helpers
- command-to-UI wiring helpers
- standard contribution conventions

### Domain Plugins

- language packs
- build/debug/test systems
- embedded/device tooling
- remote/devops/data tools
- AI providers and tool integrations
- all concrete feature workflows and workbench ownership

## Migration Direction

Current code already has the first steps:

- plugin-owned build/debug/language menus and toolbars
- `WorkbenchPluginBase`
- profile-driven activation
- shell toolbar removal

The remaining work is re-grouping, not inventing new features:

1. move reusable non-visual logic into shared services
2. move reusable visual panels into shared components
3. remove remaining subsystem ownership from shell/core glue
4. make bundled and third-party plugins follow the same model
5. finish all 20 target templates on top of this architecture

## Source of Truth

For plugin contracts and ownership rules:
- [plugin-model.md](plugin-model.md)

For template rollout and the 20 target environments:
- [template-roadmap.md](template-roadmap.md)
