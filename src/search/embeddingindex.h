#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>

#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// EmbeddingIndex — vector embedding storage and similarity search.
//
// Stores embeddings for code chunks and supports cosine-similarity
// nearest-neighbor search. Embeddings can be provided by any backend
// (local TF-IDF, remote API, etc.).
// ─────────────────────────────────────────────────────────────────────────────

struct EmbeddingEntry {
    QString filePath;
    int startLine = 0;
    int endLine = 0;
    QString symbol;
    QString snippet;
    std::vector<float> embedding;
};

struct EmbeddingSearchResult {
    EmbeddingEntry entry;
    float score = 0.0f;
};

class EmbeddingIndex : public QObject
{
    Q_OBJECT

public:
    explicit EmbeddingIndex(QObject *parent = nullptr) : QObject(parent) {}

    void addEntry(const EmbeddingEntry &entry);
    void removeFile(const QString &filePath);
    void clear() { m_entries.clear(); }
    int count() const { return m_entries.size(); }

    QList<EmbeddingSearchResult> search(const std::vector<float> &queryEmbedding,
                                         int maxResults = 10,
                                         float minScore = 0.1f) const;

    std::vector<float> computeTfIdfEmbedding(const QString &text) const;
    void buildVocabulary(const QStringList &allTexts);

    bool save(const QString &path) const;
    bool load(const QString &path);

signals:
    void vocabularyBuilt(int size);

private:
    static float cosineSimilarity(const std::vector<float> &a, const std::vector<float> &b);
    static QStringList tokenize(const QString &text);

    QList<EmbeddingEntry> m_entries;
    QStringList m_vocabulary;
    QList<float> m_idfValues;
};
