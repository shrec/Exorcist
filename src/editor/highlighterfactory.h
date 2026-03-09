#pragma once

#include <QSyntaxHighlighter>
#include <QString>
#include <QTextDocument>

class SyntaxHighlighter;
class TreeSitterHighlighter;

/// Unified highlighter factory — prefers tree-sitter when a grammar is available,
/// falls back to the regex-based SyntaxHighlighter otherwise.
class HighlighterFactory
{
public:
    /// Creates the best available highlighter for the given file path.
    /// The highlighter is parented to `doc` and auto-deleted with it.
    /// Returns nullptr for plain text files (no highlighting).
    static QSyntaxHighlighter *create(const QString &filePath, QTextDocument *doc);

    /// Returns true if tree-sitter support is compiled in.
    static bool hasTreeSitter();
};
