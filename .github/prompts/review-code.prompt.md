---
description: "Review code changes for Exorcist convention compliance, architecture alignment, and security"
agent: "reviewer"
argument-hint: "File or feature to review, e.g. 'src/core/isettings.h' or 'the new terminal backend'"
---
Review the specified code for:

## Convention Compliance
- PascalCase classes, `I`-prefix interfaces, `m_` member variables
- `#pragma once` headers, minimal includes, forward declarations
- Smart pointer ownership (`std::unique_ptr`, Qt parent/child)
- `QString *error` error reporting pattern
- `tr()` for user-facing strings
- Platform guards using `Q_OS_*` macros

## Architecture Alignment
- Correct layer placement (core vs plugin vs UI)
- Interface-based design (no concrete dependencies in wrong layers)
- Service registry usage (no direct plugin coupling)
- Roadmap phase compliance

## Security & Quality
- Resource management and cleanup
- Input validation at boundaries
- No hardcoded paths or credentials
- Dependency license compliance (MIT/BSD/public-domain)
