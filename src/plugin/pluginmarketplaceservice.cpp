#include "pluginmarketplaceservice.h"
#include "pluginmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QCryptographicHash>
#include <QProcess>
#include <QTemporaryFile>

// ── MarketplaceEntry ─────────────────────────────────────────────────────────

QJsonObject MarketplaceEntry::toJson() const
{
    QJsonObject o;
    o[QLatin1String("id")]            = id;
    o[QLatin1String("name")]          = name;
    o[QLatin1String("version")]       = version;
    o[QLatin1String("author")]        = author;
    o[QLatin1String("description")]   = description;
    o[QLatin1String("homepage")]      = homepage;
    o[QLatin1String("downloadUrl")]   = downloadUrl;
    o[QLatin1String("minIdeVersion")] = minIdeVersion;
    if (!sha256.isEmpty())
        o[QLatin1String("sha256")] = sha256;
    if (!tags.isEmpty()) {
        QJsonArray arr;
        for (const auto &t : tags)
            arr.append(t);
        o[QLatin1String("tags")] = arr;
    }
    return o;
}

MarketplaceEntry MarketplaceEntry::fromJson(const QJsonObject &obj)
{
    MarketplaceEntry e;
    e.id            = obj.value(QLatin1String("id")).toString();
    e.name          = obj.value(QLatin1String("name")).toString();
    e.version       = obj.value(QLatin1String("version")).toString();
    e.author        = obj.value(QLatin1String("author")).toString();
    e.description   = obj.value(QLatin1String("description")).toString();
    e.homepage      = obj.value(QLatin1String("homepage")).toString();
    e.downloadUrl   = obj.value(QLatin1String("downloadUrl")).toString();
    e.minIdeVersion = obj.value(QLatin1String("minIdeVersion")).toString();
    e.sha256        = obj.value(QLatin1String("sha256")).toString();
    const auto arr  = obj.value(QLatin1String("tags")).toArray();
    for (const auto &v : arr)
        e.tags.append(v.toString());
    return e;
}

// ── PluginMarketplaceService ─────────────────────────────────────────────────

PluginMarketplaceService::PluginMarketplaceService(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    m_pluginsDir = QCoreApplication::applicationDirPath()
                   + QLatin1String("/plugins");
}

void PluginMarketplaceService::setPluginManager(PluginManager *manager)
{
    m_pluginManager = manager;
}

void PluginMarketplaceService::setPluginsDirectory(const QString &path)
{
    m_pluginsDir = path;
}

// ── Registry loading ─────────────────────────────────────────────────────────

bool PluginMarketplaceService::loadRegistryFromFile(const QString &jsonPath)
{
    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) {
        emit error(tr("Cannot open registry file: %1").arg(jsonPath));
        return false;
    }

    parseRegistryData(f.readAll());
    return !m_entries.isEmpty();
}

void PluginMarketplaceService::loadRegistryFromUrl(const QString &url)
{
    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("ExorcistIDE/1.0"));
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    auto *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit error(tr("Registry download failed: %1").arg(reply->errorString()));
            return;
        }
        parseRegistryData(reply->readAll());
    });
}

void PluginMarketplaceService::parseRegistryData(const QByteArray &data)
{
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        emit error(tr("Invalid registry JSON: %1").arg(parseError.errorString()));
        return;
    }

    m_entries.clear();
    const auto arr = doc.array();
    for (const auto &v : arr) {
        auto entry = MarketplaceEntry::fromJson(v.toObject());
        if (!entry.id.isEmpty())
            m_entries.append(std::move(entry));
    }

    emit registryLoaded(m_entries.size());
}

// ── Download & install ───────────────────────────────────────────────────────

void PluginMarketplaceService::downloadAndInstall(const MarketplaceEntry &entry)
{
    if (entry.downloadUrl.isEmpty()) {
        emit error(tr("No download URL for plugin: %1").arg(entry.name));
        return;
    }

    QNetworkRequest request;
    request.setUrl(QUrl(entry.downloadUrl));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("ExorcistIDE/1.0"));
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    auto *reply = m_network->get(request);

    connect(reply, &QNetworkReply::downloadProgress,
            this, [this, id = entry.id](qint64 received, qint64 total) {
        if (total > 0)
            emit downloadProgress(id, static_cast<int>(received * 100 / total));
    });

    connect(reply, &QNetworkReply::finished,
            this, [this, reply, entry]() {
        reply->deleteLater();
        handleDownloadFinished(reply, entry);
    });
}

static bool isValidPluginId(const QString &id)
{
    if (id.isEmpty())
        return false;
    // Plugin IDs must be simple alphanumeric + hyphen/underscore/dot.
    // Block path separators and traversal sequences.
    for (const QChar &ch : id) {
        if (ch == QLatin1Char('/') || ch == QLatin1Char('\\'))
            return false;
    }
    if (id.contains(QLatin1String("..")))
        return false;
    return true;
}

static bool isPathInside(const QString &child, const QString &parent)
{
    const QString canonChild  = QDir(child).canonicalPath();
    const QString canonParent = QDir(parent).canonicalPath();
    if (canonChild.isEmpty() || canonParent.isEmpty())
        return false;
    return canonChild.startsWith(canonParent + QLatin1Char('/'))
        || canonChild == canonParent;
}

void PluginMarketplaceService::handleDownloadFinished(
    QNetworkReply *reply, const MarketplaceEntry &entry)
{
    if (reply->error() != QNetworkReply::NoError) {
        emit error(tr("Download failed for %1: %2")
                       .arg(entry.name, reply->errorString()));
        return;
    }

    // ── Path traversal check on entry.id ──────────────────────────────
    if (!isValidPluginId(entry.id)) {
        emit error(tr("Invalid plugin ID (path traversal blocked): %1").arg(entry.id));
        return;
    }

    // Write to a temporary file
    const QString suffix = entry.downloadUrl.endsWith(QLatin1String(".tar.gz"))
                               ? QStringLiteral(".tar.gz")
                               : QStringLiteral(".zip");

    QTemporaryFile tmpFile;
    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cacheDir);
    tmpFile.setFileTemplate(cacheDir + QLatin1String("/exorcist_plugin_XXXXXX") + suffix);
    tmpFile.setAutoRemove(false);

    if (!tmpFile.open()) {
        emit error(tr("Cannot create temp file for %1").arg(entry.name));
        return;
    }

    const QByteArray data = reply->readAll();

    // ── SHA-256 integrity verification ─────────────────────────────
    if (!entry.sha256.isEmpty()) {
        const QByteArray actual = QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
        if (actual != entry.sha256.toLatin1()) {
            emit error(tr("Integrity check failed for %1: expected SHA-256 %2, got %3")
                           .arg(entry.name, entry.sha256, QString::fromLatin1(actual)));
            return;
        }
    }

    tmpFile.write(data);
    const QString tmpPath = tmpFile.fileName();
    tmpFile.close();

    // Extract to plugins dir
    const QString destDir = m_pluginsDir + QLatin1Char('/') + entry.id;

    QString extractError;
    if (!extractZip(tmpPath, destDir, &extractError)) {
        QFile::remove(tmpPath);
        emit error(tr("Install failed for %1: %2").arg(entry.name, extractError));
        return;
    }

    // ── Post-extraction: verify all files are inside destDir ─────────
    if (!isPathInside(destDir, m_pluginsDir)) {
        QDir(destDir).removeRecursively();
        QFile::remove(tmpPath);
        emit error(tr("Install rejected: extracted path escapes plugins directory"));
        return;
    }

    QFile::remove(tmpPath);
    emit pluginInstalled(entry.id);
}

// ── Uninstall ────────────────────────────────────────────────────────────────

bool PluginMarketplaceService::uninstallPlugin(const QString &pluginId, QString *err)
{
    // ── Validate pluginId against path traversal ─────────────────────
    if (!isValidPluginId(pluginId)) {
        if (err) *err = tr("Invalid plugin ID: %1").arg(pluginId);
        return false;
    }

    const QString pluginDir = m_pluginsDir + QLatin1Char('/') + pluginId;
    QDir dir(pluginDir);
    if (!dir.exists()) {
        if (err) *err = tr("Plugin directory not found: %1").arg(pluginDir);
        return false;
    }

    // Verify the resolved path is actually inside the plugins directory
    if (!isPathInside(pluginDir, m_pluginsDir)) {
        if (err) *err = tr("Refusing to remove path outside plugins directory");
        return false;
    }

    if (!dir.removeRecursively()) {
        if (err) *err = tr("Failed to remove: %1").arg(pluginDir);
        return false;
    }

    emit pluginUninstalled(pluginId);
    return true;
}

// ── Update checking ──────────────────────────────────────────────────────────

QList<MarketplaceEntry> PluginMarketplaceService::availableUpdates() const
{
    QList<MarketplaceEntry> updates;
    if (!m_pluginManager)
        return updates;

    // Build map of installed plugin versions
    QMap<QString, QString> installed;
    for (const auto &lp : m_pluginManager->loadedPlugins()) {
        const auto info = lp.instance->info();
        installed.insert(info.id, info.version);
    }

    for (const auto &entry : m_entries) {
        auto it = installed.find(entry.id);
        if (it != installed.end() && it.value() != entry.version)
            updates.append(entry);
    }

    return updates;
}

// ── Archive extraction ───────────────────────────────────────────────────────

bool PluginMarketplaceService::extractZip(
    const QString &zipPath, const QString &destDir, QString *err)
{
    QDir().mkpath(destDir);

    QProcess proc;
    proc.setWorkingDirectory(destDir);

#ifdef Q_OS_WIN
    // PowerShell Expand-Archive — use -LiteralPath to prevent wildcard
    // expansion and ensure special characters in paths are handled safely.
    proc.setProgram(QStringLiteral("powershell"));
    proc.setArguments({
        QStringLiteral("-NoProfile"),
        QStringLiteral("-Command"),
        QStringLiteral("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
            .arg(zipPath, destDir)
    });
#else
    if (zipPath.endsWith(QLatin1String(".tar.gz"))) {
        proc.setProgram(QStringLiteral("tar"));
        proc.setArguments({
            QStringLiteral("xzf"), zipPath,
            QStringLiteral("-C"), destDir,
            QStringLiteral("--no-absolute-filenames"),  // block absolute paths
        });
    } else {
        proc.setProgram(QStringLiteral("unzip"));
        proc.setArguments({QStringLiteral("-o"), zipPath,
                           QStringLiteral("-d"), destDir});
    }
#endif

    proc.start();
    if (!proc.waitForFinished(30000)) {
        if (err) *err = tr("Extraction timed out");
        return false;
    }

    if (proc.exitCode() != 0) {
        if (err) *err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        return false;
    }

    return true;
}
