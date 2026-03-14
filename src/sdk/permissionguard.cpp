#include "sdk/permissionguard.h"

#include <QSet>

// ── Protected service names ──────────────────────────────────────────────────
//
// Core-internal services that plugins must NEVER access via queryService().
// These are implementation details of the IDE — direct access would bypass
// the permission model entirely (e.g., getting MainWindow gives full UI
// control, getting agentController gives tool execution, etc.).
//
// Plugins should use the typed SDK interfaces (IEditorService, IGitService,
// etc.) which are already permission-guarded above.

const QSet<QString> &PermissionGuardedHostServices::protectedServiceNames()
{
    static const QSet<QString> names = {
        // Core infrastructure — full IDE control
        QStringLiteral("mainwindow"),

        // Security-sensitive
        QStringLiteral("secureKeyStorage"),

        // Agent internals — tool execution, prompt injection surface
        QStringLiteral("toolRegistry"),
        QStringLiteral("agentController"),
        QStringLiteral("contextBuilder"),
        QStringLiteral("sessionStore"),
        QStringLiteral("agentProviderRegistry"),
        QStringLiteral("agentRequestRouter"),
        QStringLiteral("chatSessionService"),
        QStringLiteral("toolApprovalService"),
        QStringLiteral("projectBrainService"),

        // Plugin system internals
        QStringLiteral("pluginMarketplace"),
    };
    return names;
}

PermissionGuardedHostServices::PermissionGuardedHostServices(
    IHostServices *delegate, const QList<PluginPermission> &granted)
    : m_delegate(delegate)
    , m_permissions{granted}
{
}

IWorkspaceService *PermissionGuardedHostServices::workspace()
{
    if (m_permissions.has(PluginPermission::WorkspaceRead)
        || m_permissions.has(PluginPermission::WorkspaceWrite))
        return m_delegate->workspace();
    return nullptr;
}

IGitService *PermissionGuardedHostServices::git()
{
    if (m_permissions.has(PluginPermission::GitRead)
        || m_permissions.has(PluginPermission::GitWrite))
        return m_delegate->git();
    return nullptr;
}

ITerminalService *PermissionGuardedHostServices::terminal()
{
    if (m_permissions.has(PluginPermission::TerminalExecute))
        return m_delegate->terminal();
    return nullptr;
}

IDiagnosticsService *PermissionGuardedHostServices::diagnostics()
{
    if (m_permissions.has(PluginPermission::DiagnosticsRead))
        return m_delegate->diagnostics();
    return nullptr;
}

void PermissionGuardedHostServices::registerService(const QString &name,
                                                     QObject *svc)
{
    if (protectedServiceNames().contains(name))
        return; // silently block — plugin cannot shadow core services

    m_delegate->registerService(name, svc);
}

QObject *PermissionGuardedHostServices::queryService(const QString &name)
{
    if (protectedServiceNames().contains(name))
        return nullptr; // core-internal service — not visible to plugins

    return m_delegate->queryService(name);
}
