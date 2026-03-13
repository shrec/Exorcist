#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QJsonObject>

class QNetworkAccessManager;
class QNetworkReply;
class PluginManager;

/// Registry entry describing an available plugin.
struct MarketplaceEntry
{
    QString id;
    QString name;
    QString version;
    QString author;
    QString description;
    QString homepage;
    QString downloadUrl;
    QString minIdeVersion;
    QStringList tags;

    QJsonObject toJson() const;
    static MarketplaceEntry fromJson(const QJsonObject &obj);
};

/// Service that manages browsing, downloading, and installing plugins
/// from a local or remote registry.
///
/// Lifecycle:
///  1. loadRegistry(url) or loadRegistryFromFile(path)
///  2. User picks a plugin → downloadAndInstall(entry)
///  3. Service downloads .zip, extracts to plugins dir, signals installed
///
/// Does NOT modify MainWindow (MainWindow FREEZE compliant).
class PluginMarketplaceService : public QObject
{
    Q_OBJECT

public:
    explicit PluginMarketplaceService(QObject *parent = nullptr);

    /// Set the PluginManager for reload after install.
    void setPluginManager(PluginManager *manager);

    /// Set the directory where plugins are installed (default: appDir/plugins).
    void setPluginsDirectory(const QString &path);
    QString pluginsDirectory() const { return m_pluginsDir; }

    /// Load registry from a local JSON file (array of MarketplaceEntry).
    bool loadRegistryFromFile(const QString &jsonPath);

    /// Load registry from a remote URL (async — emits registryLoaded on success).
    void loadRegistryFromUrl(const QString &url);

    /// Get the currently loaded registry entries.
    const QList<MarketplaceEntry> &entries() const { return m_entries; }

    /// Download and install a plugin by its marketplace entry.
    /// Emits downloadProgress, pluginInstalled, or error.
    void downloadAndInstall(const MarketplaceEntry &entry);

    /// Uninstall a plugin by removing its directory.
    bool uninstallPlugin(const QString &pluginId, QString *error = nullptr);

    /// Check which installed plugins have updates available.
    QList<MarketplaceEntry> availableUpdates() const;

signals:
    /// Registry entries have been loaded successfully.
    void registryLoaded(int count);

    /// Download progress for a plugin (0–100).
    void downloadProgress(const QString &pluginId, int percent);

    /// A plugin was successfully installed.
    void pluginInstalled(const QString &pluginId);

    /// A plugin was uninstalled.
    void pluginUninstalled(const QString &pluginId);

    /// An error occurred during registry load, download, or install.
    void error(const QString &message);

private:
    void parseRegistryData(const QByteArray &data);
    void handleDownloadFinished(QNetworkReply *reply, const MarketplaceEntry &entry);
    bool extractZip(const QString &zipPath, const QString &destDir, QString *error);

    QNetworkAccessManager *m_network = nullptr;
    PluginManager *m_pluginManager = nullptr;
    QString m_pluginsDir;
    QList<MarketplaceEntry> m_entries;
};
