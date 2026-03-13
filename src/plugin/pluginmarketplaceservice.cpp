#include "pluginmarketplaceservice.h"
#include "pluginmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
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

void PluginMarketplaceService::handleDownloadFinished(
    QNetworkReply *reply, const MarketplaceEntry &entry)
{
    if (reply->error() != QNetworkReply::NoError) {
        emit error(tr("Download failed for %1: %2")
                       .arg(entry.name, reply->errorString()));
        return;
    }

    // Write to a temporary file
    const QString suffix = entry.downloadUrl.endsWith(QLatin1String(".tar.gz"))
                               ? QStringLiteral(".tar.gz")
                               : QStringLiteral(".zip");

    QTemporaryFile tmpFile;
    tmpFile.setFileTemplate(QDir::tempPath() + QLatin1String("/exorcist_plugin_XXXXXX") + suffix);
    tmpFile.setAutoRemove(false);

    if (!tmpFile.open()) {
        emit error(tr("Cannot create temp file for %1").arg(entry.name));
        return;
    }

    tmpFile.write(reply->readAll());
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

    QFile::remove(tmpPath);
    emit pluginInstalled(entry.id);
}

// ── Uninstall ────────────────────────────────────────────────────────────────

bool PluginMarketplaceService::uninstallPlugin(const QString &pluginId, QString *err)
{
    const QString pluginDir = m_pluginsDir + QLatin1Char('/') + pluginId;
    QDir dir(pluginDir);
    if (!dir.exists()) {
        if (err) *err = tr("Plugin directory not found: %1").arg(pluginDir);
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
    // PowerShell Expand-Archive
    proc.setProgram(QStringLiteral("powershell"));
    proc.setArguments({
        QStringLiteral("-NoProfile"),
        QStringLiteral("-Command"),
        QStringLiteral("Expand-Archive -Path '%1' -DestinationPath '%2' -Force")
            .arg(zipPath, destDir)
    });
#else
    if (zipPath.endsWith(QLatin1String(".tar.gz"))) {
        proc.setProgram(QStringLiteral("tar"));
        proc.setArguments({QStringLiteral("xzf"), zipPath,
                           QStringLiteral("-C"), destDir});
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
