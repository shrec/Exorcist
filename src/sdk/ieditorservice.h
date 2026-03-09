#pragma once

#include <QString>

// ── Editor Service ───────────────────────────────────────────────────────────
//
// Stable SDK interface for interacting with the text editor.
// Plugins can query the active document, modify selections, and open files.

class IEditorService
{
public:
    virtual ~IEditorService() = default;

    /// Absolute path of the currently active editor, or empty if none.
    virtual QString activeFilePath() const = 0;

    /// Language id of the active editor (e.g. "cpp", "python").
    virtual QString activeLanguageId() const = 0;

    /// Currently selected text, or empty if no selection.
    virtual QString selectedText() const = 0;

    /// Full text content of the active editor.
    virtual QString activeDocumentText() const = 0;

    /// Replace the current selection with new text.
    virtual void replaceSelection(const QString &text) = 0;

    /// Insert text at the current cursor position.
    virtual void insertText(const QString &text) = 0;

    /// Open a file in the editor. Optionally jump to line/column (1-based).
    virtual void openFile(const QString &path, int line = -1, int column = -1) = 0;

    /// Current cursor position (1-based line, 0-based column).
    virtual int cursorLine() const = 0;
    virtual int cursorColumn() const = 0;
};
