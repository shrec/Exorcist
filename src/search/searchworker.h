#pragma once

#include <QObject>
#include <QAtomicInt>

#include "searchmatch.h"
#include "searchquery.h"
#include "searchindex.h"

class SearchWorker : public QObject
{
    Q_OBJECT

public:
    explicit SearchWorker(QObject *parent = nullptr);

    void requestCancel();

public slots:
    void run(const QString &rootPath, const SearchQuery &query);

signals:
    void matchFound(const SearchMatch &match);
    void finished();

private:
    SearchMatch buildMatch(const QString &path, int lineNumber, int column,
                           const QString &preview) const;

    QAtomicInt m_cancel;
    class SearchIndex m_index;
    QString m_indexRoot;
};
