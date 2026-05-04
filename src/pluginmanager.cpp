#include "pluginmanager.h"
#include "sdk/permissionguard.h"
#include "sdk/cabi/cabi_bridge.h"
#include "sdk/luajit/luascriptengine.h"
#include "sdk/contributionregistry.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLibrary>
#include <QPluginLoader>
#include <QFileInfo>
#include <QSettings>
#include <stdexcept>

// C ABI function pointer types for resolving exported symbols.
using ExApiVersionFn   = int32_t (*)();
using ExDescribeFn     = ExPluginDescriptor (*)();
using ExInitializeFn   = int32_t (*)(const ExHostAPI *);
using ExShutdownFn     = void (*)();

PluginManager::PluginManager(QObject *parent)
    : QObject(parent)
{
    loadDisabledSet();
}

PluginManager::~PluginManager() = default;

int PluginManager::loadPluginsFrom(const QString &path)
{
    QDir dir(path);
    if (!dir.exists()) {
        return 0;
    }

    QStringList filters;
#if defined(Q_OS_WIN)
    filters << "*.dll";
#elif defined(Q_OS_MAC)
    filters << "*.dylib";
#else
    filters << "*.so";
#endif

    // ── Step 1: load all plugin binaries ─────────────────────────────────
    const int countBefore = m_loaded.size();
    const QFileInfoList entries = dir.entryInfoList(filters, QDir::Files);
    for (const QFileInfo &entry : entries) {
        auto *loader = new QPluginLoader(entry.absoluteFilePath(), this);
        QObject *instance = loader->instance();
        if (!instance) {
            loader->deleteLater();
            if (tryLoadCAbi(entry.absoluteFilePath()))
                continue;
            m_errors << QString("Not a Qt or C ABI plugin: %1").arg(entry.fileName());
            continue;
        }

        auto *plugin = qobject_cast<IPlugin *>(instance);
        if (!plugin) {
            m_errors << QString("Not an Exorcist plugin: %1").arg(entry.fileName());
            loader->unload();
            loader->deleteLater();
            continue;
        }

        m_loaded.push_back({plugin, loader});
    }

    // ── Step 2: enumerate all JSON manifests in the directory and match
    //           them to loaded plugins by the "id" field.
    //           This avoids any filename convention dependency (lib prefix,
    //           platform differences, etc.) — any .json with a matching id
    //           is accepted automatically.
    // Build id→index map for O(1) lookup instead of O(N*M) linear scan.
    QHash<QString, int> idToIndex;
    for (int i = 0; i < m_loaded.size(); ++i)
        idToIndex.insert(m_loaded[i].instance->info().id, i);

    const QFileInfoList jsonFiles = dir.entryInfoList(
        QStringList{QStringLiteral("*.json")}, QDir::Files);
    for (const QFileInfo &jf : jsonFiles) {
        QFile f(jf.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
        const QString manifestId = obj.value(QLatin1String("id")).toString();
        if (manifestId.isEmpty())
            continue;
        // Find the loaded plugin whose info().id matches this manifest's id.
        // Use index map for O(1) lookup instead of O(N) linear scan.
        auto it = idToIndex.find(manifestId);
        if (it != idToIndex.end() && m_loaded[it.value()].manifest.id.isEmpty())
            m_loaded[it.value()].manifest = PluginManifest::fromJson(obj);
    }

    return m_loaded.size() - countBefore;
}

void PluginManager::initializeAll(IHostServices *host)
{
    m_host = host;

    // Initialize Qt/C++ plugins — defer those with lazy activation events.
    // Skip disabled plugins entirely.
    // Language-specific plugins are also deferred until their profile activates.
    QVector<LoadedPlugin> immediate;
    for (const LoadedPlugin &lp : m_loaded) {
        const QString id = lp.instance->info().id;
        if (m_disabledIds.contains(id))
            continue;

        // Language-specific plugins are deferred until their profile activates,
        // unless the profile is already active.
        if (lp.manifest.isLanguagePlugin() && !isPluginAllowedByProfile(lp.manifest)) {
            m_deferred.push_back({lp, lp.manifest.activationEvents});
            continue;
        }

        if (shouldDeferPlugin(lp.manifest)) {
            m_deferred.push_back({lp, lp.manifest.activationEvents});
        } else {
            immediate.push_back(lp);
        }
    }

    immediate = sortByDependencies(immediate);
    for (const LoadedPlugin &lp : immediate) {
        activatePlugin(lp);
    }

    // Initialize C ABI plugins (always immediate).
    for (LoadedCAbiPlugin &cp : m_cabiLoaded) {
        cp.bridge = std::make_unique<cabi::CAbiPluginBridge>(host, this);

        auto initFn = reinterpret_cast<ExInitializeFn>(
            cp.library->resolve("ex_plugin_initialize"));
        if (!initFn) continue;
        if (!initFn(cp.bridge->api())) {
            m_errors << QString("C ABI plugin init returned failure: %1").arg(cp.id);
        }
    }
}

bool PluginManager::shouldDeferPlugin(const PluginManifest &manifest) const
{
    if (manifest.activationEvents.isEmpty())
        return false;  // No events = always active
    if (manifest.activatesOnStartup())
        return false;  // "*" = always active
    for (const QString &ev : manifest.activationEvents) {
        if (ev == QLatin1String("onStartupFinished"))
            return false;  // Start immediately
    }
    return true;  // Has lazy activation events
}

// Kahn's algorithm — topological sort of plugins by declared dependencies.
// Any plugin whose required dependency is not in this set activates last.
// Cycles are broken by treating the cycle-closing edge as optional.
QVector<PluginManager::LoadedPlugin> PluginManager::sortByDependencies(QVector<LoadedPlugin> plugins)
{
    // Build id → index map
    QHash<QString, int> indexById;
    for (int i = 0; i < plugins.size(); ++i) {
        const QString id = plugins[i].manifest.id.isEmpty()
            ? plugins[i].instance->info().id
            : plugins[i].manifest.id;
        indexById.insert(id, i);
    }

    const int n = plugins.size();
    QVector<int> inDegree(n, 0);
    QVector<QVector<int>> adj(n); // adj[i] = list of indices that depend on i

    for (int i = 0; i < n; ++i) {
        for (const PluginDependency &dep : plugins[i].manifest.dependencies) {
            auto it = indexById.find(dep.pluginId);
            if (it == indexById.end())
                continue; // dependency not in this set — ignore (ok for optional; warn for required)
            const int j = it.value();
            adj[j].push_back(i);   // j must come before i (both required and optional define ordering)
            ++inDegree[i];
        }
    }

    // Standard BFS topological sort (Kahn's algorithm)
    QVector<int> queue;
    for (int i = 0; i < n; ++i)
        if (inDegree[i] == 0)
            queue.push_back(i);

    QVector<LoadedPlugin> sorted;
    sorted.reserve(n);
    int head = 0;
    while (head < queue.size()) {
        const int cur = queue[head++];
        sorted.push_back(plugins[cur]);
        for (int next : adj[cur]) {
            if (--inDegree[next] == 0)
                queue.push_back(next);
        }
    }

    // Any nodes not yet added are part of a cycle — append them as-is
    if (sorted.size() < n) {
        for (int i = 0; i < n; ++i) {
            if (inDegree[i] > 0)
                sorted.push_back(plugins[i]);
        }
        qWarning("[PluginManager] Dependency cycle detected — some plugins may "
                 "initialize in an undefined order.");
    }

    return sorted;
}

void PluginManager::activatePlugin(const LoadedPlugin &lp)
{
    try {
        const PluginInfo pi = lp.instance->info();
        QList<PluginPermission> permissions = pi.requestedPermissions;
        for (const PluginPermission permission : lp.manifest.requestedPermissions) {
            if (!permissions.contains(permission))
                permissions.append(permission);
        }

        // ── apiVersion compatibility check ───────────────────────────────
        if (!pi.apiVersion.isEmpty()) {
            const QStringList parts = pi.apiVersion.split(QLatin1Char('.'));
            const int pluginMajor = parts.value(0).toInt();
            if (pluginMajor != EXORCIST_SDK_VERSION_MAJOR) {
                m_errors << QString("Plugin '%1' requires SDK %2, host is %3.%4 — skipped")
                                .arg(pi.id, pi.apiVersion)
                                .arg(EXORCIST_SDK_VERSION_MAJOR)
                                .arg(EXORCIST_SDK_VERSION_MINOR);
                return;
            }
        }

        auto guard = std::make_unique<PermissionGuardedHostServices>(m_host, permissions);
        lp.instance->initialize(guard.get());
        m_permGuards.push_back(std::move(guard));
    } catch (const std::exception &e) {
        m_errors << QString("Plugin init failed: %1").arg(e.what());
    } catch (...) {
        m_errors << QString("Plugin init failed with unknown exception");
    }
}

int PluginManager::activateByEvent(const QString &event)
{
    int activated = 0;
    QVector<DeferredPlugin> remaining;
    QVector<LoadedPlugin> toActivate;

    for (const DeferredPlugin &dp : m_deferred) {
        bool matches = false;
        for (const QString &ev : dp.activationEvents) {
            if (ev == event) {
                matches = true;
                break;
            }
        }
        if (matches) {
            toActivate.push_back(dp.loaded);
        } else {
            remaining.push_back(dp);
        }
    }

    toActivate = sortByDependencies(toActivate);
    for (const LoadedPlugin &lp : toActivate) {
        activatePlugin(lp);
        ++activated;
        if (m_contributions && lp.manifest.hasContributions()) {
            m_contributions->registerManifest(
                lp.instance->info().id,
                lp.manifest,
                lp.instance);
        }
    }

    m_deferred = remaining;
    return activated;
}

int PluginManager::activateByWorkspace(const QString &workspaceRoot)
{
    if (workspaceRoot.isEmpty())
        return 0;

    QVector<DeferredPlugin> toActivate;
    QVector<DeferredPlugin> remaining;
    const QDir wsDir(workspaceRoot);

    for (const DeferredPlugin &dp : m_deferred) {
        bool matches = false;
        for (const QString &ev : dp.activationEvents) {
            if (ev.startsWith(QLatin1String("workspaceContains:"))) {
                const QString pattern = ev.mid(18);  // after "workspaceContains:"
                const QStringList found = wsDir.entryList(
                    QStringList{pattern}, QDir::Files | QDir::Dirs);
                if (!found.isEmpty()) {
                    matches = true;
                    break;
                }
            }
        }
        if (matches)
            toActivate.push_back(dp);
        else
            remaining.push_back(dp);
    }

    // Collect LoadedPlugins, sort by dependency order, then activate
    QVector<LoadedPlugin> loaded;
    loaded.reserve(toActivate.size());
    for (const DeferredPlugin &dp : toActivate)
        loaded.push_back(dp.loaded);
    loaded = sortByDependencies(loaded);

    for (const LoadedPlugin &lp : loaded) {
        activatePlugin(lp);
        if (m_contributions && lp.manifest.hasContributions()) {
            m_contributions->registerManifest(
                lp.instance->info().id,
                lp.manifest,
                lp.instance);
        }
    }

    m_deferred = remaining;
    return loaded.size();
}

void PluginManager::initializeAll(QObject *services)
{
    for (int i = 0; i < m_loaded.size(); ++i) {
        const LoadedPlugin &lp = m_loaded[i];
        try {
            lp.instance->initialize(services);
        } catch (const std::exception &e) {
            m_errors << QString("Plugin init failed: %1").arg(e.what());
        } catch (...) {
            m_errors << QString("Plugin init failed with unknown exception");
        }
    }
}

bool PluginManager::isPluginDeferred(const IPlugin *instance) const
{
    for (const DeferredPlugin &dp : m_deferred) {
        if (dp.loaded.instance == instance)
            return true;
    }
    return false;
}

void PluginManager::notifyWorkspaceChanged(const QString &root)
{
    // Build O(1) lookup set instead of calling isPluginDeferred() per plugin (O(N*M)).
    QSet<const IPlugin *> deferredSet;
    deferredSet.reserve(m_deferred.size());
    for (const DeferredPlugin &dp : m_deferred)
        deferredSet.insert(dp.loaded.instance);

    for (const LoadedPlugin &lp : m_loaded) {
        if (deferredSet.contains(lp.instance))
            continue;
        try {
            lp.instance->onWorkspaceChanged(root);
            // Phase 3: also fire the explicit open/close hooks (rule L2).
            // Default impls forward to onWorkspaceChanged so backward
            // compatibility is preserved for plugins that haven't migrated.
            if (root.isEmpty())
                lp.instance->onWorkspaceClosed();
            else
                lp.instance->onWorkspaceOpened(root);
        } catch (...) {
        }
    }
}

void PluginManager::shutdownAll()
{
    // Call shutdown() on each plugin to stop services (clangd, GDB, etc.)
    // but do NOT call unload() here. Plugin DLLs may still have live QWidget
    // subclasses parented to the MainWindow dock system; unloading the DLL
    // while those widgets exist invalidates their vtable pointers and causes
    // heap corruption when MainWindow's destructor later destroys its children.
    // The OS will unload all DLLs cleanly after process exit.
    for (const LoadedPlugin &lp : m_loaded) {
        try {
            lp.instance->shutdown();
        } catch (...) {
        }
    }
    m_loaded.clear();
    m_deferred.clear();

    // Shutdown C ABI plugins — call shutdown fn but skip unload() for the
    // same reason: the bridge object may hold Qt widgets still alive.
    for (LoadedCAbiPlugin &cp : m_cabiLoaded) {
        auto shutdownFn = reinterpret_cast<ExShutdownFn>(
            cp.library->resolve("ex_plugin_shutdown"));
        if (shutdownFn) shutdownFn();
        cp.bridge.reset();
        // cp.library->unload() intentionally omitted — see comment above.
    }
    m_cabiLoaded.clear();

    // Shutdown Lua script plugins.
    m_luaEngine.reset();
}

const QStringList &PluginManager::errors() const
{
    return m_errors;
}

QVector<IPlugin *> PluginManager::plugins() const
{
    QVector<IPlugin *> result;
    result.reserve(m_loaded.size());
    for (const LoadedPlugin &plugin : m_loaded) {
        result.push_back(plugin.instance);
    }
    return result;
}

QVector<QObject *> PluginManager::pluginObjects() const
{
    QVector<QObject *> result;
    result.reserve(m_loaded.size());
    for (const LoadedPlugin &lp : m_loaded)
        result.push_back(lp.loader->instance());
    return result;
}

bool PluginManager::tryLoadCAbi(const QString &filePath)
{
    auto lib = std::make_unique<QLibrary>(filePath);
    if (!lib->load()) {
        return false;
    }

    // Resolve required entry points.
    auto versionFn  = reinterpret_cast<ExApiVersionFn>(lib->resolve("ex_plugin_api_version"));
    auto describeFn = reinterpret_cast<ExDescribeFn>(lib->resolve("ex_plugin_describe"));
    auto initFn     = reinterpret_cast<ExInitializeFn>(lib->resolve("ex_plugin_initialize"));
    auto shutdownFn = reinterpret_cast<ExShutdownFn>(lib->resolve("ex_plugin_shutdown"));

    if (!versionFn || !describeFn || !initFn || !shutdownFn) {
        lib->unload();
        return false;
    }

    // Check ABI version compatibility (major must match).
    const int32_t ver = versionFn();
    const int pluginMajor = (ver >> 16) & 0xFFFF;
    if (pluginMajor != EX_ABI_VERSION_MAJOR) {
        m_errors << QString("C ABI plugin ABI version mismatch (host=%1, plugin=%2): %3")
                        .arg(EX_ABI_VERSION_MAJOR).arg(pluginMajor)
                        .arg(QFileInfo(filePath).fileName());
        lib->unload();
        return false;
    }

    const ExPluginDescriptor desc = describeFn();
    const QString pluginId = QString::fromUtf8(desc.id ? desc.id : "unknown");
    qInfo("Loaded C ABI plugin: %s (%s)",
          qUtf8Printable(pluginId),
          qUtf8Printable(QFileInfo(filePath).fileName()));

    // Bridge is created now; initialization happens in initializeAll().
    m_cabiLoaded.push_back({std::move(lib), nullptr, pluginId});
    return true;
}

QList<IAgentProvider *> PluginManager::cabiProviders() const
{
    QList<IAgentProvider *> result;
    for (const LoadedCAbiPlugin &cp : m_cabiLoaded) {
        if (cp.bridge)
            result.append(cp.bridge->providers());
    }
    return result;
}

int PluginManager::loadLuaPluginsFrom(const QString &path, IHostServices *host)
{
    m_luaEngine = std::make_unique<luabridge::LuaScriptEngine>(host, this);
    const int count = m_luaEngine->loadPluginsFrom(path);
    if (count > 0) {
        m_luaEngine->initializeAll();
        m_luaEngine->enableHotReload(path);
    }
    return count;
}

QStringList PluginManager::luaErrors() const
{
    return m_luaEngine ? m_luaEngine->errors() : QStringList();
}

void PluginManager::fireLuaEvent(const QString &eventName, const QStringList &args)
{
    if (m_luaEngine)
        m_luaEngine->fireEvent(eventName, args);
}

QVector<luabridge::LuaPluginInfo> PluginManager::loadedLuaScripts() const
{
    QVector<luabridge::LuaPluginInfo> result;
    if (m_luaEngine) {
        for (const auto &lp : m_luaEngine->plugins())
            result.append(lp.info);
    }
    return result;
}

// ── Enable / Disable ─────────────────────────────────────────────────────────

void PluginManager::loadDisabledSet()
{
    QSettings s;
    const QStringList ids = s.value(QStringLiteral("plugins/disabled")).toStringList();
    m_disabledIds = QSet<QString>(ids.begin(), ids.end());
}

void PluginManager::setPluginDisabled(const QString &pluginId, bool disabled)
{
    if (disabled)
        m_disabledIds.insert(pluginId);
    else
        m_disabledIds.remove(pluginId);

    QSettings s;
    s.setValue(QStringLiteral("plugins/disabled"),
              QStringList(m_disabledIds.begin(), m_disabledIds.end()));
}

bool PluginManager::isPluginDisabled(const QString &pluginId) const
{
    return m_disabledIds.contains(pluginId);
}

// ── Language Profile Activation ──────────────────────────────────────────────

void PluginManager::setActiveLanguageProfiles(const QSet<QString> &profileIds)
{
    m_activeProfiles = profileIds;
    m_workspaceProfiles = profileIds;  // initial set is workspace-level
}

void PluginManager::addWorkspaceLanguage(const QString &languageId)
{
    m_workspaceProfiles.insert(languageId);
    m_activeProfiles.insert(languageId);
}

bool PluginManager::isPluginAllowedByProfile(const PluginManifest &manifest) const
{
    // General / non-language plugins are always allowed
    if (manifest.isGeneralPlugin())
        return true;

    // Language plugins need at least one of their languageIds
    // to be in the active profiles set
    for (const QString &lid : manifest.languageIds) {
        if (m_activeProfiles.contains(lid))
            return true;
    }
    return false;
}

int PluginManager::activateByLanguageProfile(const QString &languageId)
{
    if (languageId.isEmpty())
        return 0;

    m_activeProfiles.insert(languageId);

    int activated = 0;
    QVector<DeferredPlugin> remaining;

    for (const DeferredPlugin &dp : m_deferred) {
        const PluginManifest &m = dp.loaded.manifest;
        if (m.isLanguagePlugin() && m.languageIds.contains(languageId)) {
            activatePlugin(dp.loaded);
            ++activated;
        } else {
            remaining.push_back(dp);
        }
    }

    // Also resume any suspended plugins for this language
    for (LoadedPlugin &lp : m_loaded) {
        if (lp.suspended && lp.manifest.isLanguagePlugin()
            && lp.manifest.languageIds.contains(languageId)) {
            try {
                lp.instance->resume();
            } catch (...) {
                qWarning("Plugin '%s' threw during resume()",
                         qUtf8Printable(lp.instance->info().id));
            }
            lp.suspended = false;
            if (m_contributions)
                m_contributions->registerManifest(lp.instance->info().id,
                                                  lp.manifest, lp.instance);
            ++activated;
        }
    }

    m_deferred = remaining;
    return activated;
}

int PluginManager::activateByPluginId(const QString &pluginId)
{
    if (pluginId.isEmpty())
        return 0;

    int activated = 0;
    QVector<DeferredPlugin> remaining;

    for (const DeferredPlugin &dp : m_deferred) {
        if (dp.loaded.manifest.id == pluginId) {
            activatePlugin(dp.loaded);
            ++activated;
            if (m_contributions && dp.loaded.manifest.hasContributions()) {
                m_contributions->registerManifest(
                    dp.loaded.instance->info().id,
                    dp.loaded.manifest,
                    dp.loaded.instance);
            }
        } else {
            remaining.push_back(dp);
        }
    }

    m_deferred = remaining;
    return activated;
}

int PluginManager::suspendByLanguageProfile(const QString &languageId)
{
    if (languageId.isEmpty())
        return 0;

    int suspended = 0;
    for (LoadedPlugin &lp : m_loaded) {
        if (lp.suspended)
            continue;
        if (!lp.manifest.isLanguagePlugin())
            continue;
        if (!lp.manifest.languageIds.contains(languageId))
            continue;

        // Don't suspend if the plugin also serves another active language
        bool servedByOtherProfile = false;
        for (const QString &lid : lp.manifest.languageIds) {
            if (lid != languageId && m_activeProfiles.contains(lid)) {
                servedByOtherProfile = true;
                break;
            }
        }
        if (servedByOtherProfile)
            continue;

        // Unregister contributions before suspending
        if (m_contributions)
            m_contributions->unregisterPlugin(lp.instance->info().id);

        try {
            lp.instance->suspend();
        } catch (...) {
            qWarning("Plugin '%s' threw during suspend()",
                     qUtf8Printable(lp.instance->info().id));
        }
        lp.suspended = true;
        ++suspended;
    }
    return suspended;
}

int PluginManager::resumeByLanguageProfile(const QString &languageId)
{
    if (languageId.isEmpty())
        return 0;

    int resumed = 0;
    for (LoadedPlugin &lp : m_loaded) {
        if (!lp.suspended)
            continue;
        if (!lp.manifest.isLanguagePlugin())
            continue;
        if (!lp.manifest.languageIds.contains(languageId))
            continue;

        try {
            lp.instance->resume();
        } catch (...) {
            qWarning("Plugin '%s' threw during resume()",
                     qUtf8Printable(lp.instance->info().id));
        }
        lp.suspended = false;

        // Re-register contributions
        if (m_contributions)
            m_contributions->registerManifest(lp.instance->info().id,
                                              lp.manifest, lp.instance);
        ++resumed;
    }
    return resumed;
}

void PluginManager::switchActiveLanguage(const QString &newLanguageId)
{
    if (newLanguageId == m_activeEditorLanguage)
        return;

    const QString prev = m_activeEditorLanguage;
    m_activeEditorLanguage = newLanguageId;

    // Suspend plugins for the previous language ONLY if it's not a
    // workspace-level profile (workspace profiles stay active always)
    if (!prev.isEmpty() && !m_workspaceProfiles.contains(prev)) {
        suspendByLanguageProfile(prev);
        m_activeProfiles.remove(prev);
    }

    // Activate / resume plugins for the new language
    if (!newLanguageId.isEmpty()) {
        m_activeProfiles.insert(newLanguageId);
        activateByLanguageProfile(newLanguageId);
    }
}

void PluginManager::setContributionRegistry(ContributionRegistry *registry)
{
    m_contributions = registry;
}
