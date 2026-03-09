#pragma once

#include "sdk/ihostservices.h"
#include "sdk/pluginpermission.h"

#include <QList>

// ── PermissionGuardedHostServices ────────────────────────────────────────────
//
// Per-plugin wrapper around IHostServices that checks declared permissions
// before delegating calls. Services that require no special permission
// (commands, editor, notifications, views) pass through directly.
// Services that access filesystem, git, terminal, or diagnostics
// check the plugin's granted permissions first.
//
// If a permission is denied, the service method returns a safe default
// (empty string, false, empty list, or no-op for void methods).

class PermissionGuardedHostServices : public IHostServices
{
public:
    PermissionGuardedHostServices(IHostServices *delegate,
                                  const QList<PluginPermission> &granted)
        : m_delegate(delegate)
        , m_permissions{granted}
    {
    }

    // Always available — no permission required
    ICommandService *commands() override { return m_delegate->commands(); }
    IEditorService *editor() override { return m_delegate->editor(); }
    IViewService *views() override { return m_delegate->views(); }
    INotificationService *notifications() override { return m_delegate->notifications(); }

    // Requires WorkspaceRead or WorkspaceWrite
    IWorkspaceService *workspace() override
    {
        if (m_permissions.has(PluginPermission::WorkspaceRead)
            || m_permissions.has(PluginPermission::WorkspaceWrite))
            return m_delegate->workspace();
        return nullptr;
    }

    // Requires GitRead or GitWrite
    IGitService *git() override
    {
        if (m_permissions.has(PluginPermission::GitRead)
            || m_permissions.has(PluginPermission::GitWrite))
            return m_delegate->git();
        return nullptr;
    }

    // Requires TerminalExecute
    ITerminalService *terminal() override
    {
        if (m_permissions.has(PluginPermission::TerminalExecute))
            return m_delegate->terminal();
        return nullptr;
    }

    // Requires DiagnosticsRead
    IDiagnosticsService *diagnostics() override
    {
        if (m_permissions.has(PluginPermission::DiagnosticsRead))
            return m_delegate->diagnostics();
        return nullptr;
    }

    // Task service — available to all (task execution checks handled elsewhere)
    ITaskService *tasks() override { return m_delegate->tasks(); }

private:
    IHostServices *m_delegate;
    PluginPermissions m_permissions;
};
