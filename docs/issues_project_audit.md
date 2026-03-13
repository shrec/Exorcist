# Audit Issues (Local Backlog)

Date: 2026-03-13
Scope: consolidated issues from security and project audits.

Format:
- ID: short stable id
- Priority: P0/P1/P2
- Area: Security / Performance / Plugin / UI-UX / Docs
- Summary: one line
- Evidence: file:line
- Acceptance: measurable outcome

## Security

- ID: SEC-001
  Priority: P0
  Area: Security
  Summary: Add workspace trust gate before reading `.mcp.json` and starting MCP servers.
  Evidence: `src/mcp/mcpservermanager.cpp:7`
  Acceptance: Untrusted workspace cannot start MCP servers without explicit user approval.

- ID: SEC-002
  Priority: P0
  Area: Security
  Summary: Validate MCP command against allowlist + per-server confirmation.
  Evidence: `src/mcp/mcpservermanager.cpp:39`
  Acceptance: Unknown commands require confirmation; allowlisted servers auto-run.

- ID: SEC-003
  Priority: P0
  Area: Security
  Summary: Add integrity verification for marketplace downloads (hash/signature).
  Evidence: `src/plugin/pluginmarketplaceservice.cpp:129`
  Acceptance: Install fails with clear error when hash/signature mismatch.

- ID: SEC-004
  Priority: P1
  Area: Security
  Summary: Block zip-slip and path traversal during plugin extraction.
  Evidence: `src/plugin/pluginmarketplaceservice.cpp:243`
  Acceptance: Extraction cannot write outside `m_pluginsDir` (test cases included).

- ID: SEC-005
  Priority: P1
  Area: Security
  Summary: Harden plugin uninstall path handling.
  Evidence: `src/plugin/pluginmarketplaceservice.cpp:199`
  Acceptance: Uninstall only removes plugin directory under `m_pluginsDir`.

- ID: SEC-006
  Priority: P1
  Area: Security
  Summary: Add safe SSH path/command escaping and host key confirmation flow.
  Evidence: `plugins/remote/sshsession.cpp:237`
  Acceptance: No injection on special characters; host key prompts are explicit.

- ID: SEC-007
  Priority: P2
  Area: Security
  Summary: Secure SSH askpass temp helper and cleanup.
  Evidence: `plugins/remote/sshsession.cpp:158`
  Acceptance: Unique temp helper per session, deleted on exit.

## Performance / Async Correctness

- ID: PERF-001
  Priority: P0
  Area: Performance
  Summary: Remove UI-thread QEventLoop waits in auth/confirmation flows.
  Evidence: `src/agent/authmanager.cpp:69`, `src/agent/chat/chatpanelwidget.cpp:759`
  Acceptance: No UI-thread blocking during auth or tool confirmation.

- ID: PERF-002
  Priority: P0
  Area: Performance
  Summary: Add timeout + cancellation to MCP tool calls.
  Evidence: `src/mcp/mcptooladapter.h:27`
  Acceptance: Tool call times out with user-visible error and recoverable state.

- ID: PERF-003
  Priority: P1
  Area: Performance
  Summary: Add runtime task timing beyond startup.
  Evidence: `src/startupprofiler.cpp:1`
  Acceptance: Long tasks log duration with context and thresholds.

## Plugin Lifecycle / SDK Stability

- ID: PLUG-001
  Priority: P0
  Area: Plugin
  Summary: Enforce PluginInfo.apiVersion compatibility on load.
  Evidence: `src/plugininterface.h:11`, `src/pluginmanager.cpp:85`
  Acceptance: Incompatible plugins are blocked with clear error.

- ID: PLUG-002
  Priority: P1
  Area: Plugin
  Summary: Deprecate or gate legacy initialize(QObject*) path.
  Evidence: `src/plugininterface.h:28`, `src/pluginmanager.cpp:206`
  Acceptance: Legacy path disabled by default or routed via permission guard.

- ID: PLUG-003
  Priority: P1
  Area: Plugin
  Summary: Add manifest version policy (min host version / breaking change).
  Evidence: `src/plugin/pluginmanifest.h:193`
  Acceptance: Plugin activation respects host compatibility rules.

## UI/UX Consistency

- ID: UX-001
  Priority: P1
  Area: UI-UX
  Summary: Fix fallback dashboard step updates to use stepId mapping.
  Evidence: `src/agent/ui/agentdashboardpanel.cpp:127`
  Acceptance: Step updates reflect correct item in QWidget fallback.

- ID: UX-002
  Priority: P1
  Area: UI-UX
  Summary: Add JS bridge readiness gating or event queue for dashboard.
  Evidence: `src/agent/ui/agentdashboardpanel.cpp:33`
  Acceptance: Dashboard does not drop early events; events replay on ready.

- ID: UX-003
  Priority: P1
  Area: UI-UX
  Summary: Tie dashboard mission lifecycle to chat session.
  Evidence: `src/agent/tools/dashboardtools.h:24`
  Acceptance: Dashboard state restores with chat session and shared missionId.

- ID: UX-004
  Priority: P2
  Area: UI-UX
  Summary: Align fallback dashboard theme with chat theme tokens.
  Evidence: `src/agent/ui/agentdashboardpanel.cpp:83`
  Acceptance: Fallback dashboard uses shared theme tokens or ThemeManager.

## Docs / Process

- ID: DOC-001
  Priority: P2
  Area: Docs
  Summary: Mark roadmap items as planned vs implemented with dates.
  Evidence: `docs/roadmap.md:1`, `docs/vision.md:1`
  Acceptance: Roadmap entries include status and last-updated.
