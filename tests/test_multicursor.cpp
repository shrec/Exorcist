#include <QTest>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>

#include "editor/multicursorengine.h"

class TestMultiCursor : public QObject
{
    Q_OBJECT

private slots:
    // ── Lifecycle & basic state ───────────────────────────────────────────
    void initEmptyEngine();
    void setDocumentClearsCursors();

    // ── Cursor management ─────────────────────────────────────────────────
    void addSingleCursor();
    void addMultipleCursors();
    void addCursorMergesOverlap();
    void addCursorWithSelection();
    void addCursorOutOfBoundsClamps();
    void clearSecondaryCursors();
    void clearAll();
    void setPrimaryCursorOnEmpty();
    void primaryCursorReturnsLowest();

    // ── Text insertion ────────────────────────────────────────────────────
    void insertTextSingleCursor();
    void insertTextMultipleCursors();
    void insertTextReplacesSelection();
    void insertTextEmptyString();

    // ── Backspace ─────────────────────────────────────────────────────────
    void backspaceSingleCursor();
    void backspaceMultipleCursors();
    void backspaceWithSelection();
    void backspaceAtDocumentStart();

    // ── Delete ────────────────────────────────────────────────────────────
    void deleteCharSingleCursor();
    void deleteCharMultipleCursors();
    void deleteCharWithSelection();
    void deleteCharAtDocumentEnd();

    // ── removeSelectedText ────────────────────────────────────────────────
    void removeSelectedTextNoSelection();
    void removeSelectedTextMultiple();

    // ── Ctrl+D (addCursorAtNextOccurrence) ────────────────────────────────
    void ctrlD_findsNextOccurrence();
    void ctrlD_wrapsAround();
    void ctrlD_noSelectionReturnsFalse();
    void ctrlD_noMoreOccurrences();
    void ctrlD_skipsDuplicate();

    // ── Ctrl+Shift+L (addCursorsAtAllOccurrences) ─────────────────────────
    void ctrlShiftL_allOccurrences();
    void ctrlShiftL_noSelectionReturnsZero();
    void ctrlShiftL_singleOccurrence();

    // ── Rectangular selection ─────────────────────────────────────────────
    void rectSelection_basicBox();
    void rectSelection_reversedCoords();
    void rectSelection_shortLines();
    void rectSelection_singleLine();
    void rectSelection_emptyDocument();

    // ── Merge cursors ─────────────────────────────────────────────────────
    void mergeTouchingCursors();
    void mergeOverlappingSelections();

    // ── Cursor movement ───────────────────────────────────────────────────
    void moveLeft_movesCursors();
    void moveRight_movesCursors();
    void moveUp_movesCursors();
    void moveDown_movesCursors();
    void moveLeft_withSelection();
    void moveToStartOfLine();
    void moveToEndOfLine();
    void moveWordLeft();
    void moveWordRight();
    void moveRight_mergesCursors();

    // ── Line operations ───────────────────────────────────────────────────
    void duplicateLine_singleCursor();
    void duplicateLine_multipleCursors();
    void deleteLine_singleCursor();
    void deleteLine_multipleCursors();
    void deleteLine_lastLine();

    // ── Word selection ────────────────────────────────────────────────────
    void selectWord_singleCursor();
    void selectWord_multipleCursors();

    // ── Edge cases ────────────────────────────────────────────────────────
    void operationsWithNullDocument();
    void operationsWithEmptyDocument();
};

// ═══════════════════════════════════════════════════════════════════════════
// Lifecycle
// ═══════════════════════════════════════════════════════════════════════════

void TestMultiCursor::initEmptyEngine()
{
    MultiCursorEngine engine;
    QCOMPARE(engine.cursorCount(), 0);
    QVERIFY(!engine.hasMultipleCursors());
    QVERIFY(engine.document() == nullptr);
}

void TestMultiCursor::setDocumentClearsCursors()
{
    QTextDocument doc(QStringLiteral("hello world"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(0);
    engine.addCursor(5);
    QCOMPARE(engine.cursorCount(), 2);

    QTextDocument doc2(QStringLiteral("other"));
    engine.setDocument(&doc2);
    QCOMPARE(engine.cursorCount(), 0);
    QCOMPARE(engine.document(), &doc2);
}

// ═══════════════════════════════════════════════════════════════════════════
// Cursor management
// ═══════════════════════════════════════════════════════════════════════════

void TestMultiCursor::addSingleCursor()
{
    QTextDocument doc(QStringLiteral("hello"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(3);
    QCOMPARE(engine.cursorCount(), 1);
    QCOMPARE(engine.primaryCursor().position(), 3);
}

void TestMultiCursor::addMultipleCursors()
{
    QTextDocument doc(QStringLiteral("hello world test"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(0);
    engine.addCursor(6);
    engine.addCursor(12);
    QCOMPARE(engine.cursorCount(), 3);
    QVERIFY(engine.hasMultipleCursors());

    auto curs = engine.cursors();
    QCOMPARE(curs[0].position(), 0);
    QCOMPARE(curs[1].position(), 6);
    QCOMPARE(curs[2].position(), 12);
}

void TestMultiCursor::addCursorMergesOverlap()
{
    QTextDocument doc(QStringLiteral("hello"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(3);
    engine.addCursor(3); // same position — should merge
    QCOMPARE(engine.cursorCount(), 1);
}

void TestMultiCursor::addCursorWithSelection()
{
    QTextDocument doc(QStringLiteral("hello world"));
    MultiCursorEngine engine(&doc);
    engine.addCursorWithSelection(0, 5);
    QCOMPARE(engine.cursorCount(), 1);
    auto cur = engine.primaryCursor();
    QVERIFY(cur.hasSelection());
    QCOMPARE(cur.selectedText(), QStringLiteral("hello"));
}

void TestMultiCursor::addCursorOutOfBoundsClamps()
{
    QTextDocument doc(QStringLiteral("hi"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(9999);
    QCOMPARE(engine.cursorCount(), 1);
    // Should be clamped to document end.
    QVERIFY(engine.primaryCursor().position() <= doc.characterCount() - 1);
}

void TestMultiCursor::clearSecondaryCursors()
{
    QTextDocument doc(QStringLiteral("hello world"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(0);
    engine.addCursor(5);
    engine.addCursor(10);
    QCOMPARE(engine.cursorCount(), 3);

    engine.clearSecondaryCursors();
    QCOMPARE(engine.cursorCount(), 1);
    QCOMPARE(engine.primaryCursor().position(), 0); // lowest position
}

void TestMultiCursor::clearAll()
{
    QTextDocument doc(QStringLiteral("hello"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(0);
    engine.addCursor(3);
    engine.clearAll();
    QCOMPARE(engine.cursorCount(), 0);
}

void TestMultiCursor::setPrimaryCursorOnEmpty()
{
    QTextDocument doc(QStringLiteral("test"));
    MultiCursorEngine engine(&doc);
    QTextCursor cur(&doc);
    cur.setPosition(2);
    engine.setPrimaryCursor(cur);
    QCOMPARE(engine.cursorCount(), 1);
    QCOMPARE(engine.primaryCursor().position(), 2);
}

void TestMultiCursor::primaryCursorReturnsLowest()
{
    QTextDocument doc(QStringLiteral("hello world"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(8);
    engine.addCursor(2);
    engine.addCursor(5);
    QCOMPARE(engine.primaryCursor().position(), 2);
}

// ═══════════════════════════════════════════════════════════════════════════
// Text insertion
// ═══════════════════════════════════════════════════════════════════════════

void TestMultiCursor::insertTextSingleCursor()
{
    QTextDocument doc(QStringLiteral("hllo"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(1);
    engine.insertText(QStringLiteral("e"));
    QCOMPARE(doc.toPlainText(), QStringLiteral("hello"));
}

void TestMultiCursor::insertTextMultipleCursors()
{
    // "ab" → insert "X" at pos 0 and pos 1:
    // "XaXb" (X before a, X before b)
    QTextDocument doc(QStringLiteral("ab"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(0);
    engine.addCursor(1);
    engine.insertText(QStringLiteral("X"));
    QCOMPARE(doc.toPlainText(), QStringLiteral("XaXb"));
}

void TestMultiCursor::insertTextReplacesSelection()
{
    QTextDocument doc(QStringLiteral("hello world"));
    MultiCursorEngine engine(&doc);
    engine.addCursorWithSelection(0, 5); // select "hello"
    engine.insertText(QStringLiteral("hi"));
    QCOMPARE(doc.toPlainText(), QStringLiteral("hi world"));
}

void TestMultiCursor::insertTextEmptyString()
{
    QTextDocument doc(QStringLiteral("hello"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(0);
    engine.insertText(QString());
    QCOMPARE(doc.toPlainText(), QStringLiteral("hello"));
}

// ═══════════════════════════════════════════════════════════════════════════
// Backspace
// ═══════════════════════════════════════════════════════════════════════════

void TestMultiCursor::backspaceSingleCursor()
{
    QTextDocument doc(QStringLiteral("hXello"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(2); // after 'X'
    engine.backspace();
    QCOMPARE(doc.toPlainText(), QStringLiteral("hello"));
}

void TestMultiCursor::backspaceMultipleCursors()
{
    // "XaXb" → backspace at pos 1 (after first X) and pos 3 (after second X)
    QTextDocument doc(QStringLiteral("XaXb"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(1); // after first X
    engine.addCursor(3); // after second X
    engine.backspace();
    QCOMPARE(doc.toPlainText(), QStringLiteral("ab"));
}

void TestMultiCursor::backspaceWithSelection()
{
    QTextDocument doc(QStringLiteral("hello world"));
    MultiCursorEngine engine(&doc);
    engine.addCursorWithSelection(5, 6); // select the space
    engine.backspace();
    QCOMPARE(doc.toPlainText(), QStringLiteral("helloworld"));
}

void TestMultiCursor::backspaceAtDocumentStart()
{
    QTextDocument doc(QStringLiteral("hello"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(0);
    engine.backspace(); // should be no-op
    QCOMPARE(doc.toPlainText(), QStringLiteral("hello"));
}

// ═══════════════════════════════════════════════════════════════════════════
// Delete
// ═══════════════════════════════════════════════════════════════════════════

void TestMultiCursor::deleteCharSingleCursor()
{
    QTextDocument doc(QStringLiteral("hXello"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(1); // before 'X'
    engine.deleteChar();
    QCOMPARE(doc.toPlainText(), QStringLiteral("hello"));
}

void TestMultiCursor::deleteCharMultipleCursors()
{
    // "XaXb" → delete at pos 0 (X) and pos 2 (X)
    QTextDocument doc(QStringLiteral("XaXb"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(0);
    engine.addCursor(2);
    engine.deleteChar();
    QCOMPARE(doc.toPlainText(), QStringLiteral("ab"));
}

void TestMultiCursor::deleteCharWithSelection()
{
    QTextDocument doc(QStringLiteral("hello world"));
    MultiCursorEngine engine(&doc);
    engine.addCursorWithSelection(0, 5); // select "hello"
    engine.deleteChar();
    QCOMPARE(doc.toPlainText(), QStringLiteral(" world"));
}

void TestMultiCursor::deleteCharAtDocumentEnd()
{
    QTextDocument doc(QStringLiteral("hi"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(doc.characterCount() - 1); // end
    engine.deleteChar(); // should be no-op
    QCOMPARE(doc.toPlainText(), QStringLiteral("hi"));
}

// ═══════════════════════════════════════════════════════════════════════════
// removeSelectedText
// ═══════════════════════════════════════════════════════════════════════════

void TestMultiCursor::removeSelectedTextNoSelection()
{
    QTextDocument doc(QStringLiteral("hello"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(3); // no selection
    engine.removeSelectedText();
    QCOMPARE(doc.toPlainText(), QStringLiteral("hello")); // unchanged
}

void TestMultiCursor::removeSelectedTextMultiple()
{
    // "AAhelloBBworld" → select and remove "AA" and "BB"
    QTextDocument doc(QStringLiteral("AAhelloBBworld"));
    MultiCursorEngine engine(&doc);
    engine.addCursorWithSelection(0, 2);  // "AA"
    engine.addCursorWithSelection(7, 9);  // "BB"
    engine.removeSelectedText();
    QCOMPARE(doc.toPlainText(), QStringLiteral("helloworld"));
}

// ═══════════════════════════════════════════════════════════════════════════
// Ctrl+D
// ═══════════════════════════════════════════════════════════════════════════

void TestMultiCursor::ctrlD_findsNextOccurrence()
{
    QTextDocument doc(QStringLiteral("foo bar foo baz foo"));
    MultiCursorEngine engine(&doc);
    engine.addCursorWithSelection(0, 3); // select first "foo"
    bool found = engine.addCursorAtNextOccurrence();
    QVERIFY(found);
    QCOMPARE(engine.cursorCount(), 2);

    // Second cursor should select "foo" at position 8
    auto curs = engine.cursors();
    QCOMPARE(curs[1].selectedText(), QStringLiteral("foo"));
}

void TestMultiCursor::ctrlD_wrapsAround()
{
    QTextDocument doc(QStringLiteral("foo bar foo"));
    MultiCursorEngine engine(&doc);
    // Select the second "foo" (pos 8-11)
    engine.addCursorWithSelection(8, 11);
    bool found = engine.addCursorAtNextOccurrence();
    QVERIFY(found);
    QCOMPARE(engine.cursorCount(), 2);

    // Should have wrapped and found first "foo" at pos 0
    auto curs = engine.cursors();
    QCOMPARE(qMin(curs[0].anchor(), curs[0].position()), 0);
}

void TestMultiCursor::ctrlD_noSelectionReturnsFalse()
{
    QTextDocument doc(QStringLiteral("foo bar foo"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(0); // no selection
    QVERIFY(!engine.addCursorAtNextOccurrence());
    QCOMPARE(engine.cursorCount(), 1);
}

void TestMultiCursor::ctrlD_noMoreOccurrences()
{
    QTextDocument doc(QStringLiteral("unique"));
    MultiCursorEngine engine(&doc);
    engine.addCursorWithSelection(0, 6); // select "unique"
    bool found = engine.addCursorAtNextOccurrence();
    QVERIFY(!found); // only one occurrence
    QCOMPARE(engine.cursorCount(), 1);
}

void TestMultiCursor::ctrlD_skipsDuplicate()
{
    QTextDocument doc(QStringLiteral("foo bar foo"));
    MultiCursorEngine engine(&doc);
    engine.addCursorWithSelection(0, 3); // first "foo"
    engine.addCursorAtNextOccurrence();  // adds second "foo"
    QCOMPARE(engine.cursorCount(), 2);

    bool found = engine.addCursorAtNextOccurrence(); // no more
    QVERIFY(!found);
    QCOMPARE(engine.cursorCount(), 2);
}

// ═══════════════════════════════════════════════════════════════════════════
// Ctrl+Shift+L
// ═══════════════════════════════════════════════════════════════════════════

void TestMultiCursor::ctrlShiftL_allOccurrences()
{
    QTextDocument doc(QStringLiteral("foo bar foo baz foo"));
    MultiCursorEngine engine(&doc);
    engine.addCursorWithSelection(0, 3); // select first "foo"
    int added = engine.addCursorsAtAllOccurrences();
    QVERIFY(added > 0);
    QCOMPARE(engine.cursorCount(), 3); // three "foo"s

    auto curs = engine.cursors();
    for (auto &c : curs)
        QCOMPARE(c.selectedText(), QStringLiteral("foo"));
}

void TestMultiCursor::ctrlShiftL_noSelectionReturnsZero()
{
    QTextDocument doc(QStringLiteral("foo"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(0);
    QCOMPARE(engine.addCursorsAtAllOccurrences(), 0);
}

void TestMultiCursor::ctrlShiftL_singleOccurrence()
{
    QTextDocument doc(QStringLiteral("unique word"));
    MultiCursorEngine engine(&doc);
    engine.addCursorWithSelection(0, 6); // "unique"
    int added = engine.addCursorsAtAllOccurrences();
    // Replaces the 1 cursor with 1 found — net 0 added
    QCOMPARE(engine.cursorCount(), 1);
    QCOMPARE(added, 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// Rectangular selection
// ═══════════════════════════════════════════════════════════════════════════

void TestMultiCursor::rectSelection_basicBox()
{
    // 3 lines of 10 chars each
    QTextDocument doc(QStringLiteral("0123456789\n0123456789\n0123456789"));
    MultiCursorEngine engine(&doc);
    engine.setRectangularSelection(0, 2, 2, 5);
    QCOMPARE(engine.cursorCount(), 3);

    auto curs = engine.cursors();
    for (auto &c : curs) {
        QVERIFY(c.hasSelection());
        QCOMPARE(c.selectedText(), QStringLiteral("234"));
    }
}

void TestMultiCursor::rectSelection_reversedCoords()
{
    QTextDocument doc(QStringLiteral("abcdef\nabcdef\nabcdef"));
    MultiCursorEngine engine(&doc);
    // endLine < startLine, endCol < startCol — should normalize
    engine.setRectangularSelection(2, 4, 0, 1);
    QCOMPARE(engine.cursorCount(), 3);
    auto curs = engine.cursors();
    for (auto &c : curs)
        QCOMPARE(c.selectedText(), QStringLiteral("bcd"));
}

void TestMultiCursor::rectSelection_shortLines()
{
    // Lines have different lengths
    QTextDocument doc(QStringLiteral("long line here\nhi\nmedium"));
    MultiCursorEngine engine(&doc);
    engine.setRectangularSelection(0, 0, 2, 6);
    QCOMPARE(engine.cursorCount(), 3);

    auto curs = engine.cursors();
    QCOMPARE(curs[0].selectedText(), QStringLiteral("long l"));  // full 6 chars
    QCOMPARE(curs[1].selectedText(), QStringLiteral("hi"));       // only 2 chars available
    QCOMPARE(curs[2].selectedText(), QStringLiteral("medium"));   // exactly 6
}

void TestMultiCursor::rectSelection_singleLine()
{
    QTextDocument doc(QStringLiteral("hello"));
    MultiCursorEngine engine(&doc);
    engine.setRectangularSelection(0, 1, 0, 3);
    QCOMPARE(engine.cursorCount(), 1);
    QCOMPARE(engine.primaryCursor().selectedText(), QStringLiteral("el"));
}

void TestMultiCursor::rectSelection_emptyDocument()
{
    QTextDocument doc;
    MultiCursorEngine engine(&doc);
    engine.setRectangularSelection(0, 0, 5, 5);
    // Empty doc has 1 block but 0 length — should produce at most 1 cursor with no selection
    QVERIFY(engine.cursorCount() <= 1);
}

// ═══════════════════════════════════════════════════════════════════════════
// Merge cursors
// ═══════════════════════════════════════════════════════════════════════════

void TestMultiCursor::mergeTouchingCursors()
{
    QTextDocument doc(QStringLiteral("hello"));
    MultiCursorEngine engine(&doc);
    engine.addCursorWithSelection(0, 3); // "hel"
    engine.addCursorWithSelection(3, 5); // "lo"  — touching at 3
    // Should merge into one cursor spanning 0..5
    QCOMPARE(engine.cursorCount(), 1);
    auto cur = engine.primaryCursor();
    QVERIFY(cur.hasSelection());
    QCOMPARE(cur.selectedText(), QStringLiteral("hello"));
}

void TestMultiCursor::mergeOverlappingSelections()
{
    QTextDocument doc(QStringLiteral("hello world"));
    MultiCursorEngine engine(&doc);
    engine.addCursorWithSelection(0, 6); // "hello "
    engine.addCursorWithSelection(3, 9); // "lo wor" — overlaps
    // Should merge into one cursor spanning 0..9
    QCOMPARE(engine.cursorCount(), 1);
}

// ═══════════════════════════════════════════════════════════════════════════
// Cursor movement
// ═══════════════════════════════════════════════════════════════════════════

void TestMultiCursor::moveLeft_movesCursors()
{
    QTextDocument doc(QStringLiteral("abcdef"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(2); // after 'b'
    engine.addCursor(5); // after 'e'
    engine.moveLeft();
    auto curs = engine.cursors();
    QCOMPARE(curs.size(), 2u);
    QCOMPARE(curs[0].position(), 1);
    QCOMPARE(curs[1].position(), 4);
}

void TestMultiCursor::moveRight_movesCursors()
{
    QTextDocument doc(QStringLiteral("abcdef"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(1);
    engine.addCursor(3);
    engine.moveRight();
    auto curs = engine.cursors();
    QCOMPARE(curs.size(), 2u);
    QCOMPARE(curs[0].position(), 2);
    QCOMPARE(curs[1].position(), 4);
}

void TestMultiCursor::moveUp_movesCursors()
{
    QTextDocument doc(QStringLiteral("line1\nline2\nline3"));
    MultiCursorEngine engine(&doc);
    // Place cursor at start of line2 and line3
    engine.addCursor(6);  // start of "line2"
    engine.addCursor(12); // start of "line3"
    engine.moveUp();
    auto curs = engine.cursors();
    QCOMPARE(curs.size(), 2u);
    // After moveUp, they should be on line1 and line2
    QCOMPARE(curs[0].block().blockNumber(), 0);
    QCOMPARE(curs[1].block().blockNumber(), 1);
}

void TestMultiCursor::moveDown_movesCursors()
{
    QTextDocument doc(QStringLiteral("line1\nline2\nline3"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(0);  // start of "line1"
    engine.addCursor(6);  // start of "line2"
    engine.moveDown();
    auto curs = engine.cursors();
    QCOMPARE(curs.size(), 2u);
    QCOMPARE(curs[0].block().blockNumber(), 1);
    QCOMPARE(curs[1].block().blockNumber(), 2);
}

void TestMultiCursor::moveLeft_withSelection()
{
    QTextDocument doc(QStringLiteral("abcdef"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(3);
    engine.moveLeft(true); // keepSelection
    auto curs = engine.cursors();
    QCOMPARE(curs.size(), 1u);
    QVERIFY(curs[0].hasSelection());
    QCOMPARE(curs[0].selectedText(), QStringLiteral("c"));
}

void TestMultiCursor::moveToStartOfLine()
{
    QTextDocument doc(QStringLiteral("hello\nworld"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(8); // middle of "world"
    engine.moveToStartOfLine();
    auto curs = engine.cursors();
    QCOMPARE(curs[0].positionInBlock(), 0);
    QCOMPARE(curs[0].block().blockNumber(), 1);
}

void TestMultiCursor::moveToEndOfLine()
{
    QTextDocument doc(QStringLiteral("hello\nworld"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(6); // start of "world"
    engine.moveToEndOfLine();
    auto curs = engine.cursors();
    QCOMPARE(curs[0].positionInBlock(), 5); // end of "world"
}

void TestMultiCursor::moveWordLeft()
{
    QTextDocument doc(QStringLiteral("hello world test"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(11); // at 't' of "test"
    engine.moveWordLeft();
    auto curs = engine.cursors();
    // Should move to start of "world" (position 6)
    QCOMPARE(curs[0].position(), 6);
}

void TestMultiCursor::moveWordRight()
{
    QTextDocument doc(QStringLiteral("hello world test"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(0); // start
    engine.moveWordRight();
    auto curs = engine.cursors();
    // Should move past "hello" to position 5 or 6
    QVERIFY(curs[0].position() >= 5);
}

void TestMultiCursor::moveRight_mergesCursors()
{
    QTextDocument doc(QStringLiteral("ab"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(0);
    engine.addCursor(1);
    // "ab" has characterCount 3 (a, b, paragraph separator)
    // After moveRight: positions 1, 2. After another: 2, 2 → merge.
    engine.moveRight();
    engine.moveRight();
    QCOMPARE(engine.cursorCount(), 1);
}

// ═══════════════════════════════════════════════════════════════════════════
// Line operations
// ═══════════════════════════════════════════════════════════════════════════

void TestMultiCursor::duplicateLine_singleCursor()
{
    QTextDocument doc(QStringLiteral("alpha\nbeta\ngamma"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(7); // somewhere in "beta"
    engine.duplicateLine();
    QCOMPARE(doc.toPlainText(), QStringLiteral("alpha\nbeta\nbeta\ngamma"));
}

void TestMultiCursor::duplicateLine_multipleCursors()
{
    QTextDocument doc(QStringLiteral("aaa\nbbb\nccc"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(0);  // in "aaa"
    engine.addCursor(5);  // in "bbb"
    engine.duplicateLine();
    QCOMPARE(doc.toPlainText(), QStringLiteral("aaa\naaa\nbbb\nbbb\nccc"));
}

void TestMultiCursor::deleteLine_singleCursor()
{
    QTextDocument doc(QStringLiteral("alpha\nbeta\ngamma"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(7); // in "beta"
    engine.deleteLine();
    QCOMPARE(doc.toPlainText(), QStringLiteral("alpha\ngamma"));
}

void TestMultiCursor::deleteLine_multipleCursors()
{
    QTextDocument doc(QStringLiteral("aaa\nbbb\nccc\nddd"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(0);  // in "aaa"
    engine.addCursor(8);  // in "ccc"
    engine.deleteLine();
    QCOMPARE(doc.toPlainText(), QStringLiteral("bbb\nddd"));
}

void TestMultiCursor::deleteLine_lastLine()
{
    QTextDocument doc(QStringLiteral("only line"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(3);
    engine.deleteLine();
    // Should clear the line content
    QVERIFY(doc.toPlainText().isEmpty());
}

// ═══════════════════════════════════════════════════════════════════════════
// Word selection
// ═══════════════════════════════════════════════════════════════════════════

void TestMultiCursor::selectWord_singleCursor()
{
    QTextDocument doc(QStringLiteral("hello world"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(2); // inside "hello"
    engine.selectWordUnderCursors();
    auto curs = engine.cursors();
    QCOMPARE(curs.size(), 1u);
    QVERIFY(curs[0].hasSelection());
    QCOMPARE(curs[0].selectedText(), QStringLiteral("hello"));
}

void TestMultiCursor::selectWord_multipleCursors()
{
    QTextDocument doc(QStringLiteral("foo bar baz"));
    MultiCursorEngine engine(&doc);
    engine.addCursor(1);  // inside "foo"
    engine.addCursor(5);  // inside "bar"
    engine.addCursor(9);  // inside "baz"
    engine.selectWordUnderCursors();
    auto curs = engine.cursors();
    QCOMPARE(curs.size(), 3u);
    QCOMPARE(curs[0].selectedText(), QStringLiteral("foo"));
    QCOMPARE(curs[1].selectedText(), QStringLiteral("bar"));
    QCOMPARE(curs[2].selectedText(), QStringLiteral("baz"));
}

// ═══════════════════════════════════════════════════════════════════════════
// Edge cases
// ═══════════════════════════════════════════════════════════════════════════

void TestMultiCursor::operationsWithNullDocument()
{
    MultiCursorEngine engine;
    // All operations should be safe no-ops.
    engine.addCursor(0);
    QCOMPARE(engine.cursorCount(), 0);
    engine.insertText(QStringLiteral("test"));
    engine.backspace();
    engine.deleteChar();
    engine.removeSelectedText();
    engine.moveLeft();
    engine.moveRight();
    engine.moveUp();
    engine.moveDown();
    engine.duplicateLine();
    engine.deleteLine();
    engine.selectWordUnderCursors();
    QVERIFY(!engine.addCursorAtNextOccurrence());
    QCOMPARE(engine.addCursorsAtAllOccurrences(), 0);
    engine.setRectangularSelection(0, 0, 5, 5);
    QCOMPARE(engine.cursorCount(), 0);
}

void TestMultiCursor::operationsWithEmptyDocument()
{
    QTextDocument doc;
    MultiCursorEngine engine(&doc);
    engine.addCursor(0);
    QCOMPARE(engine.cursorCount(), 1);
    engine.insertText(QStringLiteral("hello"));
    QCOMPARE(doc.toPlainText(), QStringLiteral("hello"));
}

QTEST_MAIN(TestMultiCursor)
#include "test_multicursor.moc"
