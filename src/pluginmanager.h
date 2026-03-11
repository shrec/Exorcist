#pragma once

#include <QObject>
#include <QStringList>
#include <QVector>

#include <memory>
#include <vector>

#include "plugininterface.h"
#include "plugin/pluginmanifest.h"

class QPluginLoader;
class QLibrary;
class IHostServices;
class IAgentProvider;
class PermissionGuardedHostServices;
namespace cabi { class CAbiPluginBridge; }

#include "sdk/luajit/luascriptengine.h"

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

    struct LoadedCAbiPlugin {
        std::unique_ptr<QLibrary> library;
        std::unique_ptr<cabi::CAbiPluginBridge> bridge;
        QString id;
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

    /// Get AI providers from C ABI plugins.
    QList<IAgentProvider *> cabiProviders() const;

    /// Load and initialize Lua script plugins from a directory.
    int loadLuaPluginsFrom(const QString &path, IHostServices *host);

    /// Errors from Lua script engine.
    QStringList luaErrors() const;

    /// Fire an event to all Lua plugins.
    void fireLuaEvent(const QString &eventName, const QStringList &args = {});

    /// Get loaded Lua plugin info.
    QVector<luabridge::LuaPluginInfo> loadedLuaScripts() const;

    /// Access the LuaJIT engine (for agent run_lua tool).
    luabridge::LuaScriptEngine *luaEngine() const { return m_luaEngine.get(); }

private:
    bool tryLoadCAbi(const QString &filePath);

    QVector<LoadedPlugin> m_loaded;
    std::vector<LoadedCAbiPlugin> m_cabiLoaded;
    QStringList m_errors;
    std::vector<std::unique_ptr<PermissionGuardedHostServices>> m_permGuards;
    std::unique_ptr<luabridge::LuaScriptEngine> m_luaEngine;
};
