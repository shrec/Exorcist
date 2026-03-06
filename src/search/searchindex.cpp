#include "searchindex.h"

#include <QDirIterator>
#include <QFile>

namespace {
constexpr qint64 kMaxIndexBytes = 5 * 1024 * 1024;
}

void SearchIndex::build(const QString &rootPath)
{
    m_rootPath = rootPath;
    m_trigramToFiles.clear();
    m_allFiles.clear();

    QDirIterator it(rootPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        m_allFiles.push_back(path);

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }

        const QByteArray bytes = file.read(kMaxIndexBytes);
        if (bytes.isEmpty()) {
            continue;
        }

        const QString text = QString::fromUtf8(bytes).toLower();
        const QStringList trigrams = extractTrigrams(text);
        for (const QString &tri : trigrams) {
            m_trigramToFiles[tri].insert(path);
        }
    }
}

QStringList SearchIndex::candidateFiles(const QString &pattern, bool caseSensitive) const
{
    if (pattern.size() < 3) {
        return m_allFiles;
    }

    QString normalized = caseSensitive ? pattern : pattern.toLower();
    const QStringList trigrams = extractTrigrams(normalized);
    if (trigrams.isEmpty()) {
        return m_allFiles;
    }

    QSet<QString> result;
    bool first = true;
    for (const QString &tri : trigrams) {
        const QSet<QString> files = m_trigramToFiles.value(tri);
        if (first) {
            result = files;
            first = false;
        } else {
            result.intersect(files);
        }

        if (result.isEmpty()) {
            break;
        }
    }

    return result.values();
}

QStringList SearchIndex::allFiles() const
{
    return m_allFiles;
}

bool SearchIndex::isReady() const
{
    return !m_rootPath.isEmpty();
}

QString SearchIndex::rootPath() const
{
    return m_rootPath;
}

QStringList SearchIndex::extractTrigrams(const QString &text)
{
    QStringList trigrams;
    if (text.size() < 3) {
        return trigrams;
    }

    trigrams.reserve(text.size() - 2);
    for (int i = 0; i + 2 < text.size(); ++i) {
        trigrams.push_back(text.mid(i, 3));
    }
    trigrams.removeDuplicates();
    return trigrams;
}
