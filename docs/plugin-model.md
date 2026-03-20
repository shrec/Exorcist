# Exorcist Plugin Model

This is the canonical working document for bundled plugins and external
third-party plugins.

## Plugin Contract

Plugins integrate through SDK interfaces only.
Plugins are not just adapters; they are the concrete owners of feature systems.

They must not depend on:

- `MainWindow`
- internal dock/widget implementations
- concrete subsystem classes outside the public SDK/contracts

Primary host boundary:

- `IHostServices`
- `ServiceRegistry`
- `IDockManager`
- `IMenuManager`
- `IToolBarManager`
- `IStatusBarManager`
- `IWorkspaceManager`
- component/profile services

## End-to-End Ownership Rule

The IDE shell is a container plus abstract host surface. Plugins own the rest.

If a feature has any of the following, the default owner is a plugin:

- workflow logic
- runtime/session state
- commands and command handlers
- menus, toolbars, docks, status items
- settings pages and activation rules
- provider/process/integration lifecycle

Core may expose the interfaces, bridges, and shared primitives those plugins use,
but it must not absorb the domain ownership itself.

## Standard Plugin Layers

Every plugin should fit one of these roles:

| Role | Responsibility |
|---|---|
| Provider plugin | backend/provider integration only |
| Workbench plugin | owns menus, toolbars, commands, docks, workflow UI |
| Component-backed plugin | orchestrates shared components without owning low-level UI implementation |
| Headless plugin | services, tasks, analyzers, integrations with no direct UI |

## Base Class Direction

Workbench-facing plugins should use the shared base plugin layer
(`WorkbenchPluginBase` or equivalent) so ownership rules stay consistent.

Language and tooling plugins should go one step further and inherit a more
specialized abstract base when they share the same standard workbench surface.
Current direction:

- `WorkbenchPluginBase` for generic workbench-facing plugins
- `LanguageWorkbenchPluginBase` for language plugins that share project tree,
  problems, terminal, search, git, workspace-root, and language-service glue
- process-based language plugins should reuse the shared language-server
  lifecycle helpers in the language workbench base instead of duplicating
  registration/startup wiring

The base layer should standardize:

- initialization and shutdown hooks
- command creation helpers
- menu contribution helpers
- toolbar contribution helpers
- dock/view registration helpers
- permission/capability declarations

This is not optional once patterns repeat.

If multiple plugins need the same standard IDE-facing behavior, the correct
solution is to move that behavior into a shared abstract plugin base or shared
service layer, not to duplicate it in each plugin.

Typical examples of base-layer responsibilities:

- workspace root and active-editor access
- project-tree and standard dock visibility helpers
- status/notification helpers
- command registration and execution wrappers
- standard service lookup wrappers
- common language-plugin startup/shutdown glue

Concrete plugins should stay focused on domain behavior only:

- C++ specifics stay in the C++ plugin
- C# specifics stay in the C# plugin
- Rust specifics stay in the Rust plugin
- shared workbench mechanics stay in the base layer

## UI Ownership Rules

- Plugin-owned UI is strict.
- Each plugin owns its own commands.
- The same command ID should back command palette, keybindings, menus, and toolbars.
- The shell must not duplicate plugin actions.
- Toolbars must be created by plugins, not by the shell.
- Domain menus belong to domain plugins.
- Concrete feature behavior belongs to the plugin that owns the feature.

Standard workbench order:

`File -> Edit -> View -> Git -> Project -> Build -> Debug -> Test -> Analyze -> Run -> Terminal -> Tools -> Extensions -> Window -> Help`

## Shared Reuse Rules

If multiple plugins need the same thing:

- move non-visual logic to shared services
- move reusable UI to shared components
- move lifecycle/UI helper code to the base plugin layer

If the same helper logic exists in two plugins already, adding a third copy is
an architecture regression.

Do not solve repetition by copy-pasting code into multiple plugins.

For language and tooling plugins specifically:

- prefer a specialized abstract base class over per-plugin boilerplate
- keep standard IDE tooling access centralized
- keep language-specific behavior in the concrete plugin only

## External Plugin Policy

Third-party plugins must follow the same model as bundled plugins:

- same SDK boundary
- same menu/toolbar/dock ownership rules
- same manifest and activation model
- same permission model
- same profile compatibility rules

Bundled plugins must not use private shortcuts that external plugins cannot use.

## Manifest Expectations

Each plugin should declare:

- `id`, `name`, `version`
- `author`, `license`, `layer`, `category`
- activation events or eligibility conditions
- requested permissions
- contributions
- optional profile/template affinity metadata

Canonical manifest values:

- `layer`: `cpp-sdk`, `c-abi`, `lua-jit`, `dsl`
- `views[].location`: `sidebar-left`, `sidebar-right`, `bottom`, `top`

Validation path:

- `python tools/validate_plugin_manifests.py`

Plugins should be loadable, activatable, deactivatable, and unloadable without
leaving orphaned UI or services behind.

## Lifecycle

1. `load`
   Discover metadata, validate API compatibility, load binary/runtime.
2. `initialize`
   Register services, commands, contributions.
3. `activate`
   Materialize menus, toolbars, docks, and runtime bindings when needed.
4. `deactivate`
   Stop work, unbind events, hide/release UI.
5. `shutdown`
   Unregister and free all owned resources.

## Current Standardization Targets

Near-term plugin-management cleanup:

1. normalize bundled plugins onto the base workbench layer
2. move reusable panels out of feature plugins into shared components
3. document stable external plugin authoring paths
4. eliminate remaining shell-owned feature UI
5. keep plugin/profile/template terminology consistent across docs

## Source of Truth

For overall architecture:
- [system-model.md](system-model.md)

For target workbench environments and rollout:
- [template-roadmap.md](template-roadmap.md)
