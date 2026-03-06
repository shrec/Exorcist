---
description: "Evaluate an architectural decision or design proposal against Exorcist's principles"
agent: "architect"
argument-hint: "Design question, e.g. 'Should the LSP client be a core abstraction or a plugin?'"
---
Analyze the proposed design decision against Exorcist's architecture principles:

1. **Interface-first**: Does it define abstract interfaces before implementations?
2. **Plugin-first**: Should this be a plugin or a core abstraction?
3. **Loose coupling**: Does it use ServiceRegistry instead of direct dependencies?
4. **Layered architecture**: Are dependencies flowing downward only?
5. **Minimal footprint**: Does it avoid unnecessary dependencies?
6. **Roadmap alignment**: Which phase does this belong to?

Reference:
- [Architecture docs](docs/plugin_api.md)
- [AI strategy](docs/ai.md)
- [Roadmap](docs/roadmap.md)
- [Dependencies policy](docs/dependencies.md)

Provide a clear recommendation with rationale and risk assessment.
