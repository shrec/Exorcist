# Architecture

This file is now a thin index to avoid architectural drift between multiple
long-form documents.

## Canonical Sources

- System architecture and layering: [system-model.md](system-model.md)
- Plugin ownership and external plugin contract: [plugin-model.md](plugin-model.md)
- Template/profile rollout and 20-template target: [template-roadmap.md](template-roadmap.md)

## Current Summary

- `MainWindow` is a shell/container only.
- The IDE shell owns interfaces, bridge access, lifecycle orchestration, and other abstract host responsibilities only.
- Shared behavior should be grouped into shared services and shared components.
- Workbench-facing plugins should use the shared base plugin layer.
- Domain plugins own their commands, menus, toolbars, docks, runtime state, and workflow UI end to end.
- Profiles assemble capability packs for workspaces.
- Templates are the final workbench outcomes built from profiles/plugins/components.

Refer to the canonical documents above for detailed rules and migration phases.
