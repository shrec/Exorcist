# Exorcist

A fast, lightweight, cross-platform Qt 6 editor aiming for VS Code-level capabilities without the heavy footprint.

## Mission
- Start with a robust, minimal core (editor + project tree + search)
- Keep memory and CPU usage predictable
- Build a clean, contributor-friendly architecture for plugins and features

## Build

Requirements:
- Qt 6 (Widgets)
- CMake 3.21+
- C++17 compiler
- LLVM (optional, for future language tooling integration)

Configure and build:

```sh
cmake -S . -B build
cmake --build build
```

## Theme and Language
- Default theme: dark (Fusion palette).
- Translations: place `.qm` files in `translations/` and set `EXORCIST_LANG` if needed.

## Roadmap (high level)
- Core UI shell (main window, editor, panels)
- Project tree + file operations
- Syntax highlighting + language services (LSP)
- Git integration
- Extensions and theming

## License
MIT. See `LICENSE`.

## Dependencies
See `docs/dependencies.md` for optional library notes.

## Roadmap
See `docs/roadmap.md`.

## Architecture
See `docs/architecture.md`.

## Manifesto
See `docs/manifesto.md`.

## Search Framework
See `docs/search_framework.md`.

## LSP Lifecycle
See `docs/lsp_lifecycle.md`.
