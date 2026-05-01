#include "pluginmarketplaceservice.h"
#include "pluginmanager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QStandardPaths>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QCryptographicHash>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryFile>
#include <QUrl>
#include <QUuid>

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

    // ── SHA-256 integrity verification (mandatory) ─────────────────────────
    if (entry.sha256.isEmpty()) {
        emit error(tr("Refusing to install %1: package has no SHA-256 hash. "
                      "All marketplace packages must include a sha256 field.")
                       .arg(entry.name));
        return;
    }
    {
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

// ── installFromUrl() ─────────────────────────────────────────────────────────

namespace {

#ifdef Q_OS_WIN
constexpr unsigned long kCreateNoWindow = 0x08000000;

void suppressWindowsConsole(QProcess *proc)
{
    proc->setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments *a) {
            if (a) a->flags |= kCreateNoWindow;
        });
}
#else
void suppressWindowsConsole(QProcess *) {}
#endif

bool looksLikeZipUrl(const QString &url)
{
    const QString lower = url.toLower();
    if (lower.endsWith(QLatin1String(".zip")))
        return true;
    if (lower.contains(QLatin1String("/archive/")) && lower.endsWith(QLatin1String(".zip")))
        return true;
    return false;
}

bool looksLikeGitUrl(const QString &url)
{
    const QString lower = url.toLower();
    if (lower.endsWith(QLatin1String(".git")))
        return true;
    if (lower.startsWith(QLatin1String("git@")) ||
        lower.startsWith(QLatin1String("git://"))) {
        return true;
    }
    if (lower.contains(QLatin1String("github.com/")) ||
        lower.contains(QLatin1String("gitlab.com/")) ||
        lower.contains(QLatin1String("bitbucket.org/")) ||
        lower.contains(QLatin1String("codeberg.org/"))) {
        // GitHub/GitLab tree URLs without .git suffix — git clone still works.
        return !looksLikeZipUrl(url);
    }
    return false;
}

QString slugFromUrl(const QString &url)
{
    QString slug = QFileInfo(QUrl(url).path()).baseName();
    slug.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")), QStringLiteral("_"));
    if (slug.isEmpty())
        slug = QStringLiteral("plugin");
    return slug;
}

bool copyDirRecursive(const QString &srcDir, const QString &dstDir, QString *err)
{
    QDir src(srcDir);
    if (!src.exists()) {
        if (err) *err = QStringLiteral("source directory does not exist: %1").arg(srcDir);
        return false;
    }
    QDir().mkpath(dstDir);
    const auto entries = src.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &fi : entries) {
        const QString srcPath = fi.absoluteFilePath();
        const QString dstPath = dstDir + QLatin1Char('/') + fi.fileName();
        if (fi.isDir()) {
            if (!copyDirRecursive(srcPath, dstPath, err))
                return false;
        } else {
            if (QFile::exists(dstPath))
                QFile::remove(dstPath);
            if (!QFile::copy(srcPath, dstPath)) {
                if (err) *err = QStringLiteral("failed to copy %1 → %2")
                                    .arg(srcPath, dstPath);
                return false;
            }
        }
    }
    return true;
}

} // namespace

void PluginMarketplaceService::installFromUrl(const QString &url,
                                              const QString &subdir,
                                              bool buildAfter,
                                              bool activateAfter)
{
    if (m_stage != InstallStage::Idle) {
        emit installLog(tr("[error] Another install is already running."));
        return;
    }

    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty()) {
        finishInstall(false, QString(), tr("Source URL is empty."));
        return;
    }

    const QUrl parsed(trimmed, QUrl::StrictMode);
    if (!parsed.isValid() || parsed.scheme().isEmpty()) {
        finishInstall(false, QString(),
                      tr("Malformed URL: %1").arg(trimmed));
        return;
    }
    const QString scheme = parsed.scheme().toLower();
    if (scheme != QLatin1String("http") &&
        scheme != QLatin1String("https") &&
        scheme != QLatin1String("git") &&
        scheme != QLatin1String("ssh")) {
        finishInstall(false, QString(),
                      tr("Unsupported URL scheme: %1").arg(scheme));
        return;
    }

    m_inputUrl       = trimmed;
    m_subdir         = subdir.trimmed();
    m_buildAfter     = buildAfter;
    m_activateAfter  = activateAfter;
    m_cancelled      = false;
    m_finalPluginRoot.clear();

    // Reserve a unique cache directory under plugins/.cache/<token>/.
    QDir().mkpath(m_pluginsDir);
    const QString cacheBase = m_pluginsDir + QLatin1String("/.cache");
    QDir().mkpath(cacheBase);
    const QString token = QStringLiteral("%1-%2")
        .arg(slugFromUrl(trimmed),
             QString::number(QDateTime::currentMSecsSinceEpoch()));
    m_cacheRoot = cacheBase + QLatin1Char('/') + token;
    QDir().mkpath(m_cacheRoot);

    if (looksLikeZipUrl(trimmed)) {
        startZipDownload(trimmed);
    } else if (looksLikeGitUrl(trimmed)) {
        startGitClone(trimmed);
    } else {
        // Default to git clone for ambiguous https URLs (cheap fallback).
        startGitClone(trimmed);
    }
}

void PluginMarketplaceService::cancelInstall()
{
    if (m_stage == InstallStage::Idle)
        return;
    m_cancelled = true;
    if (m_proc) {
        m_proc->kill();
    }
    if (m_zipReply) {
        m_zipReply->abort();
    }
    finishInstall(false, QString(), tr("Cancelled by user."));
}

void PluginMarketplaceService::startGitClone(const QString &url)
{
    m_stage = InstallStage::Cloning;
    emit installProgress(5, tr("Cloning %1…").arg(url));
    emit installLog(tr("[git] clone %1").arg(url));

    if (m_proc) {
        m_proc->deleteLater();
        m_proc = nullptr;
    }

    auto *proc = new QProcess(this);
    m_proc = proc;
    proc->setProgram(QStringLiteral("git"));
    proc->setArguments({
        QStringLiteral("clone"),
        QStringLiteral("--progress"),
        QStringLiteral("--depth=1"),
        url,
        m_cacheRoot,
    });
    proc->setProcessChannelMode(QProcess::MergedChannels);
    suppressWindowsConsole(proc);

    QPointer<PluginMarketplaceService> self(this);
    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
        const QByteArray bytes = proc->readAllStandardOutput();
        const QString text = QString::fromLocal8Bit(bytes);
        const auto lines = text.split(QChar(QLatin1Char('\n')), Qt::SkipEmptyParts);
        for (const QString &line : lines)
            emit installLog(QStringLiteral("[git] ") + line.trimmed());
    });
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [self, this](int exitCode, QProcess::ExitStatus status) {
        if (!self) return;
        if (m_cancelled) return;
        if (status != QProcess::NormalExit || exitCode != 0) {
            finishInstall(false, QString(),
                          tr("git clone failed (exit %1)").arg(exitCode));
            return;
        }
        emit installProgress(40, tr("Clone complete."));
        afterFetch(m_cacheRoot);
    });
    connect(proc, &QProcess::errorOccurred, this,
            [self, this](QProcess::ProcessError err) {
        if (!self) return;
        if (m_cancelled) return;
        finishInstall(false, QString(),
                      tr("git failed to start (error %1). Is git installed and on PATH?")
                          .arg(int(err)));
    });
    proc->start();
}

void PluginMarketplaceService::startZipDownload(const QString &url)
{
    m_stage = InstallStage::Downloading;
    emit installProgress(5, tr("Downloading %1…").arg(url));
    emit installLog(tr("[zip] GET %1").arg(url));

    QNetworkRequest req;
    req.setUrl(QUrl(url));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("ExorcistIDE/1.0"));
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    auto *reply = m_network->get(req);
    m_zipReply = reply;

    QPointer<PluginMarketplaceService> self(this);
    connect(reply, &QNetworkReply::downloadProgress,
            this, [this](qint64 received, qint64 total) {
        if (total > 0) {
            const int pct = 5 + int(received * 35 / total);  // 5..40
            emit installProgress(pct,
                tr("Downloading… %1 / %2 KB")
                    .arg(received / 1024).arg(total / 1024));
        }
    });
    connect(reply, &QNetworkReply::finished, this, [self, this, reply]() {
        if (!self) return;
        m_zipReply = nullptr;
        reply->deleteLater();
        if (m_cancelled) return;

        if (reply->error() != QNetworkReply::NoError) {
            finishInstall(false, QString(),
                          tr("Download failed: %1").arg(reply->errorString()));
            return;
        }

        // Write to a temp .zip in the cache dir.
        const QString zipPath = m_cacheRoot + QLatin1String("/_download.zip");
        QFile out(zipPath);
        if (!out.open(QIODevice::WriteOnly)) {
            finishInstall(false, QString(),
                          tr("Cannot create temp file: %1").arg(zipPath));
            return;
        }
        out.write(reply->readAll());
        out.close();

        emit installProgress(45, tr("Extracting…"));
        emit installLog(tr("[zip] extract %1").arg(zipPath));

        m_stage = InstallStage::Extracting;
        const QString extractDir = m_cacheRoot + QLatin1String("/_extracted");
        QDir().mkpath(extractDir);

        QString err;
        if (!extractZip(zipPath, extractDir, &err)) {
            finishInstall(false, QString(),
                          tr("Extraction failed: %1").arg(err));
            return;
        }
        QFile::remove(zipPath);

        // GitHub-style ZIPs always have a single top-level directory like
        // "myrepo-main/". Detect and unwrap it so afterFetch() sees the
        // actual repo root.
        QDir ed(extractDir);
        const auto entries = ed.entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot);
        QString fetchedRoot = extractDir;
        if (entries.size() == 1 && entries.first().isDir())
            fetchedRoot = entries.first().absoluteFilePath();

        emit installProgress(50, tr("Extracted."));
        afterFetch(fetchedRoot);
    });
}

void PluginMarketplaceService::afterFetch(const QString &fetchedRoot)
{
    m_stage = InstallStage::Reading;
    emit installProgress(55, tr("Reading plugin manifest…"));

    QString pluginRoot = fetchedRoot;
    if (!m_subdir.isEmpty()) {
        pluginRoot = fetchedRoot + QLatin1Char('/') + m_subdir;
        if (!QFileInfo(pluginRoot).isDir()) {
            finishInstall(false, QString(),
                          tr("Subdirectory not found: %1").arg(m_subdir));
            return;
        }
    }

    QString name, version, err;
    const QString pluginId = readManifestId(pluginRoot, &name, &version, &err);
    if (pluginId.isEmpty()) {
        finishInstall(false, QString(),
                      tr("Cannot read plugin.json: %1").arg(err));
        return;
    }
    emit installLog(tr("[manifest] id=%1 name=%2 version=%3")
                        .arg(pluginId, name, version));

    // Move/copy to plugins/<id>/.
    m_stage = InstallStage::Moving;
    emit installProgress(60, tr("Installing to plugins/%1/…").arg(pluginId));

    const QString destDir = m_pluginsDir + QLatin1Char('/') + pluginId;
    if (QDir(destDir).exists()) {
        emit installLog(tr("[install] removing existing plugins/%1/").arg(pluginId));
        QDir(destDir).removeRecursively();
    }

    QString copyErr;
    if (!copyDirRecursive(pluginRoot, destDir, &copyErr)) {
        finishInstall(false, QString(),
                      tr("Install copy failed: %1").arg(copyErr));
        return;
    }
    m_finalPluginRoot = destDir;

    if (m_buildAfter) {
        startBuild(destDir);
    } else {
        finalizeAndActivate(destDir, pluginId);
    }
}

void PluginMarketplaceService::startBuild(const QString &pluginRoot)
{
    m_stage = InstallStage::Building;
    emit installProgress(70, tr("Building plugin…"));
    emit installLog(tr("[build] cmake -B build && cmake --build build (in %1)")
                        .arg(pluginRoot));

    if (m_proc) {
        m_proc->deleteLater();
        m_proc = nullptr;
    }

    auto *proc = new QProcess(this);
    m_proc = proc;
    proc->setWorkingDirectory(pluginRoot);
    proc->setProgram(QStringLiteral("cmake"));
    proc->setArguments({QStringLiteral("-B"), QStringLiteral("build")});
    proc->setProcessChannelMode(QProcess::MergedChannels);
    suppressWindowsConsole(proc);

    QPointer<PluginMarketplaceService> self(this);
    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
        const QString text = QString::fromLocal8Bit(proc->readAllStandardOutput());
        const auto lines = text.split(QChar(QLatin1Char('\n')), Qt::SkipEmptyParts);
        for (const QString &line : lines)
            emit installLog(QStringLiteral("[cmake] ") + line.trimmed());
    });
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [self, this, pluginRoot](int code, QProcess::ExitStatus st) {
        if (!self) return;
        if (m_cancelled) return;
        if (st != QProcess::NormalExit || code != 0) {
            finishInstall(false, QString(),
                          tr("cmake configure failed (exit %1)").arg(code));
            return;
        }

        // Phase 2: build.
        auto *build = new QProcess(this);
        m_proc = build;
        build->setWorkingDirectory(pluginRoot);
        build->setProgram(QStringLiteral("cmake"));
        build->setArguments({QStringLiteral("--build"), QStringLiteral("build")});
        build->setProcessChannelMode(QProcess::MergedChannels);
        suppressWindowsConsole(build);

        QPointer<PluginMarketplaceService> self2(this);
        connect(build, &QProcess::readyReadStandardOutput, this, [this, build]() {
            const QString text = QString::fromLocal8Bit(build->readAllStandardOutput());
            const auto lines = text.split(QChar(QLatin1Char('\n')), Qt::SkipEmptyParts);
            for (const QString &line : lines)
                emit installLog(QStringLiteral("[build] ") + line.trimmed());
        });
        connect(build, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [self2, this, pluginRoot](int c, QProcess::ExitStatus s) {
            if (!self2) return;
            if (m_cancelled) return;
            if (s != QProcess::NormalExit || c != 0) {
                finishInstall(false, QString(),
                              tr("Build failed (exit %1).").arg(c));
                return;
            }
            emit installProgress(90, tr("Build complete."));
            // Re-read manifest id (in case the build wrote a generated one).
            QString name, version, err;
            const QString id = readManifestId(pluginRoot, &name, &version, &err);
            finalizeAndActivate(pluginRoot, id);
        });
        build->start();
    });
    connect(proc, &QProcess::errorOccurred, this,
            [self, this](QProcess::ProcessError err) {
        if (!self) return;
        if (m_cancelled) return;
        finishInstall(false, QString(),
                      tr("cmake failed to start (error %1). Is cmake installed and on PATH?")
                          .arg(int(err)));
    });
    proc->start();
}

void PluginMarketplaceService::finalizeAndActivate(const QString &pluginRoot,
                                                   const QString &pluginId)
{
    if (m_activateAfter && m_pluginManager) {
        m_stage = InstallStage::Activating;
        emit installProgress(95, tr("Activating plugin…"));
        emit installLog(tr("[activate] loadPluginsFrom(%1)").arg(pluginRoot));

        const int loaded = m_pluginManager->loadPluginsFrom(pluginRoot);
        if (loaded > 0) {
            emit installLog(tr("[activate] %1 plugin(s) loaded").arg(loaded));
        } else {
            // Not necessarily a hard error — the plugin might be a script
            // plugin or already loaded from a different path.
            emit installLog(tr("[activate] no native plugins loaded "
                               "(may be Lua/manifest-only or already active)"));
        }
    }

    emit pluginInstalled(pluginId);
    finishInstall(true, pluginId, QString());
}

void PluginMarketplaceService::finishInstall(bool success,
                                             const QString &pluginId,
                                             const QString &errorMessage)
{
    if (m_proc) {
        m_proc->deleteLater();
        m_proc = nullptr;
    }
    m_zipReply = nullptr;
    m_stage = InstallStage::Idle;

    // Best-effort cache cleanup. Keep on failure so the user can inspect.
    if (success) {
        QDir(m_cacheRoot).removeRecursively();
    }

    if (success) {
        emit installProgress(100, tr("Installed %1").arg(pluginId));
    } else {
        emit installProgress(0, errorMessage);
    }
    emit installFinished(success, pluginId, errorMessage);
}

QString PluginMarketplaceService::readManifestId(const QString &pluginRoot,
                                                 QString *name,
                                                 QString *version,
                                                 QString *err)
{
    const QString manifestPath = pluginRoot + QLatin1String("/plugin.json");
    QFile f(manifestPath);
    if (!f.exists()) {
        if (err) *err = tr("plugin.json not found at %1").arg(manifestPath);
        return QString();
    }
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = tr("cannot open %1").arg(manifestPath);
        return QString();
    }
    QJsonParseError pe;
    const auto doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        if (err) *err = tr("invalid JSON: %1").arg(pe.errorString());
        return QString();
    }
    const auto obj = doc.object();
    const QString id = obj.value(QLatin1String("id")).toString();
    if (id.isEmpty() || !isValidPluginId(id)) {
        if (err) *err = tr("plugin.json has missing or invalid \"id\"");
        return QString();
    }
    if (name)    *name    = obj.value(QLatin1String("name")).toString();
    if (version) *version = obj.value(QLatin1String("version")).toString();
    return id;
}
