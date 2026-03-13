#pragma once

#include <QDir>
#include <QObject>
#include <QString>
#include <QStringList>

// ─────────────────────────────────────────────────────────────────────────────
// WorkspaceChunkIndex — local code chunk index for semantic search.
//
// Chunks workspace files at language-aware boundaries, stores them
// with metadata, and supports keyword + partial matching search.
// Persists index to disk for fast reload.
// ─────────────────────────────────────────────────────────────────────────────

struct CodeChunk {
    QString filePath;
    int startLine = 0;
    int endLine = 0;
    QString content;
    QString symbol;
    QString language;
    QByteArray contentHash;
};

class WorkspaceChunkIndex : public QObject
{
    Q_OBJECT

public:
    explicit WorkspaceChunkIndex(QObject *parent = nullptr) : QObject(parent) {}

    void buildIndex(const QString &root);
    void reindexFile(const QString &filePath);
    void removeFile(const QString &filePath);

    QList<CodeChunk> search(const QString &query, int maxResults = 20) const;
    QList<CodeChunk> searchSymbol(const QString &name) const;

    bool saveIndex(const QString &indexPath) const;
    bool loadIndex(const QString &indexPath, const QString &root);

    int chunkCount() const { return m_chunks.size(); }
    QList<CodeChunk> allChunks() const { return m_chunks; }

    void setIgnorePatterns(const QStringList &patterns) { m_ignorePatterns = patterns; }

signals:
    void indexingStarted();
    void indexProgress(int processed, int total);
    void indexingFinished(int chunkCount);

private:
    void collectFiles(const QDir &dir, QStringList &files) const;
    void indexFile(const QString &filePath);
    static QString extractSymbol(const QString &code);
    static QString languageFromExt(const QString &ext);

    QString m_root;
    QList<CodeChunk> m_chunks;
    QStringList m_ignorePatterns;
};
