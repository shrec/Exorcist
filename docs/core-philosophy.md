# Exorcist Core Philosophy

This historical document now points to the canonical working set.

## Core Idea

Exorcist stays fast and predictable by keeping the shell minimal and pushing
feature ownership into reusable shared layers and plugins.

## Canonical Sources

- Core/system model: [system-model.md](system-model.md)
- Plugin ownership model: [plugin-model.md](plugin-model.md)
- Profile/template rollout: [template-roadmap.md](template-roadmap.md)

## Still True

- bundled does not mean always active
- plugins must be controllable and context-aware
- the shell must remain a small container with interfaces and bridge access, not a feature owner
- core is responsible for abstract host concerns; plugins are responsible for concrete systems end to end
- performance and developer control remain first-class constraints

Use the canonical documents above for the current architecture and standards.
