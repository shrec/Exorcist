#pragma once

#include <QSyntaxHighlighter>
#include <QString>
#include <QTextDocument>

#include <functional>

class SyntaxHighlighter;
class TreeSitterHighlighter;

/// Unified highlighter factory — prefers tree-sitter when a grammar is available,
/// falls back to the regex-based SyntaxHighlighter otherwise.
///
/// When a language lookup function is set via setLanguageLookup(), the factory
/// consults plugin-registered language declarations first (extension → language
/// ID → grammar). This enables language pack plugins to control which languages
/// are active. Falls back to the hard-coded extension map when no lookup is set
/// or when the lookup returns an empty string.
class HighlighterFactory
{
public:
    /// Callback type: given a file extension (without dot), returns a language
    /// ID (e.g. "cpp", "rust") or empty string if no plugin claims it.
    using LanguageLookupFn = std::function<QString(const QString &ext)>;

    /// Creates the best available highlighter for the given file path.
    /// The highlighter is parented to `doc` and auto-deleted with it.
    /// Returns nullptr for plain text files (no highlighting).
    static QSyntaxHighlighter *create(const QString &filePath, QTextDocument *doc);

    /// Returns true if tree-sitter support is compiled in.
    static bool hasTreeSitter();

    /// Set a language lookup function for plugin-driven language detection.
    /// When set, the factory queries this function first to map a file
    /// extension to a language ID, falling back to hard-coded extensions
    /// if the function returns an empty string.
    static void setLanguageLookup(LanguageLookupFn fn);

private:
    static LanguageLookupFn s_languageLookup;
};
