#pragma once

#include <QObject>
#include <QStringList>
#include <QVector>

#include <memory>
#include <vector>

#include "plugininterface.h"
#include "plugin/pluginmanifest.h"

class QPluginLoader;
class IHostServices;
class PermissionGuardedHostServices;

class PluginManager : public QObject
{
public:
    explicit PluginManager(QObject *parent = nullptr);
    ~PluginManager() override;

    struct LoadedPlugin {
        IPlugin *instance;
        QPluginLoader *loader;
        PluginManifest manifest;  // from plugin.json (may be empty)
    };

    int loadPluginsFrom(const QString &path);

    /// SDK v1: Initialize plugins with typed host services.
    void initializeAll(IHostServices *host);

    /// Legacy: Initialize plugins with raw service registry.
    void initializeAll(QObject *services);

    void shutdownAll();

    const QStringList &errors() const;
    QVector<IPlugin *> plugins() const;
    QVector<QObject *> pluginObjects() const;

    /// Get loaded plugins with their manifests (for ContributionRegistry).
    const QVector<LoadedPlugin> &loadedPlugins() const { return m_loaded; }

private:
    QVector<LoadedPlugin> m_loaded;
    QStringList m_errors;
    std::vector<std::unique_ptr<PermissionGuardedHostServices>> m_permGuards;
};
