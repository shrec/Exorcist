#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// ── ICompletionContributor ────────────────────────────────────────────────────
//
// Plugins implement this to provide custom completion items outside of LSP.
// Useful for snippet-style completions, AI completions, template expansions.
//
// This is NOT a replacement for LSP completion — it supplements it.
// Items from ICompletionContributor are merged into the CompletionPopup
// alongside LSP results.

struct CompletionItem
{
    QString label;         // shown in popup
    QString insertText;    // text to insert (may contain snippets)
    QString detail;        // type info or source
    QString documentation; // Markdown docs

    enum Kind {
        Text,
        Snippet,
        Keyword,
        Variable,
        Function,
        Class,
        Module,
        File,
    };
    Kind kind = Text;

    int sortOrder = 0;     // lower = higher in list
    QString filterText;    // used for fuzzy matching (defaults to label)
};

class ICompletionContributor
{
public:
    virtual ~ICompletionContributor() = default;

    /// Provide completion items for the current context.
    /// `prefix` is the text before the cursor on the current line.
    virtual QList<CompletionItem> provideCompletions(
        const QString &filePath,
        const QString &languageId,
        int line,
        int column,
        const QString &prefix) = 0;

    /// Languages this contributor is interested in. Empty = all.
    virtual QStringList supportedLanguages() const { return {}; }
};

#define EXORCIST_COMPLETION_CONTRIBUTOR_IID "org.exorcist.ICompletionContributor/1.0"
Q_DECLARE_INTERFACE(ICompletionContributor, EXORCIST_COMPLETION_CONTRIBUTOR_IID)
