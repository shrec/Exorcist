# Plugin UI Standards

This document now defers to the canonical plugin and system model to prevent
duplicate UI ownership rules from drifting.

## Canonical Sources

- Plugin contract and ownership: [plugin-model.md](plugin-model.md)
- Shell/container boundaries: [system-model.md](system-model.md)

## Current UI Rules

- the shell exposes empty workbench rails and interfaces
- plugins own visible feature UI
- standard workbench menu order must be preserved
- toolbars are plugin-owned, not shell-owned
- reusable panels belong in shared components, not duplicated plugin code
- profile defaults control which plugin-owned panels appear by default
