#include "searchworker.h"

#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>

#include "searchindex.h"

namespace {
constexpr int kMaxResults = 5000;       // Stop after this many matches
constexpr qint64 kMaxFileSize = 1024 * 1024;  // Skip files > 1 MB
}

SearchWorker::SearchWorker(QObject *parent)
    : QObject(parent),
      m_cancel(false)
{
}

void SearchWorker::requestCancel()
{
    m_cancel.storeRelease(1);
}

void SearchWorker::run(const QString &rootPath, const SearchQuery &query)
{
    m_cancel.storeRelease(0);

    // Rebuild file index if root changed
    if (m_indexRoot != rootPath || !m_index.isReady()) {
        m_index.build(rootPath);
        m_indexRoot = rootPath;
    }

    const QStringList candidates = m_index.allFiles();

    QRegularExpression regex;
    const Qt::CaseSensitivity cs = query.caseSensitive
                                       ? Qt::CaseSensitive
                                       : Qt::CaseInsensitive;

    if (query.mode == SearchMode::Regex) {
        QRegularExpression::PatternOptions opts;
        if (!query.caseSensitive)
            opts |= QRegularExpression::CaseInsensitiveOption;
        regex = QRegularExpression(query.pattern, opts);
        if (!regex.isValid()) {
            emit finished();
            return;
        }
    } else if (query.wholeWord) {
        const QString escaped = QRegularExpression::escape(query.pattern);
        QRegularExpression::PatternOptions opts;
        if (!query.caseSensitive)
            opts |= QRegularExpression::CaseInsensitiveOption;
        regex = QRegularExpression(QStringLiteral("\\b%1\\b").arg(escaped), opts);
    }

    int totalMatches = 0;

    for (const QString &path : candidates) {
        if (m_cancel.loadAcquire() || totalMatches >= kMaxResults)
            break;

        QFile file(path);
        if (file.size() > kMaxFileSize)
            continue;
        if (!file.open(QIODevice::ReadOnly))
            continue;

        // Read file as raw bytes, scan line by line without full QString copy
        const QByteArray data = file.readAll();
        file.close();

        int lineNumber = 0;
        int pos = 0;
        const int dataSize = data.size();

        while (pos < dataSize && !m_cancel.loadAcquire()
               && totalMatches < kMaxResults) {
            // Find end of line
            int eol = data.indexOf('\n', pos);
            if (eol < 0) eol = dataSize;

            const int lineLen = eol - pos;
            // Skip very long lines (minified files, etc.)
            if (lineLen > 4096) {
                pos = eol + 1;
                ++lineNumber;
                continue;
            }

            const QString line = QString::fromUtf8(data.constData() + pos, lineLen);
            ++lineNumber;
            pos = eol + 1;

            if (query.mode == SearchMode::Literal && !query.wholeWord) {
                int idx = line.indexOf(query.pattern, 0, cs);
                if (idx >= 0) {
                    emit matchFound(buildMatch(path, lineNumber, idx + 1,
                                               line.trimmed()));
                    ++totalMatches;
                }
            } else {
                const QRegularExpressionMatch m = regex.match(line);
                if (m.hasMatch()) {
                    emit matchFound(buildMatch(path, lineNumber,
                                               m.capturedStart() + 1,
                                               line.trimmed()));
                    ++totalMatches;
                }
            }
        }
    }

    emit finished();
}

SearchMatch SearchWorker::buildMatch(const QString &path, int lineNumber,
                                     int column, const QString &preview) const
{
    SearchMatch match;
    match.filePath = path;
    match.line = lineNumber;
    match.column = column;
    // Truncate long preview lines
    match.preview = preview.length() > 200 ? preview.left(200) + "..." : preview;
    return match;
}
