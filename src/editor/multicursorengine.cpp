#include "multicursorengine.h"

#include <QTextBlock>

#include <algorithm>

MultiCursorEngine::MultiCursorEngine(QTextDocument *doc)
    : m_doc(doc)
{
}

void MultiCursorEngine::setDocument(QTextDocument *doc)
{
    m_doc = doc;
    m_cursors.clear();
}

int MultiCursorEngine::cursorCount() const
{
    return static_cast<int>(m_cursors.size());
}

bool MultiCursorEngine::hasMultipleCursors() const
{
    return m_cursors.size() > 1;
}

std::vector<QTextCursor> MultiCursorEngine::cursors() const
{
    auto copy = m_cursors;
    std::sort(copy.begin(), copy.end(),
              [](const QTextCursor &a, const QTextCursor &b) {
                  return a.position() < b.position();
              });
    return copy;
}

void MultiCursorEngine::addCursor(int position)
{
    if (!m_doc)
        return;
    QTextCursor cur(m_doc);
    cur.setPosition(qBound(0, position, m_doc->characterCount() - 1));
    m_cursors.push_back(cur);
    mergeCursors();
}

void MultiCursorEngine::addCursorWithSelection(int anchor, int position)
{
    if (!m_doc)
        return;
    const int maxPos = m_doc->characterCount() - 1;
    QTextCursor cur(m_doc);
    cur.setPosition(qBound(0, anchor, maxPos));
    cur.setPosition(qBound(0, position, maxPos), QTextCursor::KeepAnchor);
    m_cursors.push_back(cur);
    mergeCursors();
}

void MultiCursorEngine::clearSecondaryCursors()
{
    if (m_cursors.size() <= 1)
        return;
    sortCursors();
    QTextCursor primary = m_cursors.front();
    m_cursors.clear();
    m_cursors.push_back(primary);
}

void MultiCursorEngine::clearAll()
{
    m_cursors.clear();
}

void MultiCursorEngine::setPrimaryCursor(const QTextCursor &cursor)
{
    if (m_cursors.empty())
        m_cursors.push_back(cursor);
    else {
        sortCursors();
        m_cursors[0] = cursor;
    }
    mergeCursors();
}

QTextCursor MultiCursorEngine::primaryCursor() const
{
    if (m_cursors.empty())
        return QTextCursor();
    auto sorted = cursors();
    return sorted.front();
}

// ── Text operations ───────────────────────────────────────────────────────

void MultiCursorEngine::insertText(const QString &text)
{
    if (m_cursors.empty() || !m_doc)
        return;

    // Process bottom-to-top to preserve positions of earlier cursors.
    sortCursors();
    for (int i = static_cast<int>(m_cursors.size()) - 1; i >= 0; --i)
        m_cursors[static_cast<size_t>(i)].insertText(text);

    mergeCursors();
}

void MultiCursorEngine::backspace()
{
    if (m_cursors.empty() || !m_doc)
        return;

    sortCursors();
    for (int i = static_cast<int>(m_cursors.size()) - 1; i >= 0; --i) {
        auto &cur = m_cursors[static_cast<size_t>(i)];
        if (cur.hasSelection())
            cur.removeSelectedText();
        else
            cur.deletePreviousChar();
    }
    mergeCursors();
}

void MultiCursorEngine::deleteChar()
{
    if (m_cursors.empty() || !m_doc)
        return;

    sortCursors();
    for (int i = static_cast<int>(m_cursors.size()) - 1; i >= 0; --i) {
        auto &cur = m_cursors[static_cast<size_t>(i)];
        if (cur.hasSelection())
            cur.removeSelectedText();
        else
            cur.deleteChar();
    }
    mergeCursors();
}

void MultiCursorEngine::removeSelectedText()
{
    if (m_cursors.empty() || !m_doc)
        return;

    sortCursors();
    for (int i = static_cast<int>(m_cursors.size()) - 1; i >= 0; --i) {
        auto &cur = m_cursors[static_cast<size_t>(i)];
        if (cur.hasSelection())
            cur.removeSelectedText();
    }
    mergeCursors();
}

// ── Selection operations ──────────────────────────────────────────────────

bool MultiCursorEngine::addCursorAtNextOccurrence()
{
    if (m_cursors.empty() || !m_doc)
        return false;

    // Use the primary cursor's selection as the search term.
    QTextCursor primary = primaryCursor();
    if (!primary.hasSelection())
        return false;

    const QString needle = primary.selectedText();
    if (needle.isEmpty())
        return false;

    // Find the position after the last cursor that has this selection,
    // so we search forward from the latest match.
    sortCursors();
    int searchFrom = 0;
    for (auto &cur : m_cursors) {
        if (cur.hasSelection() && cur.selectedText() == needle) {
            int end = qMax(cur.anchor(), cur.position());
            if (end > searchFrom)
                searchFrom = end;
        }
    }

    // Search forward from the last occurrence.
    QTextCursor found = m_doc->find(needle, searchFrom);

    // Wrap around if not found.
    if (found.isNull())
        found = m_doc->find(needle, 0);

    if (found.isNull())
        return false;

    // Check we haven't already got a cursor at this exact match.
    int foundStart = qMin(found.anchor(), found.position());
    int foundEnd   = qMax(found.anchor(), found.position());
    for (auto &cur : m_cursors) {
        int s = qMin(cur.anchor(), cur.position());
        int e = qMax(cur.anchor(), cur.position());
        if (s == foundStart && e == foundEnd)
            return false; // already have this one
    }

    m_cursors.push_back(found);
    mergeCursors();
    return true;
}

int MultiCursorEngine::addCursorsAtAllOccurrences()
{
    if (m_cursors.empty() || !m_doc)
        return 0;

    QTextCursor primary = primaryCursor();
    if (!primary.hasSelection())
        return 0;

    const QString needle = primary.selectedText();
    if (needle.isEmpty())
        return 0;

    // Find all occurrences.
    std::vector<QTextCursor> found;
    QTextCursor search = m_doc->find(needle, 0);
    while (!search.isNull()) {
        found.push_back(search);
        search = m_doc->find(needle, search);
    }

    if (found.empty())
        return 0;

    // Replace ALL cursors with the found matches.
    int prevCount = static_cast<int>(m_cursors.size());
    m_cursors = std::move(found);
    mergeCursors();
    return static_cast<int>(m_cursors.size()) - prevCount;
}

void MultiCursorEngine::setRectangularSelection(int startLine, int startCol,
                                                 int endLine, int endCol)
{
    if (!m_doc)
        return;

    m_cursors.clear();

    // Normalize ranges.
    if (startLine > endLine)
        std::swap(startLine, endLine);
    if (startCol > endCol)
        std::swap(startCol, endCol);

    const int blockCount = m_doc->blockCount();
    if (startLine >= blockCount)
        return;
    endLine = qMin(endLine, blockCount - 1);

    for (int line = startLine; line <= endLine; ++line) {
        QTextBlock block = m_doc->findBlockByNumber(line);
        if (!block.isValid())
            continue;

        const int lineLen = block.length() - 1; // -1 for block separator
        const int anchorCol = qMin(startCol, lineLen);
        const int posCol    = qMin(endCol, lineLen);

        QTextCursor cur(block);
        cur.movePosition(QTextCursor::StartOfBlock);
        cur.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, anchorCol);
        cur.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, posCol - anchorCol);
        m_cursors.push_back(cur);
    }
    // No merge needed — each cursor is on a separate line.
}

// ── Cursor movement ──────────────────────────────────────────────────────

void MultiCursorEngine::moveAllCursors(QTextCursor::MoveOperation op,
                                       bool keepSelection)
{
    if (m_cursors.empty() || !m_doc)
        return;

    auto mode = keepSelection ? QTextCursor::KeepAnchor
                              : QTextCursor::MoveAnchor;
    for (auto &cur : m_cursors)
        cur.movePosition(op, mode);

    mergeCursors();
}

void MultiCursorEngine::moveLeft(bool keepSelection)
{
    moveAllCursors(QTextCursor::Left, keepSelection);
}

void MultiCursorEngine::moveRight(bool keepSelection)
{
    moveAllCursors(QTextCursor::Right, keepSelection);
}

void MultiCursorEngine::moveUp(bool keepSelection)
{
    moveAllCursors(QTextCursor::Up, keepSelection);
}

void MultiCursorEngine::moveDown(bool keepSelection)
{
    moveAllCursors(QTextCursor::Down, keepSelection);
}

void MultiCursorEngine::moveToStartOfLine(bool keepSelection)
{
    moveAllCursors(QTextCursor::StartOfBlock, keepSelection);
}

void MultiCursorEngine::moveToEndOfLine(bool keepSelection)
{
    moveAllCursors(QTextCursor::EndOfBlock, keepSelection);
}

void MultiCursorEngine::moveWordLeft(bool keepSelection)
{
    moveAllCursors(QTextCursor::WordLeft, keepSelection);
}

void MultiCursorEngine::moveWordRight(bool keepSelection)
{
    moveAllCursors(QTextCursor::WordRight, keepSelection);
}

// ── Line operations ─────────────────────────────────────────────────────

void MultiCursorEngine::duplicateLine()
{
    if (m_cursors.empty() || !m_doc)
        return;

    sortCursors();
    // Bottom-to-top so earlier line numbers stay valid.
    for (int i = static_cast<int>(m_cursors.size()) - 1; i >= 0; --i) {
        auto &cur = m_cursors[static_cast<size_t>(i)];
        QTextCursor tc(cur);
        tc.movePosition(QTextCursor::StartOfBlock);
        tc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        QString lineText = tc.selectedText();
        tc.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
        tc.insertText(QStringLiteral("\n") + lineText);
    }
    mergeCursors();
}

void MultiCursorEngine::deleteLine()
{
    if (m_cursors.empty() || !m_doc)
        return;

    sortCursors();
    for (int i = static_cast<int>(m_cursors.size()) - 1; i >= 0; --i) {
        auto &cur = m_cursors[static_cast<size_t>(i)];
        QTextCursor tc(cur);
        tc.movePosition(QTextCursor::StartOfBlock);
        tc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        // Include the newline after the block (or before if last line).
        if (!tc.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor)) {
            // Last line — include the preceding newline instead.
            tc.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
            tc.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
            if (tc.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor)) {
                tc.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
                tc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            } else {
                // Only line — select all content.
                tc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            }
        }
        tc.removeSelectedText();
        cur = tc;
    }
    mergeCursors();
}

// ── Word selection ──────────────────────────────────────────────────────

void MultiCursorEngine::selectWordUnderCursors()
{
    if (m_cursors.empty() || !m_doc)
        return;

    for (auto &cur : m_cursors)
        cur.select(QTextCursor::WordUnderCursor);

    mergeCursors();
}

// ── Internal helpers ──────────────────────────────────────────────────────

void MultiCursorEngine::sortCursors()
{
    std::sort(m_cursors.begin(), m_cursors.end(),
              [](const QTextCursor &a, const QTextCursor &b) {
                  return a.position() < b.position();
              });
}

void MultiCursorEngine::mergeCursors()
{
    if (m_cursors.size() <= 1)
        return;

    sortCursors();

    std::vector<QTextCursor> merged;
    merged.push_back(m_cursors.front());

    for (size_t i = 1; i < m_cursors.size(); ++i) {
        auto &prev = merged.back();
        auto &cur  = m_cursors[i];

        int prevStart = qMin(prev.anchor(), prev.position());
        int prevEnd   = qMax(prev.anchor(), prev.position());
        int curStart  = qMin(cur.anchor(), cur.position());
        int curEnd    = qMax(cur.anchor(), cur.position());

        // Overlap or touching: merge by extending prev.
        if (curStart <= prevEnd) {
            if (curEnd > prevEnd) {
                // Extend the previous cursor's selection to include cur.
                prev.setPosition(prevStart);
                prev.setPosition(qMax(prevEnd, curEnd), QTextCursor::KeepAnchor);
            }
            // If curEnd <= prevEnd, cur is completely inside prev — skip.
        } else {
            merged.push_back(cur);
        }
    }

    m_cursors = std::move(merged);
}
