# Project Audit (Working Document)

Date: 2026-03-13
Scope: overall technical assessment (architecture, quality, UX, testing,
performance, docs), not limited to security.

This document complements the security-focused audit in
`docs/security_audit_working.md`.

## Executive snapshot

- Strong modular architecture with clear layering and SDK boundaries.
  Evidence: `docs/architecture.md:1`, `docs/manifesto.md:1`
- Feature coverage is broad and consistent with the roadmap.
  Evidence: `docs/roadmap.md:1`
- Testing footprint is substantial (QtTest suites across major subsystems).
  Evidence: `tests/CMakeLists.txt`
- Some manifesto principles are only partially enforced in code (UI blocking,
  network trust, and long-running tool calls).
  Evidence: `docs/manifesto.md:1`, `src/git/gitservice.cpp:29`,
  `src/mcp/mcptooladapter.h:27`

## Strengths

1) Architecture clarity and separation of concerns.
   Evidence: `docs/architecture.md:1`
   Notes: Core interfaces, SDK boundary, plugin system, and bridge isolate
   concerns and enable portability.

2) Performance intent is explicit and measurable.
   Evidence: `docs/manifesto.md:1`, `src/startupprofiler.*`
   Notes: Targets and tooling exist, which helps keep regressions visible.

3) Comprehensive subsystem coverage in the roadmap.
   Evidence: `docs/roadmap.md:1`
   Notes: Major IDE pillars are planned and many are implemented.

4) Solid automated test footprint.
   Evidence: `tests/CMakeLists.txt`
   Notes: Tests exist for LSP, UI elements, git, search, etc.

## Gaps / Risks (non-security)

1) UI thread blocking in a few synchronous paths.
   Evidence: `src/git/gitservice.cpp:29`
   Impact: violates manifesto "no UI freeze" and may cause hangs on large repos.

2) Agent tool calls can hang without timeout or cancellation.
   Evidence: `src/mcp/mcptooladapter.h:27`
   Impact: degraded UX in agent mode; potentially unrecoverable waits.
   **Status: RESOLVED** — 60s QTimer timeout already implemented (lines 51-57).
   Tool returns error on timeout. No action required.

3) Agent UI event history is unbounded.
   Evidence: `src/agent/ui/agentuibus.cpp:20`
   Impact: memory growth during long sessions.
   **Status: RESOLVED** — per-mission cap of 500 events (kMaxEventsPerMission)
   plus mission count cap of 20 (kMaxMissions) with LRU eviction.

4) Dashboard tooling is write-only and lacks feedback/ack flow.
   Evidence: `src/agent/tools/dashboardtools.h:1`,
   `src/agent/ui/dashboardjsbridge.h:1`
   Impact: hard to reconcile UI state or detect dashboard render failures.

5) Manifesto "no network without explicit action" is not fully codified.
   Evidence: `docs/manifesto.md:1`, `src/plugin/pluginmarketplaceservice.cpp:89`
   Impact: policy drift between docs and implementation.

## Documentation alignment

- Core philosophy and layering are clear and useful.
  Evidence: `docs/manifesto.md:1`, `docs/architecture.md:1`
- Some docs are forward-looking without clear "done" markers.
  Evidence: `docs/roadmap.md:1`, `docs/vision.md:1`

## Recommended tasks (prioritized)

P0
- Enforce "no UI blocking" in sync code paths; convert remaining synchronous
  git operations to async or guarded timeouts with UX feedback.
  Evidence: `src/git/gitservice.cpp:29`
  **Status: DONE** — Added async variants for all remaining sync methods:
  showAtRevisionAsync, diffRevisionsAsync, changedFilesBetweenAsync,
  localBranchesAsync, logAsync. Implemented runGitAsync for stage/unstage/commit.
  All async methods use QtConcurrent::run + QMetaObject::invokeMethod pattern.
  Sync const methods remain for backward compat but callers should migrate.
- Add timeout + cancel path for MCP tool calls, with UI feedback on timeout.
  Evidence: `src/mcp/mcptooladapter.h:27`
  **Status: DONE** — already has 60s QTimer timeout.

P1
- Add bounded retention for AgentUIBus history (cap or ring buffer).
  Evidence: `src/agent/ui/agentuibus.cpp:20`
  **Status: DONE** — kMaxEventsPerMission=500 plus kMaxMissions=20 eviction.
- Introduce dashboard render acknowledgements or error reporting from JS bridge
  to keep UI state consistent.
  Evidence: `src/agent/ui/dashboardjsbridge.h:1`
- Add a "network activity requires user action" policy check and surface it
  in the UI (aligns with manifesto).
  Evidence: `docs/manifesto.md:1`, `src/plugin/pluginmarketplaceservice.cpp:89`

P2
- Improve doc hygiene: mark completed roadmap items explicitly and tag
  "planned vs implemented" in docs/vision.md.
  Evidence: `docs/roadmap.md:1`, `docs/vision.md:1`
- Expand tests for agent UI events and dashboard render flows.
  Evidence: `src/agent/tools/dashboardtools.h:1`

## Deep dives

### Performance hotspots (profiling + async correctness)

Findings
- UI-thread blocking via QEventLoop in auth/confirmation flows.
  Evidence: `src/agent/authmanager.cpp:69`, `src/agent/chat/chatpanelwidget.cpp:759`
  Impact: potential UI stalls and re-entrancy risk during network or user waits.
- Tool execution uses synchronous QProcess waits in multiple tools.
  Evidence: `src/agent/tools/devtools.h:134`, `src/agent/tools/systemtools.h:88`
  Impact: slow tools can block their execution thread; combined with UI callbacks,
  risks perceived hangs in agent flows.
- Startup profiling exists, but no continuous runtime profiling for long tasks.
  Evidence: `src/startupprofiler.cpp:1`
  Impact: performance regressions after startup are harder to detect.

Tasks
- Convert QEventLoop UI waits to async flows with timeout + UI feedback.
  Evidence: `src/agent/chat/chatpanelwidget.cpp:759`
- Add cancellation + timeout to blocking tool operations and surface
  "in-progress" state in UI.
  Evidence: `src/agent/tools/devtools.h:134`
- Add lightweight runtime task timing (beyond startup) for long-running ops.
  Evidence: `src/startupprofiler.cpp:1`

### Plugin lifecycle / SDK stability & versioning

Findings
- PluginInfo.apiVersion exists but is not validated during load/activation.
  Evidence: `src/plugininterface.h:11`, `src/pluginmanager.cpp:85`
  Impact: silent incompatibility between SDK version and plugin behavior.
  **Status: DONE** — activatePlugin() now checks apiVersion major version
  against EXORCIST_SDK_VERSION_MAJOR. Incompatible plugins are skipped
  with clear error message.
- Two initialization paths exist; legacy QObject* path bypasses permission guard.
  Evidence: `src/plugininterface.h:28`, `src/pluginmanager.cpp:206`
  Impact: inconsistent permission enforcement and SDK surface drift.
- Manifest version fields exist but are not used for compatibility policy.
  Evidence: `src/plugin/pluginmanifest.h:193`
  Impact: no clear upgrade/downgrade gating in the plugin manager.

Tasks
- Enforce PluginInfo.apiVersion compatibility on load with clear error reporting.
  Evidence: `src/plugininterface.h:11`, `src/pluginmanager.cpp:85`
  **Status: DONE** — Major version check added in activatePlugin().
- Deprecate or gate legacy initialize(QObject*) path; route through
  PermissionGuardedHostServices or block by default.
  Evidence: `src/plugininterface.h:28`, `src/pluginmanager.cpp:206`
- Add manifest version policy (min host version, breaking changes) and
  ensure PluginManager checks it on activation.
  Evidence: `src/plugin/pluginmanifest.h:193`

### UI/UX consistency & dashboard/chat integration

Findings
- Dashboard fallback cannot update steps correctly because StepAdded stores
  label only, while StepUpdated searches by stepId.
  Evidence: `src/agent/ui/agentdashboardpanel.cpp:127`
  Impact: inconsistent UI state in non-Ultralight fallback mode.
- Dashboard events are sent without readiness/ack; JS bridge has no queue.
  Evidence: `src/agent/ui/agentdashboardpanel.cpp:33`,
  `src/agent/ui/dashboardjsbridge.cpp:33`
  Impact: early events can be dropped or lost during view initialization.
  **Status: RESOLVED** — Ultralight path already queues scripts via
  evaluateScript(). Fallback path now has null guard for m_stepsList/m_logsList.
- Dashboard mission lifecycle is independent of chat session lifecycle;
  no shared ID or persistence across session restore.
  Evidence: `src/agent/ui/agentuibus.cpp:20`,
  `src/agent/tools/dashboardtools.h:24`
  Impact: dashboard state can drift from chat transcript and recovered sessions.
- Fallback dashboard uses hardcoded dark palette and does not reuse chat theme
  tokens, leading to UX inconsistency.
  Evidence: `src/agent/ui/agentdashboardpanel.cpp:83`

Tasks
- Fix fallback step updates: store stepId in list items or map by ID.
  Evidence: `src/agent/ui/agentdashboardpanel.cpp:127`
  **Status: RESOLVED** — Already stores stepId in Qt::UserRole and label
  in Qt::UserRole+1. StepUpdated searches by stepId correctly.
- Add JS bridge readiness gating or event queue on startup.
  Evidence: `src/agent/ui/agentdashboardpanel.cpp:33`,
  `src/agent/ui/dashboardjsbridge.cpp:33`
  **Status: DONE** — Ultralight evaluateScript() already queues.
  Fallback path now has null guard.
- Tie dashboard mission lifecycle to chat session (shared missionId,
  persistence on restore).
  Evidence: `src/agent/tools/dashboardtools.h:24`
- Align fallback dashboard theming with chat theme tokens.
  Evidence: `src/agent/ui/agentdashboardpanel.cpp:83`

## Open questions

- Is there a central UX surface for background tasks (to satisfy Manifesto #2)?
- Should agent dashboard tools be allowed in non-agent mode?
- What is the desired cap for AgentUIBus history retention?
