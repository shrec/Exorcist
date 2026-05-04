#pragma once

#include <QString>
#include <QObject>
#include <QList>

#include "sdk/pluginpermission.h"

class IHostServices;

struct PluginInfo
{
    QString id;
    QString name;
    QString version;
    QString description;
    QString author;
    QString apiVersion;                          // SDK version (e.g. "1.0")
    QList<PluginPermission> requestedPermissions; // declared permissions
};

class IPlugin
{
public:
    virtual ~IPlugin() = default;
    virtual PluginInfo info() const = 0;

    /// SDK v1: Initialize with typed host services.
    /// Default implementation falls back to legacy initialize(QObject*).
    virtual bool initialize(IHostServices *host) { Q_UNUSED(host); return true; }

    /// Legacy: Initialize with raw service registry.
    /// Kept for backward compatibility. New plugins should override
    /// initialize(IHostServices*) instead.
    virtual void initialize(QObject *services) { Q_UNUSED(services); }

    /// Temporarily suspend this plugin (language no longer active).
    /// Plugins should release expensive resources but stay loadable.
    /// Default: no-op (backward compatible).
    virtual void suspend() {}

    /// Resume a previously suspended plugin (language became active again).
    /// Default: no-op (backward compatible).
    virtual void resume() {}

    /// Called when the user opens or switches the workspace folder.
    /// Plugins should update their working directory, reload config files,
    /// and refresh any workspace-dependent state.
    /// Default: no-op (backward compatible).
    virtual void onWorkspaceChanged(const QString &root) { Q_UNUSED(root); }

    /// Called when a workspace becomes active.  Default forwards to
    /// onWorkspaceChanged(root) so existing plugins keep working without
    /// changes.  New plugins should override this for the open path.
    virtual void onWorkspaceOpened(const QString &root) { onWorkspaceChanged(root); }

    /// Called when the workspace is closed (Close Folder / Close Solution).
    /// Plugins must dismantle workspace-dependent state here — stop child
    /// processes, clear caches, hide workspace-only UI, reset working
    /// directories.  See CLAUDE.md rule L2: workspace lifecycle is plugin-
    /// broadcast, never poked at by MainWindow.
    /// Default: forward to onWorkspaceChanged({}) so existing plugins that
    /// react to "empty root means closed" still work.
    virtual void onWorkspaceClosed() { onWorkspaceChanged(QString()); }

    virtual void shutdown() = 0;
};

#define EXORCIST_PLUGIN_IID "org.exorcist.IPlugin/1.0"
#define EXORCIST_SDK_VERSION_MAJOR 1
#define EXORCIST_SDK_VERSION_MINOR 0

Q_DECLARE_INTERFACE(IPlugin, EXORCIST_PLUGIN_IID)
