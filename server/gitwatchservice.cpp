#include "gitwatchservice.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>

GitWatchBridgeService::GitWatchBridgeService(QObject *parent)
    : QObject(parent)
{
}

GitWatchBridgeService::~GitWatchBridgeService()
{
    shutdown();
}

void GitWatchBridgeService::handleCall(
    const QString &method,
    const QJsonObject &args,
    std::function<void(bool ok, const QJsonObject &result)> respond)
{
    if (method == QLatin1String("watch"))
        doWatch(args, std::move(respond));
    else if (method == QLatin1String("unwatch"))
        doUnwatch(args, std::move(respond));
    else if (method == QLatin1String("list"))
        doList(std::move(respond));
    else {
        QJsonObject err;
        err[QLatin1String("message")] =
            QStringLiteral("Unknown gitwatch method: %1").arg(method);
        respond(false, err);
    }
}

void GitWatchBridgeService::shutdown()
{
    m_repos.clear();
}

// ── Method implementations ───────────────────────────────────────────────────

void GitWatchBridgeService::doWatch(
    const QJsonObject &args,
    std::function<void(bool, QJsonObject)> respond)
{
    const QString repoPath = args.value(QLatin1String("repoPath")).toString();
    if (repoPath.isEmpty()) {
        QJsonObject err;
        err[QLatin1String("message")] = QStringLiteral("Missing 'repoPath'");
        respond(false, err);
        return;
    }

    // Normalize path
    const QString canonical = QDir(repoPath).canonicalPath();
    if (canonical.isEmpty()) {
        QJsonObject err;
        err[QLatin1String("message")] =
            QStringLiteral("Invalid path: %1").arg(repoPath);
        respond(false, err);
        return;
    }

    // Already watching?
    auto it = m_repos.find(canonical);
    if (it != m_repos.end()) {
        it->second->refCount++;
        QJsonObject result;
        result[QLatin1String("repoPath")] = canonical;
        result[QLatin1String("status")]   = QStringLiteral("already_watching");
        result[QLatin1String("refCount")] = it->second->refCount;
        respond(true, result);
        return;
    }

    // Verify .git directory exists
    const QString gitDir = canonical + QStringLiteral("/.git");
    if (!QFileInfo::exists(gitDir)) {
        QJsonObject err;
        err[QLatin1String("message")] =
            QStringLiteral("Not a git repo (no .git): %1").arg(canonical);
        respond(false, err);
        return;
    }

    auto entry = std::make_unique<WatchEntry>();
    entry->repoPath = canonical;
    entry->refCount = 1;
    entry->watcher  = std::make_unique<QFileSystemWatcher>();
    entry->debounce = std::make_unique<QTimer>();
    entry->debounce->setSingleShot(true);
    entry->debounce->setInterval(200);

    setupWatcher(*entry, true);

    m_repos.emplace(canonical, std::move(entry));

    QJsonObject result;
    result[QLatin1String("repoPath")] = canonical;
    result[QLatin1String("status")]   = QStringLiteral("watching");
    respond(true, result);
}

void GitWatchBridgeService::doUnwatch(
    const QJsonObject &args,
    std::function<void(bool, QJsonObject)> respond)
{
    const QString repoPath = args.value(QLatin1String("repoPath")).toString();
    const QString canonical = QDir(repoPath).canonicalPath();

    auto it = m_repos.find(canonical);
    if (it == m_repos.end()) {
        QJsonObject err;
        err[QLatin1String("message")] =
            QStringLiteral("Not watching: %1").arg(repoPath);
        respond(false, err);
        return;
    }

    it->second->refCount--;
    if (it->second->refCount <= 0)
        m_repos.erase(it);

    QJsonObject result;
    result[QLatin1String("repoPath")] = canonical;
    result[QLatin1String("status")]   = QStringLiteral("unwatched");
    respond(true, result);
}

void GitWatchBridgeService::doList(
    std::function<void(bool, QJsonObject)> respond)
{
    QJsonArray arr;
    for (auto it = m_repos.begin(); it != m_repos.end(); ++it) {
        QJsonObject entry;
        entry[QLatin1String("repoPath")] = it->first;
        entry[QLatin1String("refCount")] = it->second->refCount;
        arr.append(entry);
    }

    QJsonObject result;
    result[QLatin1String("repos")] = arr;
    respond(true, result);
}

void GitWatchBridgeService::setupWatcher(WatchEntry &entry,
                                         bool firstTime)
{
    const QString indexPath =
        QDir(entry.repoPath).absoluteFilePath(QStringLiteral(".git/index"));
    const QString headPath =
        QDir(entry.repoPath).absoluteFilePath(QStringLiteral(".git/HEAD"));

    QStringList files;
    if (QFileInfo::exists(indexPath))
        files << indexPath;
    if (QFileInfo::exists(headPath))
        files << headPath;

    if (!files.isEmpty())
        entry.watcher->addPaths(files);

    // Only wire signal connections on first setup — re-calls just re-add
    // watched paths.  Without this guard every debounce timeout would add
    // another pair of connections, leaking memory over time.
    if (!firstTime)
        return;

    const QString repoPath = entry.repoPath;

    // On file change → debounce → emit repoChanged
    connect(entry.watcher.get(), &QFileSystemWatcher::fileChanged,
            entry.debounce.get(), [timer = entry.debounce.get()]() {
                timer->start();
            });

    connect(entry.debounce.get(), &QTimer::timeout,
            this, [this, repoPath]() {
                // Re-add paths in case the file was recreated (git operations
                // sometimes delete + recreate .git/index)
                auto it = m_repos.find(repoPath);
                if (it != m_repos.end())
                    setupWatcher(*it->second, false);

                emit repoChanged(repoPath);
            });
}
