# Exorcist Template Roadmap

This is the canonical working document for:

- profile/template rollout
- workbench reorganization phases
- the target end state of 20 development environment templates

## Current Priority

The project should not add scattered new features first.

The priority is:

1. reorganize existing code into the correct layers
2. standardize plugin ownership and shared reuse
3. finish profile-driven workbench assembly
4. land all 20 target templates on top of that structure

## Execution Phases

### Phase R1 — Shell Lockdown

Goal: keep `MainWindow` as a container only.

- no subsystem-specific toolbars in the shell
- no plugin command duplication in the shell
- no new feature state in `MainWindow`
- shell exposes only workbench surfaces and orchestration

### Phase R2 — Shared Layer Extraction

Goal: stop code duplication before adding more feature depth.

- extract shared non-visual logic into shared services
- extract reusable panels/widgets into shared components
- keep concrete domain behavior in plugins only

### Phase R3 — Plugin Standardization

Goal: one model for bundled and external plugins.

- normalize workbench plugins onto base plugin helpers
- standardize manifests, activation, permissions, contribution ownership
- ensure plugins clean up UI/services on deactivation

### Phase R4 — Profile Assembly

Goal: profiles become pure capability packs.

- detection rules score workspaces reliably
- required and deferred plugins activate correctly
- dock defaults and settings come from profiles
- user workspace overrides remain authoritative

### Phase R5 — Wave 1 Templates

- C++ Native
- Qt Application Development
- Python Development
- Rust Development
- Lightweight Text Editing

Definition of done:

- plugin-owned menus/toolbars/panels
- working build/run/debug/test flow where applicable
- profile auto-detection and correct default workbench layout

### Phase R6 — Wave 2 Templates

- Embedded / MCU Development
- Embedded Linux / SBC
- DevOps / Infrastructure
- Automation / Scripting

### Phase R7 — Wave 3 Templates

- Web Backend Development
- Web Frontend Development
- Full Stack Development
- Data Engineering

### Phase R8 — Wave 4 Templates

- Systems Programming
- Game Development
- AI / Machine Learning
- Blockchain Development

### Phase R9 — Wave 5 Templates

- Security Research
- Reverse Engineering
- cross-template polish
- external plugin ecosystem validation

## Target Template List

1. Embedded / MCU
2. Embedded Linux / SBC
3. C++ Native
4. Rust
5. Systems Programming
6. Game Development
7. Qt Application Development
8. Python
9. PHP
10. Web Backend
11. Web Frontend
12. Full Stack
13. Data Engineering
14. AI / Machine Learning
15. Blockchain
16. Security Research
17. Reverse Engineering
18. DevOps / Infrastructure
19. Automation / Scripting
20. Lightweight Text Editing

## Completion Criteria

A template is complete only when:

- detection works
- profile activation works
- plugin ownership is clean
- shared components/services are reused instead of duplicated
- workflow is usable end-to-end
- switching templates does not leave stale UI/state behind

## Supporting Docs

- [system-model.md](system-model.md)
- [plugin-model.md](plugin-model.md)
- [workbench-reorg-todo.md](workbench-reorg-todo.md)
