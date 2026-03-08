# AI Framework

Exorcist-ის AI subsystem არის provider-neutral platform, სადაც Copilot, Claude, Codex და future backends მუშაობენ ერთსა და იმავე runtime-ზე.

დეტალური refactor blueprint იხილე: `docs/agent-platform-refactor.md`
Provider plugin authoring guide: `docs/agent-provider-plugin-guide.md`
Project Brain (persistent knowledge): `docs/project-brain.md`

## Core principles

- Core owns the agent platform.
- Providers live in plugins.
- Tools are shared across all providers.
- Chat UI depends on normalized session/request/response models, not on vendor SDK details.

## Implemented architecture

### Agent Platform Core (`src/agent/`)

| Component | File(s) | Purpose |
|-----------|---------|---------|
| `AgentController` | `agentcontroller.h/.cpp` | Per-session request orchestrator, tool loop, multi-turn |
| `AgentOrchestrator` | `agentorchestrator.h/.cpp` | Compatibility facade → registry + router |
| `AgentProviderRegistry` | `agentproviderregistry.h/.cpp` | Provider list + active selection |
| `AgentRequestRouter` | `agentrequestrouter.h/.cpp` | Request dispatch + signal forwarding |
| `ChatSessionService` | `chatsessionservice.h/.cpp` | Session lifecycle facade over SessionStore |
| `ToolApprovalService` | `toolapprovalservice.h/.cpp` | Centralized tool approval policy |
| `AgentPlatformBootstrap` | `agentplatformbootstrap.h/.cpp` | Creates all services, wires registry, discovers plugin extensions |
| `SessionStore` | `sessionstore.h/.cpp` | JSONL-backed session persistence |
| `ContextBuilder` | `contextbuilder.h/.cpp` | Diagnostic / selection / file context assembly |
| `ProjectBrainService` | `projectbrainservice.h/.cpp` | Persistent workspace knowledge (rules, facts, sessions) |
| `BrainContextBuilder` | `braincontextbuilder.h/.cpp` | Assembles brain context for prompt injection |
| `MemorySuggestionEngine` | `memorysuggestionengine.h/.cpp` | Auto-extracts facts from agent turns |
| `SecureKeyStorage` | `securekeystorage.h/.cpp` | Platform-native credential storage (DPAPI/Keychain/libsecret) |
| `NetworkMonitor` | `networkmonitor.h` | Connectivity and rate-limit detection |

### Shared Tool Runtime (`src/agent/tools/`)

Tools registered centrally and available to all providers. Permission/approval
controlled by `ToolApprovalService`.

### Provider Plugins (`plugins/`)

| Plugin | Directory | Backend |
|--------|-----------|---------|
| Copilot | `plugins/copilot/` | GitHub Copilot Chat Completions + Responses API |
| Claude | `plugins/claude/` | Anthropic Messages API |
| Codex | `plugins/codex/` | OpenAI Chat Completions API |

### Plugin Extension Interfaces

| Interface | File | Purpose |
|-----------|------|---------|
| `IAgentSettingsPageProvider` | `iagentsettingspageprovider.h` | Plugin-contributed settings UI |
| `IChatSessionImporter` | `ichatsessionimporter.h` | Vendor transcript import |
| `IProviderAuthIntegration` | `iproviderauthintegration.h` | Unified auth lifecycle |

### UI Surfaces (`src/agent/chat/`)

| Widget | Purpose |
|--------|---------|
| `ChatPanelWidget` | Main chat composition root (transcript + input + toolbar) |
| `ChatTranscriptView` | Scrollable turn list |
| `ChatTurnWidget` | Single turn renderer (markdown, code, tools, thinking) |
| `ChatInputWidget` | Input area with mode/model bar, attachments |
| `InlineChatWidget` | Selection-scoped inline chat overlay |
| `QuickChatDialog` | Lightweight floating chat |

## Provider responsibilities

- Authentication (OAuth, API key, device flow)
- Model discovery and capability declaration
- Request/stream transport
- Response parsing (SSE, NDJSON, etc.)
- Optional: settings page, auth integration, session import hooks

## Core responsibilities

- Shared tool execution and presentation
- Session persistence and restore (JSONL)
- Request orchestration (multi-turn, tool loop)
- Context assembly (diagnostics, files, brain context)
- Approval policies (per-tool, auto/ask/deny)
- Transcript/event normalization
- Token usage tracking
- Async context gathering (workspace indexer, git status)
- Project Brain: persistent knowledge injection

## Important constraint

Copilot must be implemented as a plugin on top of the platform, not as the platform itself. The same runtime should serve Claude and Codex with minimal duplication.
