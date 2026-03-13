#include "embeddingindex.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <cstring>

void EmbeddingIndex::addEntry(const EmbeddingEntry &entry)
{
    m_entries.append(entry);
}

void EmbeddingIndex::removeFile(const QString &filePath)
{
    m_entries.erase(
        std::remove_if(m_entries.begin(), m_entries.end(),
            [&](const EmbeddingEntry &e) { return e.filePath == filePath; }),
        m_entries.end());
}

QList<EmbeddingSearchResult> EmbeddingIndex::search(const std::vector<float> &queryEmbedding,
                                                     int maxResults,
                                                     float minScore) const
{
    QList<EmbeddingSearchResult> results;
    for (const auto &entry : m_entries) {
        const float score = cosineSimilarity(queryEmbedding, entry.embedding);
        if (score >= minScore)
            results.append({entry, score});
    }

    std::sort(results.begin(), results.end(),
              [](const EmbeddingSearchResult &a, const EmbeddingSearchResult &b) {
                  return a.score > b.score;
              });

    if (results.size() > maxResults)
        results = results.mid(0, maxResults);

    return results;
}

std::vector<float> EmbeddingIndex::computeTfIdfEmbedding(const QString &text) const
{
    const QStringList tokens = tokenize(text);
    if (tokens.isEmpty()) return {};

    QMap<QString, int> tf;
    for (const auto &tok : tokens) tf[tok]++;

    std::vector<float> vec(m_vocabulary.size(), 0.0f);
    for (auto it = tf.begin(); it != tf.end(); ++it) {
        const int idx = m_vocabulary.indexOf(it.key());
        if (idx >= 0) {
            const float termFreq = static_cast<float>(it.value()) / tokens.size();
            const float idf = m_idfValues.value(idx, 1.0f);
            vec[idx] = termFreq * idf;
        }
    }
    return vec;
}

void EmbeddingIndex::buildVocabulary(const QStringList &allTexts)
{
    QMap<QString, int> docFreq;
    const int docCount = allTexts.size();

    for (const auto &text : allTexts) {
        const QSet<QString> unique(tokenize(text).begin(), tokenize(text).end());
        for (const auto &tok : unique)
            docFreq[tok]++;
    }

    QList<QPair<QString, int>> sorted;
    for (auto it = docFreq.begin(); it != docFreq.end(); ++it)
        sorted.append({it.key(), it.value()});
    std::sort(sorted.begin(), sorted.end(),
              [](const QPair<QString, int> &a, const QPair<QString, int> &b) {
                  return a.second > b.second;
              });

    const int maxVocab = qMin(2048, sorted.size());
    m_vocabulary.clear();
    m_idfValues.clear();
    m_vocabulary.reserve(maxVocab);
    m_idfValues.reserve(maxVocab);

    for (int i = 0; i < maxVocab; ++i) {
        m_vocabulary.append(sorted[i].first);
        m_idfValues.append(std::log(static_cast<float>(docCount) /
                                    (1.0f + sorted[i].second)));
    }

    emit vocabularyBuilt(m_vocabulary.size());
}

bool EmbeddingIndex::save(const QString &path) const
{
    QJsonObject root;

    QJsonArray vocab;
    for (const auto &v : m_vocabulary) vocab.append(v);
    root[QStringLiteral("vocabulary")] = vocab;

    QJsonArray idfArr;
    for (float v : m_idfValues) idfArr.append(static_cast<double>(v));
    root[QStringLiteral("idf")] = idfArr;

    QJsonArray entries;
    for (const auto &e : m_entries) {
        QJsonObject obj;
        obj[QStringLiteral("file")] = e.filePath;
        obj[QStringLiteral("start")] = e.startLine;
        obj[QStringLiteral("end")] = e.endLine;
        obj[QStringLiteral("symbol")] = e.symbol;
        QByteArray raw(reinterpret_cast<const char *>(e.embedding.data()),
                       static_cast<int>(e.embedding.size() * sizeof(float)));
        obj[QStringLiteral("emb")] = QString::fromLatin1(raw.toBase64());
        entries.append(obj);
    }
    root[QStringLiteral("entries")] = entries;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return true;
}

bool EmbeddingIndex::load(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();

    m_vocabulary.clear();
    const QJsonArray vocab = root.value(QStringLiteral("vocabulary")).toArray();
    for (const auto &v : vocab) m_vocabulary.append(v.toString());

    m_idfValues.clear();
    const QJsonArray idfArr = root.value(QStringLiteral("idf")).toArray();
    for (const auto &v : idfArr) m_idfValues.append(static_cast<float>(v.toDouble()));

    m_entries.clear();
    const QJsonArray entries = root.value(QStringLiteral("entries")).toArray();
    for (const auto &item : entries) {
        const QJsonObject obj = item.toObject();
        EmbeddingEntry e;
        e.filePath = obj.value(QStringLiteral("file")).toString();
        e.startLine = obj.value(QStringLiteral("start")).toInt();
        e.endLine = obj.value(QStringLiteral("end")).toInt();
        e.symbol = obj.value(QStringLiteral("symbol")).toString();
        const QByteArray raw = QByteArray::fromBase64(
            obj.value(QStringLiteral("emb")).toString().toLatin1());
        e.embedding.resize(raw.size() / sizeof(float));
        std::memcpy(e.embedding.data(), raw.constData(), raw.size());
        m_entries.append(e);
    }
    return true;
}

float EmbeddingIndex::cosineSimilarity(const std::vector<float> &a, const std::vector<float> &b)
{
    if (a.size() != b.size() || a.empty()) return 0.0f;
    float dot = 0.0f, normA = 0.0f, normB = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        normA += a[i] * a[i];
        normB += b[i] * b[i];
    }
    const float denom = std::sqrt(normA) * std::sqrt(normB);
    return (denom > 0.0f) ? (dot / denom) : 0.0f;
}

QStringList EmbeddingIndex::tokenize(const QString &text)
{
    static const QRegularExpression rx(QStringLiteral(R"([^a-zA-Z0-9_]+)"));
    QStringList tokens = text.toLower().split(rx, Qt::SkipEmptyParts);
    QStringList expanded;
    static const QRegularExpression camelRx(QStringLiteral(R"((?<=[a-z])(?=[A-Z]))"));
    for (const auto &tok : tokens) {
        const QStringList parts = tok.split(camelRx, Qt::SkipEmptyParts);
        for (const auto &p : parts)
            if (p.size() >= 2) expanded << p;
    }
    return expanded;
}
