# Architecture

The goal is strict modularity, portability, and minimal footprint. The code is
organized into layers with clear dependency direction.

## Layers
1) Core interfaces (`src/core`): pure abstractions for filesystem, process,
   terminal, and network. No UI dependencies.
2) Platform bindings (`src/core/qt*`): concrete implementations using Qt
   primitives. These are replaceable.
3) UI + features (`src`): editor UI and tools depend only on Core interfaces.
4) Plugins (`plugins`): external modules depend on the stable API only.

## Rules
- UI must not call OS APIs directly.
- Only Core implementations may touch Qt/OS facilities.
- Plugins never include internal headers.

## Target Outcome
Any platform port should require only replacing Core bindings without touching
UI or plugin layers.
