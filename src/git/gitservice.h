#pragma once

#include <QObject>
#include <QHash>
#include <functional>

class BridgeClient;
class QFileSystemWatcher;
class QTimer;

class GitService : public QObject
{
    Q_OBJECT

public:
    explicit GitService(QObject *parent = nullptr);

    void setWorkingDirectory(const QString &path);
    QString workingDirectory() const { return m_gitRoot; }
    QString currentBranch() const;
    QHash<QString, QString> fileStatuses() const;
    QChar statusChar(const QString &absPath) const;

    bool stageFile(const QString &absPath);
    bool unstageFile(const QString &absPath);
    bool commit(const QString &message);
    bool isGitRepo() const;
    QString diff(const QString &filePath = {}) const;

    // Per-line change info for gutter indicators
    enum class LineChange { Added, Modified, Deleted };
    struct LineChangeRange {
        int        startLine;  // 0-based
        int        lineCount;
        LineChange type;
    };
    QList<LineChangeRange> lineChanges(const QString &absFilePath) const;

    /// Find files with merge conflict markers.
    QStringList conflictFiles() const;
    /// Read the full content of a conflicted file.
    QString conflictContent(const QString &filePath) const;

    /// List all local branches.
    QStringList localBranches() const;
    /// Switch to a different branch.
    bool checkoutBranch(const QString &branchName);
    /// Create and switch to a new branch.
    bool createBranch(const QString &branchName);

    /// Per-line blame info.
    struct BlameEntry {
        QString commitHash;
        QString author;
        QString date;      // ISO-ish short date
        QString summary;   // first line of commit message
        int     line;      // 1-based
    };
    /// Run git blame on a file and return per-line entries.
    QList<BlameEntry> blame(const QString &absFilePath) const;

    /// Per-line blame info enriched for inline annotations (GitLens-style).
    struct BlameLineInfo {
        QString commitHash;       // 40-char hash; empty when uncommitted
        QString commitHashShort;  // first 8 chars
        QString author;           // author name
        qint64  authorTime = 0;   // unix epoch seconds (author-time)
        QString summary;          // first line of commit message
        int     line = 0;         // 1-based source line
        bool    uncommitted = false;
        bool    valid = false;
    };
    /// Async single-line blame for inline annotations.
    /// Result delivered via blameLineReady signal. Cached per (filePath, line)
    /// until the file is modified or HEAD changes.
    void blameLineAsync(const QString &absFilePath, int line);
    /// Query the cached blame info synchronously (fast path for paint).
    /// Returns invalid BlameLineInfo when not yet cached.
    BlameLineInfo cachedBlameLine(const QString &absFilePath, int line) const;
    /// Invalidate cached blame data for a file (call on save / on HEAD change).
    void invalidateBlameCache(const QString &absFilePath = QString());

    /// Return file contents at HEAD (empty on error/untracked).
    QString showAtHead(const QString &absFilePath) const;

    /// Return file contents at a specific revision (commit hash, branch, tag).
    QString showAtRevision(const QString &absFilePath, const QString &rev) const;

    /// Diff between two revisions (commit hashes, branches, tags).
    /// Returns unified diff output.
    QString diffRevisions(const QString &rev1, const QString &rev2) const;

    /// List files changed between two revisions.
    struct ChangedFile {
        QString path;           // relative to git root
        QChar   status;         // M, A, D, R, C, T
    };
    QList<ChangedFile> changedFilesBetween(const QString &rev1, const QString &rev2) const;

    /// Return short log entries (for commit picker).
    struct LogEntry {
        QString hash;           // abbreviated commit hash
        QString author;
        QString date;           // ISO short
        QString subject;        // first line of commit message
    };
    QList<LogEntry> log(int maxCount = 50) const;

    // ── Async variants (Manifesto #2: never block UI thread) ─────────
    /// Async blame — result delivered via blameReady signal.
    void blameAsync(const QString &absFilePath);
    /// Async diff — result delivered via diffReady signal.
    void diffAsync(const QString &filePath = {});
    /// Async showAtHead — result delivered via showAtHeadReady signal.
    void showAtHeadAsync(const QString &absFilePath);
    /// Async showAtRevision — result delivered via showAtRevisionReady signal.
    void showAtRevisionAsync(const QString &absFilePath, const QString &rev);
    /// Async diffRevisions — result delivered via diffRevisionsReady signal.
    void diffRevisionsAsync(const QString &rev1, const QString &rev2);
    /// Async changedFilesBetween — result delivered via changedFilesBetweenReady signal.
    void changedFilesBetweenAsync(const QString &rev1, const QString &rev2);
    /// Async localBranches — result delivered via localBranchesReady signal.
    void localBranchesAsync();
    /// Async log — result delivered via logReady signal.
    void logAsync(int maxCount = 50);

    /// Set a BridgeClient for centralized git file watching.
    /// When set, file watching is delegated to ExoBridge's GitWatchService
    /// instead of using a per-instance QFileSystemWatcher.
    void setBridgeClient(BridgeClient *client);

    /// Whether bridge-based file watching is active.
    bool useBridge() const { return m_bridgeClient != nullptr; }

signals:
    void statusRefreshed();
    void branchChanged(const QString &branch);
    void blameReady(const QString &filePath, const QList<BlameEntry> &entries);
    void blameLineReady(const QString &filePath, int line, const GitService::BlameLineInfo &info);
    void diffReady(const QString &filePath, const QString &diff);
    void showAtHeadReady(const QString &filePath, const QString &content);
    void showAtRevisionReady(const QString &filePath, const QString &rev, const QString &content);
    void diffRevisionsReady(const QString &rev1, const QString &rev2, const QString &diff);
    void changedFilesBetweenReady(const QString &rev1, const QString &rev2,
                                  const QList<ChangedFile> &files);
    void localBranchesReady(const QStringList &branches);
    void logReady(const QList<LogEntry> &entries);

private:
    void refresh();
    void refreshAsync();
    bool runGit(const QStringList &args, QString *out);
    void runGitAsync(const QStringList &args,
                     std::function<void(bool ok, const QString &output)> callback);
    QString toRelativePath(const QString &absPath) const;
    void resetWatcher();
    static QList<BlameEntry> parseBlameOutput(const QString &out);

    QString m_gitRoot;
    QString m_branch;
    QHash<QString, QString> m_statusCache;
    QFileSystemWatcher *m_watcher;
    QTimer *m_refreshTimer;
    bool m_isRepo;
    BridgeClient *m_bridgeClient = nullptr;

    // Inline blame cache (per file → per line → BlameLineInfo)
    mutable QHash<QString, QHash<int, BlameLineInfo>> m_blameLineCache;
};

Q_DECLARE_METATYPE(GitService::BlameLineInfo)
