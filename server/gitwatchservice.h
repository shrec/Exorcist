#pragma once

#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QObject>
#include <QTimer>

#include <functional>
#include <map>
#include <memory>

// ── GitWatchBridgeService ────────────────────────────────────────────────────
//
// Centralized git file watching inside ExoBridge.
// One QFileSystemWatcher per git repo, shared across all IDE instances.
//
// Methods:
//   "watch"   — Start watching a git repo   { repoPath }
//   "unwatch" — Stop watching a git repo    { repoPath }
//   "list"    — List watched repos
//
// Events:
//   Broadcasts "gitChanged" with { repoPath } whenever .git/index or
//   .git/HEAD changes, debounced to avoid notification floods.

class GitWatchBridgeService : public QObject
{
    Q_OBJECT

public:
    explicit GitWatchBridgeService(QObject *parent = nullptr);
    ~GitWatchBridgeService() override;

    void handleCall(const QString &method,
                    const QJsonObject &args,
                    std::function<void(bool ok, const QJsonObject &result)> respond);

    void shutdown();

signals:
    /// Emitted when a watched repo changes — ExoBridgeCore hooks this
    /// to broadcast to all IDE clients.
    void repoChanged(const QString &repoPath);

private:
    struct WatchEntry {
        QString repoPath;
        std::unique_ptr<QFileSystemWatcher> watcher;
        std::unique_ptr<QTimer> debounce;
        int refCount = 0;  // how many clients watching this repo
    };

    void doWatch(const QJsonObject &args,
                 std::function<void(bool, QJsonObject)> respond);
    void doUnwatch(const QJsonObject &args,
                   std::function<void(bool, QJsonObject)> respond);
    void doList(std::function<void(bool, QJsonObject)> respond);
    void setupWatcher(WatchEntry &entry);

    std::map<QString, std::unique_ptr<WatchEntry>> m_repos;
};
