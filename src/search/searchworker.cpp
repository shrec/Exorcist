#include "searchworker.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include "searchindex.h"

namespace {
constexpr int kMaxResults = 5000;       // Stop after this many matches
constexpr qint64 kMaxFileSize = 1024 * 1024;  // Skip files > 1 MB

// Convert a single glob (e.g. "*.cpp" or "build/") to a QRegularExpression.
// Patterns containing '/' or '\' are matched against the relative file path;
// pattern is treated as a directory when ending in '/'. Patterns without
// separators are matched against the file name only.
QRegularExpression globToRegex(const QString &globIn)
{
    QString glob = globIn.trimmed();
    if (glob.isEmpty())
        return QRegularExpression();

    // Trailing slash → match the directory and anything below it.
    bool dirOnly = false;
    if (glob.endsWith('/') || glob.endsWith('\\')) {
        dirOnly = true;
        glob.chop(1);
    }

    // Use Qt's wildcard-to-regex helper, anchored case-insensitively.
    QString rx = QRegularExpression::wildcardToRegularExpression(
        glob, QRegularExpression::UnanchoredWildcardConversion);

    // For directory-only globs, require it to appear as a path segment.
    if (dirOnly)
        rx = QStringLiteral("(^|/)") + rx + QStringLiteral("(/|$)");

    return QRegularExpression(rx, QRegularExpression::CaseInsensitiveOption);
}

bool pathMatchesAny(const QString &relPath, const QString &fileName,
                    const QVector<QRegularExpression> &patterns,
                    const QVector<bool> &isPathPattern)
{
    for (int i = 0; i < patterns.size(); ++i) {
        if (!patterns[i].isValid())
            continue;
        const QString &target = isPathPattern[i] ? relPath : fileName;
        if (patterns[i].match(target).hasMatch())
            return true;
    }
    return false;
}

void compilePatterns(const QStringList &globs,
                     QVector<QRegularExpression> *out,
                     QVector<bool> *isPathPattern)
{
    out->clear();
    isPathPattern->clear();
    for (const QString &g : globs) {
        const QString trimmed = g.trimmed();
        if (trimmed.isEmpty())
            continue;
        out->append(globToRegex(trimmed));
        isPathPattern->append(trimmed.contains('/') || trimmed.contains('\\'));
    }
}
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

    // Compile include / exclude glob filters once.
    QVector<QRegularExpression> includeRx, excludeRx;
    QVector<bool> includeIsPath, excludeIsPath;
    compilePatterns(query.includePatterns, &includeRx, &includeIsPath);
    compilePatterns(query.excludePatterns, &excludeRx, &excludeIsPath);
    const QDir rootDir(rootPath);

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

        // Apply include / exclude glob filters.
        const QString relPath = rootDir.relativeFilePath(path);
        const QString fileName = QFileInfo(path).fileName();
        if (!includeRx.isEmpty()
            && !pathMatchesAny(relPath, fileName, includeRx, includeIsPath)) {
            continue;
        }
        if (!excludeRx.isEmpty()
            && pathMatchesAny(relPath, fileName, excludeRx, excludeIsPath)) {
            continue;
        }

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
