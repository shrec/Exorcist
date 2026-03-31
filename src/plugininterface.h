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

    virtual void shutdown() = 0;
};

#define EXORCIST_PLUGIN_IID "org.exorcist.IPlugin/1.0"
#define EXORCIST_SDK_VERSION_MAJOR 1
#define EXORCIST_SDK_VERSION_MINOR 0

Q_DECLARE_INTERFACE(IPlugin, EXORCIST_PLUGIN_IID)
