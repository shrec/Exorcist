#include "pluginmanager.h"
#include "sdk/permissionguard.h"
#include "sdk/cabi/cabi_bridge.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLibrary>
#include <QPluginLoader>
#include <QFileInfo>
#include <stdexcept>

// C ABI function pointer types for resolving exported symbols.
using ExApiVersionFn   = int32_t (*)();
using ExDescribeFn     = ExPluginDescriptor (*)();
using ExInitializeFn   = int32_t (*)(const ExHostAPI *);
using ExShutdownFn     = void (*)();

PluginManager::PluginManager(QObject *parent)
    : QObject(parent)
{
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
    // Initialize Qt/C++ plugins.
    for (const LoadedPlugin &lp : m_loaded) {
        try {
            const PluginInfo pi = lp.instance->info();
            auto guard = std::make_unique<PermissionGuardedHostServices>(host, pi.requestedPermissions);
            lp.instance->initialize(guard.get());
            m_permGuards.push_back(std::move(guard));
        } catch (const std::exception &e) {
            m_errors << QString("Plugin init failed: %1").arg(e.what());
        } catch (...) {
            m_errors << QString("Plugin init failed with unknown exception");
        }
    }

    // Initialize C ABI plugins.
    for (LoadedCAbiPlugin &cp : m_cabiLoaded) {
        // Create the bridge (translates C ABI ↔ Qt/C++ services).
        cp.bridge = new cabi::CAbiPluginBridge(host, this);

        auto initFn = reinterpret_cast<ExInitializeFn>(
            cp.library->resolve("ex_plugin_initialize"));
        if (!initFn) continue;
        if (!initFn(cp.bridge->api())) {
            m_errors << QString("C ABI plugin init returned failure: %1").arg(cp.id);
        }
    }
}

void PluginManager::initializeAll(QObject *services)
{
    for (const LoadedPlugin &lp : m_loaded) {
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

    // Shutdown C ABI plugins.
    for (LoadedCAbiPlugin &cp : m_cabiLoaded) {
        auto shutdownFn = reinterpret_cast<ExShutdownFn>(
            cp.library->resolve("ex_plugin_shutdown"));
        if (shutdownFn) shutdownFn();
        cp.library->unload();
        delete cp.bridge; // QObject children also cleaned up
        delete cp.library;
    }
    m_cabiLoaded.clear();
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
    auto *lib = new QLibrary(filePath);
    if (!lib->load()) {
        delete lib;
        return false;
    }

    // Resolve required entry points.
    auto versionFn  = reinterpret_cast<ExApiVersionFn>(lib->resolve("ex_plugin_api_version"));
    auto describeFn = reinterpret_cast<ExDescribeFn>(lib->resolve("ex_plugin_describe"));
    auto initFn     = reinterpret_cast<ExInitializeFn>(lib->resolve("ex_plugin_initialize"));
    auto shutdownFn = reinterpret_cast<ExShutdownFn>(lib->resolve("ex_plugin_shutdown"));

    if (!versionFn || !describeFn || !initFn || !shutdownFn) {
        lib->unload();
        delete lib;
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
        delete lib;
        return false;
    }

    const ExPluginDescriptor desc = describeFn();
    const QString pluginId = QString::fromUtf8(desc.id ? desc.id : "unknown");
    qInfo("Loaded C ABI plugin: %s (%s)",
          qUtf8Printable(pluginId),
          qUtf8Printable(QFileInfo(filePath).fileName()));

    // Bridge is created now; initialization happens in initializeAll().
    m_cabiLoaded.push_back({lib, nullptr, pluginId});
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
