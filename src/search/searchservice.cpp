#include "searchservice.h"

SearchService::SearchService(QObject *parent)
    : QObject(parent)
{
}

void SearchService::startSearch(const QString &, const SearchQuery &)
{
    emit searchFinished();
}

void SearchService::cancel()
{
}
