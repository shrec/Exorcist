# Plugin API

This file is now a compatibility index. The plugin contract is split into two
canonical documents:

- overall plugin model: [plugin-model.md](plugin-model.md)
- overall system/layering context: [system-model.md](system-model.md)

## Current Direction

- Plugins integrate through `IHostServices` and SDK contracts.
- Workbench plugins should use the shared base plugin layer.
- Plugins own their commands, menus, toolbars, docks, and workflow UI.
- Shared services/components exist to prevent duplication across plugins.
- External third-party plugins follow the same contract as bundled plugins.

## Historical Note

Earlier drafts described a smaller `IPlugin`-only contract. The current
direction is broader: stable SDK interfaces, manifest-driven activation,
plugin-owned UI, and profile-driven workbench assembly.
