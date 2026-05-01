#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

// ── SnippetEngine ────────────────────────────────────────────────────────────
//
// Stores snippet definitions (trigger + body + language) and expands snippet
// bodies that use the LSP/VS Code style placeholder syntax:
//
//   ${N:default}   — N-th tab stop with a default placeholder text
//   ${N}           — N-th tab stop, empty default
//   $N             — short form of ${N}
//   ${0} / $0      — final cursor position after snippet exit
//
// Example body:
//
//   for(int ${1:i} = 0; ${1:i} < ${2:N}; ++${1:i}) {
//       ${0}
//   }
//
// Expansion produces:
//   text         = "for(int i = 0; i < N; ++i) {\n    \n}"
//   placeholders = { 1: [(8,9),(15,16),(24,25)], 2: [(19,20)], 0: [(33,33)] }
//
// EditorView integration (TODO — out of scope here, follow-up patch):
//   1. On Tab key inside an EditorView: read the word left of the cursor
//      (re-scan via QTextCursor::WordUnderCursor or a custom regex).
//   2. Look up the trigger via SnippetEngine::find(word, currentLanguageId).
//      If non-null, remove the trigger word from the document and insert
//      ExpandResult::text at the cursor position.
//   3. Convert ExpandResult::placeholderRanges (text-relative offsets) to
//      absolute document positions by adding the insertion offset.
//   4. Maintain an active "snippet session" that highlights the current
//      placeholder ranges (ExtraSelections), select-all-occurrences of the
//      current tab-stop, and on Tab/Shift+Tab move to next/previous tab stop.
//   5. When the cursor reaches ${0} or the user presses Escape, dismiss the
//      session and restore normal editing.
//
// EditorView would own the snippet session state (current tab-stop index,
// per-stop document anchors); SnippetEngine itself is stateless aside from
// its registered snippet catalog, so it can be shared across editor
// instances and reused by language plugins.

class SnippetEngine : public QObject
{
    Q_OBJECT

public:
    struct Snippet {
        QString trigger;     // "for", "if", "while"
        QString description; // "for loop"
        QString body;        // raw body with ${N:default} / ${N} / $N markers
        QString language;    // "cpp", "py" — empty means "any language"
    };

    /// Result of expanding a snippet body. `text` has placeholders replaced
    /// by their default values (or empty strings). `placeholderRanges` maps
    /// each tab-stop index to the half-open [start,end) text offsets where
    /// that tab-stop appears (multiple occurrences are grouped). The 0-stop
    /// is the final cursor position; its range is always zero-width.
    struct ExpandResult {
        QString text;
        QHash<int, QList<QPair<int, int>>> placeholderRanges; // 1-indexed; 0 = final cursor
    };

    explicit SnippetEngine(QObject *parent = nullptr);

    /// Register a snippet. Snippets are matched by (trigger, language). A new
    /// registration with the same key overwrites the previous one.
    void registerSnippet(const Snippet &s);

    /// Look up a snippet by trigger. If `language` is non-empty, language-
    /// specific snippets win over language-agnostic ones; otherwise the first
    /// matching trigger is returned. Returns nullptr if no match exists.
    const Snippet *find(const QString &trigger, const QString &language = {}) const;

    /// All registered snippets (read-only view).
    const QList<Snippet> &snippets() const { return m_snippets; }

    /// Populate the engine with built-in C++ and Python snippets. Existing
    /// entries with the same (trigger, language) pair are overwritten.
    void loadBuiltins();

    /// Expand a snippet body to plain text plus a per-tab-stop range table.
    /// Static + pure: depends only on the body string, no engine state.
    static ExpandResult expand(const QString &body);

    /// Internal demo / smoke-test. Returns true iff the canonical for-loop
    /// snippet expands to the expected text with the expected ranges. Used
    /// by tests and by the constructor (debug builds only) to assert that
    /// the parser is sane. Safe to call from production code.
    static bool runSelfTest();

private:
    QList<Snippet> m_snippets;
};
