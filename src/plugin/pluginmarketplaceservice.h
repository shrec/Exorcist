#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QJsonObject>

class QNetworkAccessManager;
class QNetworkReply;
class QProcess;
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
    QString sha256;        // hex-encoded SHA-256 of the download archive
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

    /// Install a plugin directly from a Git URL or .zip download URL.
    /// `subdir`  — optional subdirectory inside the cloned/extracted tree
    ///             (e.g. "plugins/myplugin" for monorepos). May be empty.
    /// `buildAfter`  — invoke `cmake -B build && cmake --build build`
    ///                 inside the plugin folder after install.
    /// `activateAfter` — call PluginManager::loadPluginsFrom() and try to
    ///                   activate the plugin once the build completes.
    ///
    /// Emits installProgress / installLog continuously; emits a single
    /// installFinished(...) when finished (success or failure or cancel).
    /// Returns immediately; flow runs asynchronously on the calling thread
    /// via QProcess and QNetworkAccessManager event loops.
    void installFromUrl(const QString &url,
                        const QString &subdir,
                        bool buildAfter,
                        bool activateAfter);

    /// Cancel an in-flight installFromUrl(). Safe to call when no install
    /// is running. Emits installFinished(false, "", "Cancelled by user").
    void cancelInstall();

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

    // ── Signals for installFromUrl() ─────────────────────────────────────────

    /// Coarse progress for the installFromUrl() flow.
    /// `percent` is 0–100 (or -1 when indeterminate).
    /// `message` is a human-readable phase label (e.g. "Cloning…").
    void installProgress(int percent, const QString &message);

    /// Live log line from the install flow (git stdout/stderr,
    /// extraction output, build output). Multi-line strings are split
    /// upstream; consumers append directly.
    void installLog(const QString &line);

    /// Final outcome of installFromUrl().
    /// On success, `pluginId` is the manifest id of the freshly installed
    /// plugin and `errorMessage` is empty. On failure, `pluginId` is empty
    /// and `errorMessage` holds the user-visible reason.
    void installFinished(bool success,
                         const QString &pluginId,
                         const QString &errorMessage);

private:
    void parseRegistryData(const QByteArray &data);
    void handleDownloadFinished(QNetworkReply *reply, const MarketplaceEntry &entry);
    bool extractZip(const QString &zipPath, const QString &destDir, QString *error);

    // ── installFromUrl() implementation pieces ───────────────────────────────

    enum class InstallStage {
        Idle,
        Cloning,
        Downloading,
        Extracting,
        Reading,
        Moving,
        Building,
        Activating,
    };

    void startGitClone(const QString &url);
    void startZipDownload(const QString &url);
    void afterFetch(const QString &fetchedRoot);
    void startBuild(const QString &pluginRoot);
    void finishInstall(bool success,
                       const QString &pluginId,
                       const QString &errorMessage);
    void finalizeAndActivate(const QString &finalPluginRoot,
                             const QString &pluginId);
    QString readManifestId(const QString &pluginRoot, QString *name,
                           QString *version, QString *err);

    QNetworkAccessManager *m_network = nullptr;
    PluginManager *m_pluginManager = nullptr;
    QString m_pluginsDir;
    QList<MarketplaceEntry> m_entries;

    // installFromUrl() state — only one flow may run at a time.
    InstallStage m_stage      = InstallStage::Idle;
    QProcess    *m_proc       = nullptr;     // current child (git / cmake)
    QNetworkReply *m_zipReply = nullptr;     // current zip download reply
    QString      m_inputUrl;
    QString      m_subdir;
    bool         m_buildAfter    = true;
    bool         m_activateAfter = true;
    QString      m_cacheRoot;                // plugins/.cache/<token>/
    QString      m_finalPluginRoot;          // plugins/<id>/
    bool         m_cancelled = false;
};
