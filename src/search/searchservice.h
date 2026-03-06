#pragma once

#include <QObject>
#include <QVector>

#include "searchmatch.h"
#include "searchquery.h"

class SearchService : public QObject
{
    Q_OBJECT

public:
    explicit SearchService(QObject *parent = nullptr);

    void startSearch(const QString &rootPath, const SearchQuery &query);
    void cancel();

signals:
    void matchFound(const SearchMatch &match);
    void searchFinished();
};
