#pragma once

#include <QString>
#include <QStringList>
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

    // Comma- or semicolon-separated glob patterns (e.g. "*.cpp, *.h").
    // Empty include list = match everything.
    QStringList includePatterns;
    QStringList excludePatterns;
};

Q_DECLARE_METATYPE(SearchQuery)
