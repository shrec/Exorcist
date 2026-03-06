#pragma once

#include <QString>
#include <QMetaType>

enum class SearchMode
{
    Literal,
    Regex
};

struct SearchQuery
{
    QString pattern;
    SearchMode mode = SearchMode::Literal;
    bool caseSensitive = false;
    bool wholeWord = false;
};

Q_DECLARE_METATYPE(SearchQuery)
