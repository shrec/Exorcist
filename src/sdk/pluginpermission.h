#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// ── Plugin Permission Model ──────────────────────────────────────────────────
//
// Every plugin declares what permissions it requires.
// The IDE prompts the user before granting access.
// Plugins cannot exceed their declared permissions.

enum class PluginPermission
{
    FilesystemRead,     // Read files in workspace
    FilesystemWrite,    // Create / modify / delete files
    TerminalExecute,    // Spawn processes or run shell commands
    GitRead,            // Read branch, status, blame, diff
    GitWrite,           // Stage, commit, push, checkout
    NetworkAccess,      // Outbound HTTP/WebSocket
    DiagnosticsRead,    // Read LSP diagnostics
    WorkspaceRead,      // Query open files, project structure
    WorkspaceWrite,     // Modify workspace settings
    AgentToolInvoke,    // Invoke agent tools
};

/// Resolved set of permissions for a loaded plugin.
struct PluginPermissions
{
    QList<PluginPermission> granted;

    bool has(PluginPermission p) const
    {
        return granted.contains(p);
    }
};
