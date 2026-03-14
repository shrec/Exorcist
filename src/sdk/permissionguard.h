#pragma once

#include "sdk/ihostservices.h"
#include "sdk/pluginpermission.h"

#include <QList>
#include <QSet>
#include <QString>

// ── PermissionGuardedHostServices ────────────────────────────────────────────
//
// Per-plugin wrapper around IHostServices that checks declared permissions
// before delegating calls. Services that require no special permission
// (commands, editor, notifications, views) pass through directly.
// Services that access filesystem, git, terminal, or diagnostics
// check the plugin's granted permissions first.
//
// queryService() is filtered through an allowlist — core-internal services
// (mainwindow, secureKeyStorage, agentController, toolRegistry, etc.) are
// never exposed to plugins. Plugins may only register and query services
// that are NOT in the protected set.
//
// If a permission is denied, the service method returns a safe default
// (empty string, false, empty list, or no-op for void methods).

class PermissionGuardedHostServices : public IHostServices
{
public:
    PermissionGuardedHostServices(IHostServices *delegate,
                                  const QList<PluginPermission> &granted);

    // Always available — no permission required
    ICommandService *commands() override { return m_delegate->commands(); }
    IEditorService *editor() override { return m_delegate->editor(); }
    IViewService *views() override { return m_delegate->views(); }
    INotificationService *notifications() override { return m_delegate->notifications(); }

    // Requires WorkspaceRead or WorkspaceWrite
    IWorkspaceService *workspace() override;
    IGitService *git() override;
    ITerminalService *terminal() override;
    IDiagnosticsService *diagnostics() override;

    // Task service — available to all (task execution checks handled elsewhere)
    ITaskService *tasks() override { return m_delegate->tasks(); }

    // Service registry — guarded: plugins cannot access or shadow core services
    void registerService(const QString &name, QObject *svc) override;
    QObject *queryService(const QString &name) override;

private:
    static const QSet<QString> &protectedServiceNames();

    IHostServices *m_delegate;
    PluginPermissions m_permissions;
};
