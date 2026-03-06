#include "searchworker.h"

#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#include "searchindex.h"

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
    const QStringList candidates = resolveCandidates(rootPath, query);

    QRegularExpression regex;
    const Qt::CaseSensitivity cs = query.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    if (query.mode == SearchMode::Regex) {
        QRegularExpression::PatternOptions opts;
        if (!query.caseSensitive) {
            opts |= QRegularExpression::CaseInsensitiveOption;
        }
        regex = QRegularExpression(query.pattern, opts);
        if (!regex.isValid()) {
            emit finished();
            return;
        }
    } else if (query.wholeWord) {
        const QString escaped = QRegularExpression::escape(query.pattern);
        QRegularExpression::PatternOptions opts;
        if (!query.caseSensitive) {
            opts |= QRegularExpression::CaseInsensitiveOption;
        }
        regex = QRegularExpression(QString("\\b%1\\b").arg(escaped), opts);
    }

    for (const QString &path : candidates) {
        if (m_cancel.loadAcquire()) {
            break;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        QTextStream stream(&file);
        int lineNumber = 0;
        while (!stream.atEnd() && !m_cancel.loadAcquire()) {
            const QString line = stream.readLine();
            ++lineNumber;

            if (query.mode == SearchMode::Literal && !query.wholeWord) {
                int index = line.indexOf(query.pattern, 0, cs);
                while (index >= 0) {
                    emit matchFound(buildMatch(path, lineNumber, index + 1, line.trimmed()));
                    index = line.indexOf(query.pattern, index + query.pattern.size(), cs);
                }
            } else {
                QRegularExpressionMatchIterator it = regex.globalMatch(line);
                while (it.hasNext()) {
                    const QRegularExpressionMatch match = it.next();
                    emit matchFound(buildMatch(path, lineNumber, match.capturedStart() + 1, line.trimmed()));
                }
            }
        }
    }

    emit finished();
}

QStringList SearchWorker::resolveCandidates(const QString &rootPath, const SearchQuery &query)
{
    if (m_indexRoot != rootPath || !m_index.isReady()) {
        m_index.build(rootPath);
        m_indexRoot = rootPath;
    }

    if (query.mode == SearchMode::Literal) {
        return m_index.candidateFiles(query.pattern, query.caseSensitive);
    }

    return m_index.allFiles();
}

SearchMatch SearchWorker::buildMatch(const QString &path, int lineNumber, int column, const QString &preview) const
{
    SearchMatch match;
    match.filePath = path;
    match.line = lineNumber;
    match.column = column;
    match.preview = preview;
    return match;
}
