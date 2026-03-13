# Security/Quality Audit (Working Document)

Date: 2026-03-13
Scope: static review of agent UI bus, MCP startup/tooling, plugin marketplace,
SSH remote tooling, and agent context injection.

This is a living document. It tracks findings, impact, and prioritized tasks.

## Findings (prioritized)

1) High - Untrusted workspace MCP config can auto-run local commands.
   Evidence: `src/mcp/mcpservermanager.cpp:7`, `src/mcp/mcpservermanager.cpp:39`
   Impact: opening a malicious repo can execute arbitrary binaries via `.mcp.json`.
   **Status: DONE** — Workspace trust gate implemented. MCP servers from
   workspace .mcp.json require explicit user trust (persisted via QSettings
   hash). `trustRequired` signal emitted for UI prompt. Tests in test_mcptrust.

2) High - Marketplace downloads/extracts plugins without integrity or path checks.
   Evidence: `src/plugin/pluginmarketplaceservice.cpp:129`,
   `src/plugin/pluginmarketplaceservice.cpp:183`,
   `src/plugin/pluginmarketplaceservice.cpp:199`,
   `src/plugin/pluginmarketplaceservice.cpp:243`
   Impact: supply-chain risk (tampered archives), path traversal via `entry.id`,
   zip-slip during extraction, and unsafe directory deletion on uninstall.
   **Status: DONE** — isValidPluginId() blocks `..`, `/`, `\` in IDs.
   isPathInside() validates all paths stay in plugins dir. Post-extraction
   path check added. Tar uses --no-absolute-filenames. PowerShell uses
   -LiteralPath. Tests in test_marketplacesafety.

3) Medium - MCP tool adapter blocks synchronously without a timeout.
   Evidence: `src/mcp/mcptooladapter.h:27`
   Impact: agent tool call can hang indefinitely if MCP server stalls.
   **Status: RESOLVED** — 60s QTimer timeout already present. Confirmed
   lines 27 (spec().timeoutMs=60000) and 51-57 (QTimer + timedOut flag).

4) Medium - SSH command and path strings are not escaped/quoted safely.
   Evidence: `plugins/remote/sshsession.cpp:237`,
   `plugins/remote/sshsession.cpp:271`,
   `plugins/remote/sshsession.cpp:334`,
   `plugins/remote/sshsession.cpp:454`
   Impact: remote command injection or failure on paths with spaces/metacharacters.
   **Status: DONE** — shellQuote() helper added. All 5 remote path interpolation
   sites now use POSIX single-quote escaping (cd, ls, cat, cat >, test -e).

5) Medium - SSH host key policy auto-accepts new keys.
   Evidence: `plugins/remote/sshsession.cpp:37`, `plugins/remote/sshsession.cpp:145`
   Impact: weaker MITM protection; no explicit user confirmation flow.
   **Status: DONE** — StrictHostKeyChecking changed from accept-new to yes.
   Managed known_hosts file at <appDir>/.exorcist/known_hosts. Signals
   hostKeyVerificationRequired/hostKeyAccepted for UI prompt. acceptHostKey()
   does one-time accept-new to persist key; rejectHostKey() aborts connection.

6) Medium - SSH password helper uses predictable temp file and env var.
   Evidence: `plugins/remote/sshsession.cpp:158`
   Impact: local leakage risk (temp file reuse, environment exposure).
   **Status: DONE** — Replaced predictable /tmp/exo_askpass with unique
   QTemporaryFile (exo_askpass_XXXXXX.cmd/.sh), auto-removed on session
   destruction. Owner-only permissions on Unix.

7) Low - Agent UI event history grows unbounded per mission.
   Evidence: `src/agent/ui/agentuibus.cpp:20`
   Impact: memory growth in long-running or chatty sessions.

8) Low - Agent auto-injects OS + workspace tree into model prompts.
   Evidence: `src/agent/agentcontroller.cpp:385`
   Impact: privacy/PII leakage if remote providers are used; no explicit consent.

## Tasks (prioritized backlog)

P0
- Add workspace trust gate before reading `.mcp.json` and starting MCP servers.
  **Status: DONE** — trustRequired signal + setWorkspaceRoot/trustWorkspace API.
- Validate MCP config: command allowlist + explicit user confirmation per server.
  **Status: PARTIALLY DONE** — Trust gate blocks untrusted workspaces. Command
  allowlist is a future refinement.
- Add integrity verification for marketplace downloads (hash/signature).
  **Status: DONE** — SHA-256 field added to MarketplaceEntry. Download
  archive hash verified before extraction (QCryptographicHash::Sha256).
  Registry provides expected sha256; mismatch blocks install.

P1
- Enforce safe extraction: prevent zip-slip, block path traversal on `entry.id`,
  and validate final install path is inside `m_pluginsDir`.
  **Status: DONE** — isValidPluginId + isPathInside + --no-absolute-filenames
  + -LiteralPath. Tests in test_marketplacesafety.
- Add MCP tool call timeout + cancellation path in `McpToolAdapter`.
  **Status: DONE** — Already had 60s QTimer.
- Implement SSH command/path escaping (and unit tests for edge cases).
  **Status: DONE** — shellQuote() applied to all 5 sites.
- Replace `StrictHostKeyChecking=accept-new` with explicit trust flow
  (prompt + known_hosts management).
  **Status: DONE** — StrictHostKeyChecking=yes + managed known_hosts +
  hostKeyVerificationRequired signal + acceptHostKey/rejectHostKey slots.

P2
- Secure SSH askpass: unique temp helper, remove after use, minimize env exposure.
  **Status: DONE** — QTemporaryFile with auto-remove, unique per invocation.
- Add bounded retention for `AgentUIBus` history (cap or ring buffer).
  **Status: DONE** — kMaxEventsPerMission=500 per mission + kMaxMissions=20
  with LRU eviction of oldest missions.
- Add opt-in/consent or redaction for environment_info block.
- Add security tests for marketplace extraction, MCP trust, and SSH quoting.

## Open questions

- Is there an existing workspace trust UI that should gate MCP server startup?
- Should marketplace accept only first-party registries, or third-party too?
- Do we want a global privacy mode that disables environment_info injection?
