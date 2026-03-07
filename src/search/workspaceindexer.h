#pragma once

#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

class QFileSystemWatcher;

// A single indexed chunk — a contiguous region of a file.
struct IndexChunk {
    QString filePath;       // absolute path
    int     startLine = 0;  // 0-based inclusive
    int     endLine   = 0;  // 0-based inclusive
    QString content;        // text of this chunk
    QString symbolName;     // function/class name if detected, empty otherwise
};

// Background workspace file indexer.
// Scans all files under the workspace root, splits them into chunks at
// function/class boundaries, and maintains the index incrementally via
// QFileSystemWatcher.
class WorkspaceIndexer : public QObject
{
    Q_OBJECT

public:
    explicit WorkspaceIndexer(QObject *parent = nullptr);
    ~WorkspaceIndexer() override;

    // Start indexing a workspace root (async, runs on worker thread).
    void indexWorkspace(const QString &rootPath);

    // Cancel an in-progress scan.
    void cancel();

    // Manually reindex a single file.
    void reindexFile(const QString &filePath);

    // Search the index for chunks matching a query (simple substring).
    QVector<IndexChunk> search(const QString &query, int maxResults = 20) const;

    // Get all indexed files.
    QStringList allFiles() const;

    // Is indexing currently in progress?
    bool isIndexing() const { return m_indexing; }

    // Total files indexed / total files found.
    int indexedFileCount() const;
    int totalFileCount() const;

    QString rootPath() const { return m_rootPath; }

signals:
    void indexingStarted();
    void indexingProgress(int filesProcessed, int totalFiles);
    void indexingFinished(int totalFiles, int totalChunks);
    void fileReindexed(const QString &filePath);

private:
    void runScan();
    void processFile(const QString &filePath);
    QVector<IndexChunk> chunkFile(const QString &filePath, const QString &content) const;
    bool shouldIgnore(const QString &relativePath) const;
    void loadGitignore(const QString &rootPath);

    QString              m_rootPath;
    QStringList          m_ignorePatterns;
    QSet<QString>        m_ignoreExact;      // exact directory/file names to exclude

    mutable QMutex       m_mutex;
    QVector<IndexChunk>  m_chunks;
    QStringList          m_files;
    bool                 m_indexing  = false;
    bool                 m_cancelled = false;

    QFileSystemWatcher  *m_watcher = nullptr;
};
