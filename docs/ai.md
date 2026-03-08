# AI Framework

Exorcist-ის AI subsystem უნდა იყოს provider-neutral platform, სადაც Copilot, Claude, Codex და future backends მუშაობენ ერთსა და იმავე runtime-ზე.

დეტალური refactor blueprint იხილე: `docs/agent-platform-refactor.md`
Provider plugin authoring guide: `docs/agent-provider-plugin-guide.md`

## Core principles

- Core owns the agent platform.
- Providers live in plugins.
- Tools are shared across all providers.
- Chat UI depends on normalized session/request/response models, not on vendor SDK details.

## Current foundations in code

- `src/aiinterface.h`
	generic provider contract.
- `src/agent/iagentplugin.h`
	plugin extension point for providers.
- `src/agent/agentorchestrator.h`
	active provider routing.
- `src/agent/itool.h`
	shared tool abstraction and registry.
- `src/agent/modelregistry.h`
	shared model metadata registry.

## Target architecture

- `Agent Platform Core`
	session lifecycle, request pipeline, provider registry, event model.
- `Shared Tool Runtime`
	tool registry, permissions, approval, execution, presentation.
- `Provider Plugins`
	auth, transport, model discovery, provider-specific parsing.
- `UI Surfaces`
	chat panel, quick chat, inline chat, transcript renderer.

## Provider responsibilities

- authentication
- model discovery
- request/stream transport
- response parsing
- optional provider-specific settings/auth/session import hooks

## Core responsibilities

- shared tool execution
- session persistence and restore
- request orchestration
- context assembly
- approval policies
- transcript/event normalization

## UX targets

- full panel chat
- inline chat
- inline completions
- restored/imported provider sessions
- provider-neutral tool visualization

## Important constraint

Copilot must be implemented as a plugin on top of the platform, not as the platform itself. The same runtime should serve Claude and Codex with minimal duplication.
