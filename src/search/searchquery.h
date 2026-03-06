#pragma once

#include <QString>

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
