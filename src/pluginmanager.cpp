#include "pluginmanager.h"
#include "sdk/permissionguard.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QPluginLoader>
#include <QFileInfo>
#include <stdexcept>

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
            m_errors << loader->errorString();
            loader->deleteLater();
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
    for (const LoadedPlugin &lp : m_loaded) {
        try {
            // Create a permission-guarded wrapper scoped to this plugin's permissions
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
