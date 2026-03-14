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

    const QFileInfoList entries = dir.entryInfoList(filters, QDir::Files);
    for (const QFileInfo &entry : entries) {
        auto *loader = new QPluginLoader(entry.absoluteFilePath(), this);
        QObject *instance = loader->instance();
        if (!instance) {
            // QPluginLoader failed — try loading as a C ABI plugin.
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

        // Try to load plugin.json manifest from the same directory
        const QString manifestPath = entry.absolutePath() + "/" + entry.completeBaseName() + ".json";
        if (QFile::exists(manifestPath)) {
            QFile f(manifestPath);
            if (f.open(QIODevice::ReadOnly)) {
                const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
                m_loaded.back().manifest = PluginManifest::fromJson(obj);
            }
        }

    }

    return m_loaded.size();
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

void PluginManager::activatePlugin(const LoadedPlugin &lp)
{
    try {
        const PluginInfo pi = lp.instance->info();

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

        auto guard = std::make_unique<PermissionGuardedHostServices>(m_host, pi.requestedPermissions);
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

    for (const DeferredPlugin &dp : m_deferred) {
        bool matches = false;
        for (const QString &ev : dp.activationEvents) {
            if (ev == event) {
                matches = true;
                break;
            }
        }
        if (matches) {
            activatePlugin(dp.loaded);
            ++activated;
        } else {
            remaining.push_back(dp);
        }
    }

    m_deferred = remaining;
    return activated;
}

int PluginManager::activateByWorkspace(const QString &workspaceRoot)
{
    if (workspaceRoot.isEmpty())
        return 0;

    int activated = 0;
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
        if (matches) {
            activatePlugin(dp.loaded);
            ++activated;
        } else {
            remaining.push_back(dp);
        }
    }

    m_deferred = remaining;
    return activated;
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

void PluginManager::shutdownAll()
{
    for (const LoadedPlugin &lp : m_loaded) {
        try {
            lp.instance->shutdown();
        } catch (...) {
        }
        lp.loader->unload();
    }
    m_loaded.clear();
    m_deferred.clear();

    // Shutdown C ABI plugins.
    for (LoadedCAbiPlugin &cp : m_cabiLoaded) {
        auto shutdownFn = reinterpret_cast<ExShutdownFn>(
            cp.library->resolve("ex_plugin_shutdown"));
        if (shutdownFn) shutdownFn();
        cp.library->unload();
        cp.bridge.reset();
        cp.library.reset();
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
