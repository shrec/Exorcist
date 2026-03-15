# Copilot Gap Analysis — 2026-03-14

Reference baselines refreshed in `ReserchRepos/`:

- `ReserchRepos/vscode` -> `9192689d931f` (`2026-03-14`, `Misc debug panel UX fixes (#301628)`)
- `ReserchRepos/vscode-copilot-chat` -> `b7ed22e7` (`2026-03-13`, `Handle initialSessionOptions (#4407)`)

## What is already true in Exorcist

Exorcist already has a substantial generic agent/chat platform in `src/agent/`:

- multi-turn chat UI and session history
- tool loop with approvals
- workspace edit cards
- prompt files
- followups
- subagent tool
- many local IDE/workspace tools

So the main gap is not "build chat from zero". The gap is **GitHub Copilot parity**: our `plugins/copilot/` layer is still a thin provider wrapper on top of Exorcist's generic chat stack, while latest VS Code + Copilot Chat expose a much richer Copilot-specific surface.

## Highest-priority gaps

### 1. Copilot request/env parity is incomplete

Current Exorcist state:

- `plugins/copilot/copilotprovider.cpp` hardcodes `Editor-Version` to `vscode/1.100.0`
- same file hardcodes `Editor-Plugin-Version` to `copilot-chat/0.40.0`
- request shaping is based on our own `AgentRequest`, not the richer Copilot/VS Code request environment

Why this matters:

- latest `vscode-copilot-chat` builds headers dynamically from environment metadata
- Copilot behavior increasingly depends on richer editor/session metadata, not just bearer token + prompt text

Needed to finish:

- generate headers/version info dynamically
- align request metadata with actual selected mode, permissions, session target, and tool context
- audit `/responses` payload parity against current Copilot Chat extension behavior

### 2. Attachment model is much narrower than VS Code Copilot Chat

Current Exorcist state:

- `src/aiinterface.h` only models `Attachment::File` and `Attachment::Image`
- `plugins/copilot/copilotprovider.cpp` only serializes text + inline image data URLs

Reference state:

- latest `vscode` chat input supports file, image, prompt, MCP, tool, terminal, source control, problem, symbol, notebook/paste and handoff-related attachments

Needed to finish:

- extend the attachment domain model beyond file/image
- carry typed attachments end-to-end through `ChatPanelWidget` -> `AgentRequest` -> `CopilotProvider`
- expose Copilot-specific attachment affordances in the input UI

### 3. Missing Copilot tool-surface parity

Reference state from `vscode-copilot-chat`:

- includes dedicated tools such as `view_image`, `search_subagent`, richer VS Code interaction tools, notebook tools, prompt/setup helpers, and Copilot-specific command routing

Current Exorcist state:

- we have many generic tools in `src/agent/agentplatformbootstrap.cpp`
- but there is no `view_image` tool registration
- there is no Copilot-style typed tool catalog/grouping equivalent to `toolNames.ts`
- Copilot plugin itself does not contribute Copilot-specific tools; it only consumes whatever generic tools Exorcist exposes

Needed to finish:

- close the concrete tool gaps first: `view_image`, Copilot-compatible tool aliases/names, Copilot-specific tool grouping/presentation
- decide which VS Code tools should be true IDE tools vs compatibility aliases over existing tools

### 4. Session/import/debug ecosystem is still far behind Copilot Chat

Reference state:

- `vscode-copilot-chat` contains chat session providers/importers for cloud/CLI/Claude-style sessions
- latest update added more session-flow work (`initialSessionOptions`) and terminal/session integration
- VS Code core chat service tracks richer session lifecycle, queues, cancellation, question carousels, handoffs, persisted input state, and editing-session coordination

Current Exorcist state:

- local session history exists in `src/agent/chatsessionservice.*` and `src/agent/chat/*`
- no equivalent Copilot cloud session import/provider pipeline
- no Copilot CLI session-state integration
- no question carousel / handoff workflow parity
- no dedicated Copilot debug/session log viewer parity

Needed to finish:

- implement chat session importer/provider interfaces for Copilot-originated sessions
- add handoff/question-carousel/session-target semantics where relevant
- add Copilot/agent debug artifacts closer to VS Code's session/debug model

### 5. Terminal integration parity is missing

Reference state:

- latest `vscode-copilot-chat` added `copilotCLITerminalLinkProvider.ts` to resolve Copilot CLI relative paths from session-state directories

Current Exorcist state:

- terminal tools exist
- no Copilot CLI terminal link resolver or session-state aware path opening flow

Needed to finish:

- detect Copilot-driven terminal sessions
- resolve relative paths against Copilot session-state directories
- make terminal output clickable with line/column navigation

### 6. Browser/image rich workflows are missing

Reference state from latest `vscode` pull:

- new browser APIs in workbench/ext host
- chat image extraction service
- chat image carousel service and richer image handling

Current Exorcist state:

- chat accepts pasted/dropped images
- no browser automation/editor surface comparable to new VS Code browser features
- no chat image extraction/carousel workflow

Needed to finish:

- add `view_image` + richer image inspection flow first
- then decide whether browser automation is in-scope for Copilot parity or a separate plugin

## Practical definition of "finished"

If the target is "usable Copilot provider", Exorcist is already close enough.

If the target is "GitHub Copilot Chat parity with updated VS Code references", the remaining work is still substantial. The minimum honest finish bar is:

1. request/header parity
2. typed attachment parity beyond file/image
3. Copilot-compatible tool catalog and missing tools
4. session/import/debug parity for Copilot workflows
5. terminal/path integration parity

## Bottom line

What Exorcist lacks is not basic chat. It lacks the **Copilot-specific compatibility layer** that sits between a generic agent UI and the current VS Code + Copilot Chat ecosystem.

The fastest path is to treat `plugins/copilot/` as a real compatibility adapter, not just a provider:

- upgrade request/env shaping
- add typed attachment support
- add Copilot-missing tools and aliases
- port session/terminal/debug workflows selectively
