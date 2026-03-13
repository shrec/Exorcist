#pragma once

#include <QTextCursor>
#include <QTextDocument>

#include <vector>

/// Pure-logic multi-cursor engine — manages multiple QTextCursors on a single
/// QTextDocument. All text operations are applied across every cursor in reverse
/// document order (bottom-to-top) so that earlier cursor positions stay valid.
///
/// The engine does NOT own the QTextDocument — it operates on a borrowed pointer.
/// Cursors automatically track document edits because Qt wires them internally.
class MultiCursorEngine
{
public:
    explicit MultiCursorEngine(QTextDocument *doc = nullptr);

    /// Set the document to operate on. Clears all cursors.
    void setDocument(QTextDocument *doc);
    QTextDocument *document() const { return m_doc; }

    // ── Cursor management ─────────────────────────────────────────────────

    /// Number of active cursors (0 if document is null).
    int cursorCount() const;

    /// True when more than one cursor exists.
    bool hasMultipleCursors() const;

    /// Return all cursors, sorted by position (ascending).
    std::vector<QTextCursor> cursors() const;

    /// Add a cursor at the given position. Merges if it overlaps an existing one.
    void addCursor(int position);

    /// Add a cursor with a selection range [anchor, position).
    void addCursorWithSelection(int anchor, int position);

    /// Remove all secondary cursors, leaving only the primary (lowest position).
    void clearSecondaryCursors();

    /// Remove all cursors entirely.
    void clearAll();

    /// Set the primary cursor (index 0). If the engine is empty, adds it.
    void setPrimaryCursor(const QTextCursor &cursor);

    /// Get the primary cursor (lowest-position cursor). Invalid if empty.
    QTextCursor primaryCursor() const;

    // ── Text operations (applied to ALL cursors) ──────────────────────────

    /// Insert text at every cursor position. Selections are replaced.
    void insertText(const QString &text);

    /// Delete selected text at every cursor (or the char before cursor if no
    /// selection — i.e. Backspace behavior).
    void backspace();

    /// Delete the character after each cursor (Delete key behavior).
    void deleteChar();

    /// Remove selected text at every cursor. No-op if no selection.
    void removeSelectedText();

    // ── Selection operations ──────────────────────────────────────────────

    /// Ctrl+D — find the next occurrence of the primary selection and add a
    /// cursor with that selection. Returns true if a match was found.
    bool addCursorAtNextOccurrence();

    /// Ctrl+Shift+L — find ALL occurrences of the primary selection and
    /// create cursors at each. Returns the number of cursors added.
    int addCursorsAtAllOccurrences();

    /// Build rectangular (column/box) selection from a visual rectangle
    /// defined by [startLine, startCol] to [endLine, endCol] (0-based).
    /// Creates one cursor per line spanning the column range.
    void setRectangularSelection(int startLine, int startCol,
                                 int endLine, int endCol);

    // ── Cursor movement (applied to ALL cursors) ─────────────────────────

    /// Move all cursors one character left. If keepSelection is true,
    /// extends selection; otherwise clears it.
    void moveLeft(bool keepSelection = false);

    /// Move all cursors one character right.
    void moveRight(bool keepSelection = false);

    /// Move all cursors one line up.
    void moveUp(bool keepSelection = false);

    /// Move all cursors one line down.
    void moveDown(bool keepSelection = false);

    /// Move all cursors to the start of their current line.
    void moveToStartOfLine(bool keepSelection = false);

    /// Move all cursors to the end of their current line.
    void moveToEndOfLine(bool keepSelection = false);

    /// Move all cursors one word to the left.
    void moveWordLeft(bool keepSelection = false);

    /// Move all cursors one word to the right.
    void moveWordRight(bool keepSelection = false);

    // ── Line operations (applied to ALL cursors) ─────────────────────────

    /// Duplicate the line at each cursor position.
    void duplicateLine();

    /// Delete the entire line at each cursor position.
    void deleteLine();

    // ── Word selection ───────────────────────────────────────────────────

    /// Select the word under each cursor.
    void selectWordUnderCursors();

    // ── Internal helpers ──────────────────────────────────────────────────
private:
    void moveAllCursors(QTextCursor::MoveOperation op, bool keepSelection);
    /// Merge overlapping or touching cursors. Called after every mutation.
    void mergeCursors();

    /// Sort m_cursors by position (ascending).
    void sortCursors();

    QTextDocument              *m_doc = nullptr;
    std::vector<QTextCursor>    m_cursors;
};
