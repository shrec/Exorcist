# AI Framework (Draft)

The AI subsystem is designed as a plugin-provided service so different backends
can be swapped without changing core UI.

## Scope (MVP)
- Provide a stable AI service interface.
- Allow multiple providers (local or cloud).
- Keep network access optional and explicit.

## Proposed Architecture
- `ServiceRegistry` exposes `ai.provider` service when available.
- Providers implement a minimal request/response interface.
- Core UI only depends on the interface, not a specific vendor.

Interface lives in `src/aiinterface.h`.

## Provider Types
- Local models (e.g., llama.cpp, ggml/gguf).
- Remote APIs (OpenAI-compatible, self-hosted).

## UX Targets
- Inline explain/refactor prompts.
- Panel chat for project-level questions.
- Optional code actions (format, docstrings, tests).

## Notes
- Keep prompts and outputs stored in-memory by default.
- Provide opt-in history persistence.
