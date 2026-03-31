#pragma once

#include <QObject>
#include <QSet>
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
class ContributionRegistry;
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
        bool suspended = false;   // true when language profile deactivated
    };

    struct LoadedCAbiPlugin {
        std::unique_ptr<QLibrary> library;
        std::unique_ptr<cabi::CAbiPluginBridge> bridge;
        QString id;
    };

    int loadPluginsFrom(const QString &path);

    /// SDK v1: Initialize plugins with typed host services.
    /// Plugins with activation events other than "*" or "onStartupFinished"
    /// are deferred and can be activated later via activateByEvent().
    void initializeAll(IHostServices *host);

    /// Legacy: Initialize plugins with raw service registry.
    void initializeAll(QObject *services);

    /// Activate deferred plugins that match the given event.
    /// Supported events: "onCommand:<id>", "onView:<id>",
    ///   "onLanguage:<langId>", "workspaceContains:<pattern>".
    /// Returns the number of newly activated plugins.
    int activateByEvent(const QString &event);

    /// Activate all deferred plugins whose workspaceContains pattern
    /// matches files in the given workspace root directory.
    int activateByWorkspace(const QString &workspaceRoot);

    /// Activate a specific deferred plugin by manifest ID.
    /// Returns the number of newly activated plugins.
    int activateByPluginId(const QString &pluginId);

    void shutdownAll();

    /// Notify all active plugins that the workspace folder has changed.
    /// Each plugin's onWorkspaceChanged() is called; plugins own their response.
    void notifyWorkspaceChanged(const QString &root);

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

    /// Disable/enable a plugin by ID. Persisted in QSettings.
    /// Takes effect on next startup (plugins already loaded stay loaded).
    void setPluginDisabled(const QString &pluginId, bool disabled);
    bool isPluginDisabled(const QString &pluginId) const;
    QSet<QString> disabledPluginIds() const { return m_disabledIds; }

    /// Set which language profiles are currently active.
    /// Language-specific plugins will only activate if their languageIds
    /// overlap with the active set.
    void setActiveLanguageProfiles(const QSet<QString> &profileIds);
    QSet<QString> activeLanguageProfiles() const { return m_activeProfiles; }

    /// Mark a language as workspace-level (won't be suspended on tab switch).
    void addWorkspaceLanguage(const QString &languageId);

    /// Activate deferred language plugins whose languageIds match
    /// the given profile. Returns the number of newly activated plugins.
    int activateByLanguageProfile(const QString &languageId);

    /// Suspend all loaded language plugins whose languageIds contain
    /// the given ID. Calls IPlugin::suspend() and removes contributions.
    /// Does not suspend if the plugin also serves a workspace-level profile.
    /// Returns the number of suspended plugins.
    int suspendByLanguageProfile(const QString &languageId);

    /// Resume previously suspended language plugins matching the language.
    /// Calls IPlugin::resume() and re-registers contributions.
    /// Returns the number of resumed plugins.
    int resumeByLanguageProfile(const QString &languageId);

    /// Switch the active language for the editor context.
    /// Suspends plugins for the previous language and activates/resumes
    /// plugins for the new language. General plugins are never affected.
    void switchActiveLanguage(const QString &newLanguageId);

    /// Set the ContributionRegistry for suspend/resume contribution management.
    void setContributionRegistry(ContributionRegistry *registry);

    /// Check if a plugin is allowed to run given current profile state.
    /// General plugins are always allowed. Language plugins need an
    /// active profile that matches their languageIds.
    bool isPluginAllowedByProfile(const PluginManifest &manifest) const;

    /// Returns true if the plugin with this instance pointer is currently
    /// deferred (not yet initialized). Used to skip manifest registration
    /// for un-initialized deferred plugins at startup.
    bool isPluginDeferred(const IPlugin *instance) const;

    /// Get the currently active editor language.
    QString activeEditorLanguage() const { return m_activeEditorLanguage; }

private:
    bool tryLoadCAbi(const QString &filePath);

    QVector<LoadedPlugin> m_loaded;
    std::vector<LoadedCAbiPlugin> m_cabiLoaded;
    QStringList m_errors;
    std::vector<std::unique_ptr<PermissionGuardedHostServices>> m_permGuards;
    std::unique_ptr<luabridge::LuaScriptEngine> m_luaEngine;

    // Deferred plugins waiting for activation events
    struct DeferredPlugin {
        LoadedPlugin loaded;
        QStringList activationEvents;
    };
    QVector<DeferredPlugin> m_deferred;
    IHostServices *m_host = nullptr;

    bool shouldDeferPlugin(const PluginManifest &manifest) const;
    void activatePlugin(const LoadedPlugin &lp);
    void loadDisabledSet();

    /// Topological sort by plugin dependencies. Plugins whose dependencies
    /// are not yet activated are moved after the plugins they depend on.
    /// Cycles are silently broken (the cycle member activates last).
    static QVector<LoadedPlugin> sortByDependencies(QVector<LoadedPlugin> plugins);

    QSet<QString> m_disabledIds;
    QSet<QString> m_activeProfiles;  // currently active language profile IDs
    QSet<QString> m_workspaceProfiles; // workspace-detected profiles (never suspended on tab switch)
    QString m_activeEditorLanguage;  // language of the current editor tab
    ContributionRegistry *m_contributions = nullptr;
};
