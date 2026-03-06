# Roadmap

This roadmap is optimized for self-hosting: Exorcist should be able to build
and develop itself as early as possible.

## Phase 0: Foundation
- Stable repo structure (CMake, Qt 6, presets).
- Core UI shell: main window, tabs, dock panels.
- Plugin API + service registry.

## Phase 1: Minimal Editor Core (Self-hosting v0)
- Project tree + open file in editor tab.
- File read/write via core file abstraction.
- Simple find/replace (in file).
- Build task runner (local).

## Phase 2: Language Intelligence (Self-hosting v1)
- LSP client integration.
- C/C++ language server support (clangd).
- Diagnostics + go-to-definition + completion.

## Phase 3: Terminal + Tasks
- Terminal panel backend.
- Task profiles (build/test/run).
- Output capture + problem matcher basics.

## Phase 4: Git + VCS
- Git status + diff viewer.
- Stage/unstage + commit.

## Phase 5: Remote Build + SSH
- SSH profiles.
- Remote build/run + log streaming.
- Optional file sync.

## Phase 6: AI Assistant
- AI provider interface.
- Local + remote provider plugins.
- Inline and panel prompts.

## Phase 7: Polishing
- Settings UI.
- Theming.
- Performance profiling + memory targets.
