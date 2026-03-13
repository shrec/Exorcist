#include <QtTest>
#include <QPlainTextEdit>
#include <QKeyEvent>
#include <QSignalSpy>
#include <QTextBlock>

#include "editor/vimhandler.h"

class TestVimHandler : public QObject
{
    Q_OBJECT

private slots:
    void init();

    // ── Mode transitions ─────────────────────────────────────────────────
    void startsDisabled();
    void enableSetsNormalMode();
    void iEntersInsert();
    void escReturnsToNormal();
    void colonEntersCommand();
    void vEntersVisual();
    void shiftV_entersVisualLine();
    void modeStringValues();

    // ── Basic motions (hjkl) ─────────────────────────────────────────────
    void h_movesLeft();
    void j_movesDown();
    void k_movesUp();
    void l_movesRight();
    void h_stopsAtLineStart();
    void l_stopsAtLineEnd();

    // ── Word motions ─────────────────────────────────────────────────────
    void w_movesToNextWord();
    void b_movesToPrevWord();
    void e_movesToWordEnd();

    // ── Line motions ─────────────────────────────────────────────────────
    void zero_movesToLineStart();
    void dollar_movesToLineEnd();

    // ── Document motions ─────────────────────────────────────────────────
    void gg_movesToDocStart();
    void G_movesToDocEnd();
    void countG_goesToLine();

    // ── Bracket matching ─────────────────────────────────────────────────
    void percent_matchesBracket();

    // ── Insert mode transitions ──────────────────────────────────────────
    void a_insertsAfterCursor();
    void A_insertsAtLineEnd();
    void I_insertsAtFirstNonBlank();
    void o_opensLineBelow();
    void O_opensLineAbove();

    // ── Operators ────────────────────────────────────────────────────────
    void x_deletesChar();
    void dd_deletesLine();
    void dw_deletesWord();
    void yy_yanksLine();
    void p_pastesAfter();
    void P_pastesBefore();
    void cc_changesLine();

    // ── Numeric prefix ───────────────────────────────────────────────────
    void count_motions();
    void count_dd();

    // ── Undo/Redo ────────────────────────────────────────────────────────
    void u_undoes();
    void ctrlR_redoes();

    // ── Visual mode ──────────────────────────────────────────────────────
    void visual_d_deletesSelection();
    void visual_y_yanksSelection();
    void visual_c_changesSelection();

    // ── Ex commands ──────────────────────────────────────────────────────
    void exCommand_w_emitsSave();
    void exCommand_q_emitsQuit();
    void exCommand_wq_emitsBoth();
    void exCommand_lineNumber();
    void exCommand_e_emitsOpen();

    // ── Insert passthrough ───────────────────────────────────────────────
    void insert_passesNormalKeys();

private:
    void sendKey(int key, Qt::KeyboardModifiers mods = Qt::NoModifier, const QString &text = {});
    void sendChar(QChar ch);

    std::unique_ptr<QPlainTextEdit> m_editor;
    VimHandler *m_vim = nullptr;  // owned by m_editor
};

void TestVimHandler::init()
{
    m_editor = std::make_unique<QPlainTextEdit>();
    m_editor->setPlainText(QStringLiteral("hello world\nfoo bar baz\nthird line\nfourth line"));
    m_vim = new VimHandler(m_editor.get(), m_editor.get());
    m_vim->setEnabled(true);
    // Position cursor at start
    auto tc = m_editor->textCursor();
    tc.movePosition(QTextCursor::Start);
    m_editor->setTextCursor(tc);
}

void TestVimHandler::sendKey(int key, Qt::KeyboardModifiers mods, const QString &text)
{
    QKeyEvent press(QEvent::KeyPress, key, mods, text);
    m_vim->handleKeyPress(&press);
}

void TestVimHandler::sendChar(QChar ch)
{
    int key = ch.toUpper().unicode();
    auto mods = ch.isUpper() ? Qt::ShiftModifier : Qt::NoModifier;
    sendKey(key, mods, QString(ch));
}

// ── Mode transitions ─────────────────────────────────────────────────────────

void TestVimHandler::startsDisabled()
{
    QPlainTextEdit editor;
    VimHandler vim(&editor);
    QVERIFY(!vim.isEnabled());
    QCOMPARE(vim.mode(), VimHandler::Mode::Normal);
}

void TestVimHandler::enableSetsNormalMode()
{
    QVERIFY(m_vim->isEnabled());
    QCOMPARE(m_vim->mode(), VimHandler::Mode::Normal);
}

void TestVimHandler::iEntersInsert()
{
    sendChar(QLatin1Char('i'));
    QCOMPARE(m_vim->mode(), VimHandler::Mode::Insert);
}

void TestVimHandler::escReturnsToNormal()
{
    sendChar(QLatin1Char('i'));
    QCOMPARE(m_vim->mode(), VimHandler::Mode::Insert);
    sendKey(Qt::Key_Escape);
    QCOMPARE(m_vim->mode(), VimHandler::Mode::Normal);
}

void TestVimHandler::colonEntersCommand()
{
    sendKey(Qt::Key_Colon);
    QCOMPARE(m_vim->mode(), VimHandler::Mode::Command);
    sendKey(Qt::Key_Escape);
    QCOMPARE(m_vim->mode(), VimHandler::Mode::Normal);
}

void TestVimHandler::vEntersVisual()
{
    sendChar(QLatin1Char('v'));
    QCOMPARE(m_vim->mode(), VimHandler::Mode::Visual);
    sendKey(Qt::Key_Escape);
    QCOMPARE(m_vim->mode(), VimHandler::Mode::Normal);
}

void TestVimHandler::shiftV_entersVisualLine()
{
    sendKey(Qt::Key_V, Qt::ShiftModifier, QStringLiteral("V"));
    QCOMPARE(m_vim->mode(), VimHandler::Mode::VisualLine);
}

void TestVimHandler::modeStringValues()
{
    QCOMPARE(m_vim->modeString(), QStringLiteral("NORMAL"));
    sendChar(QLatin1Char('i'));
    QCOMPARE(m_vim->modeString(), QStringLiteral("INSERT"));
    sendKey(Qt::Key_Escape);
    sendChar(QLatin1Char('v'));
    QCOMPARE(m_vim->modeString(), QStringLiteral("VISUAL"));
}

// ── Basic motions ────────────────────────────────────────────────────────────

void TestVimHandler::h_movesLeft()
{
    sendChar(QLatin1Char('l')); // move right first
    int pos = m_editor->textCursor().position();
    sendChar(QLatin1Char('h'));
    QCOMPARE(m_editor->textCursor().position(), pos - 1);
}

void TestVimHandler::j_movesDown()
{
    int startBlock = m_editor->textCursor().blockNumber();
    sendChar(QLatin1Char('j'));
    QCOMPARE(m_editor->textCursor().blockNumber(), startBlock + 1);
}

void TestVimHandler::k_movesUp()
{
    sendChar(QLatin1Char('j')); // go down first
    sendChar(QLatin1Char('k'));
    QCOMPARE(m_editor->textCursor().blockNumber(), 0);
}

void TestVimHandler::l_movesRight()
{
    sendChar(QLatin1Char('l'));
    QCOMPARE(m_editor->textCursor().position(), 1);
}

void TestVimHandler::h_stopsAtLineStart()
{
    sendChar(QLatin1Char('h'));
    QCOMPARE(m_editor->textCursor().position(), 0);
}

void TestVimHandler::l_stopsAtLineEnd()
{
    // Move to end of "hello world" (11 chars, positions 0-10)
    for (int i = 0; i < 20; ++i)
        sendChar(QLatin1Char('l'));
    QVERIFY(m_editor->textCursor().atBlockEnd());
}

// ── Word motions ─────────────────────────────────────────────────────────────

void TestVimHandler::w_movesToNextWord()
{
    // At start of "hello world", w should go to "world"
    sendChar(QLatin1Char('w'));
    QCOMPARE(m_editor->textCursor().position(), 6); // "world" starts at 6
}

void TestVimHandler::b_movesToPrevWord()
{
    sendChar(QLatin1Char('w')); // go to "world"
    sendChar(QLatin1Char('b')); // back to "hello"
    QCOMPARE(m_editor->textCursor().position(), 0);
}

void TestVimHandler::e_movesToWordEnd()
{
    // At start of "hello", e should go to 'o' (pos 4)
    sendChar(QLatin1Char('e'));
    QCOMPARE(m_editor->textCursor().position(), 4);
}

// ── Line motions ─────────────────────────────────────────────────────────────

void TestVimHandler::zero_movesToLineStart()
{
    sendChar(QLatin1Char('l'));
    sendChar(QLatin1Char('l'));
    sendKey(Qt::Key_0, Qt::NoModifier, QStringLiteral("0"));
    QCOMPARE(m_editor->textCursor().positionInBlock(), 0);
}

void TestVimHandler::dollar_movesToLineEnd()
{
    sendKey(Qt::Key_4, Qt::ShiftModifier, QStringLiteral("$"));
    QVERIFY(m_editor->textCursor().atBlockEnd());
}

// ── Document motions ─────────────────────────────────────────────────────────

void TestVimHandler::gg_movesToDocStart()
{
    sendChar(QLatin1Char('j'));
    sendChar(QLatin1Char('j'));
    sendChar(QLatin1Char('g'));
    sendChar(QLatin1Char('g'));
    QCOMPARE(m_editor->textCursor().position(), 0);
}

void TestVimHandler::G_movesToDocEnd()
{
    sendKey(Qt::Key_G, Qt::ShiftModifier, QStringLiteral("G"));
    QVERIFY(m_editor->textCursor().atEnd());
}

void TestVimHandler::countG_goesToLine()
{
    // 2G should go to line 2
    sendKey(Qt::Key_2, Qt::NoModifier, QStringLiteral("2"));
    sendKey(Qt::Key_G, Qt::ShiftModifier, QStringLiteral("G"));
    QCOMPARE(m_editor->textCursor().blockNumber(), 1);
}

// ── Bracket matching ─────────────────────────────────────────────────────────

void TestVimHandler::percent_matchesBracket()
{
    m_editor->setPlainText(QStringLiteral("(hello)"));
    auto tc = m_editor->textCursor();
    tc.setPosition(0);
    m_editor->setTextCursor(tc);

    sendKey(Qt::Key_Percent, Qt::ShiftModifier, QStringLiteral("%"));
    QCOMPARE(m_editor->textCursor().position(), 6); // closing )
}

// ── Insert mode transitions ─────────────────────────────────────────────────

void TestVimHandler::a_insertsAfterCursor()
{
    int pos = m_editor->textCursor().position();
    sendChar(QLatin1Char('a'));
    QCOMPARE(m_vim->mode(), VimHandler::Mode::Insert);
    QCOMPARE(m_editor->textCursor().position(), pos + 1);
}

void TestVimHandler::A_insertsAtLineEnd()
{
    sendKey(Qt::Key_A, Qt::ShiftModifier, QStringLiteral("A"));
    QCOMPARE(m_vim->mode(), VimHandler::Mode::Insert);
    QVERIFY(m_editor->textCursor().atBlockEnd());
}

void TestVimHandler::I_insertsAtFirstNonBlank()
{
    m_editor->setPlainText(QStringLiteral("    indented"));
    auto tc = m_editor->textCursor();
    tc.setPosition(8); // middle
    m_editor->setTextCursor(tc);

    sendKey(Qt::Key_I, Qt::ShiftModifier, QStringLiteral("I"));
    QCOMPARE(m_vim->mode(), VimHandler::Mode::Insert);
    QCOMPARE(m_editor->textCursor().positionInBlock(), 4); // skip 4 spaces
}

void TestVimHandler::o_opensLineBelow()
{
    sendChar(QLatin1Char('o'));
    QCOMPARE(m_vim->mode(), VimHandler::Mode::Insert);
    QCOMPARE(m_editor->textCursor().blockNumber(), 1);
}

void TestVimHandler::O_opensLineAbove()
{
    sendChar(QLatin1Char('j')); // go to line 2
    sendKey(Qt::Key_O, Qt::ShiftModifier, QStringLiteral("O"));
    QCOMPARE(m_vim->mode(), VimHandler::Mode::Insert);
    // Should be on a new empty line above line 2
    QCOMPARE(m_editor->textCursor().blockNumber(), 1);
    QVERIFY(m_editor->textCursor().block().text().isEmpty());
}

// ── Operators ────────────────────────────────────────────────────────────────

void TestVimHandler::x_deletesChar()
{
    // Delete 'h' from "hello world"
    sendChar(QLatin1Char('x'));
    QVERIFY(m_editor->toPlainText().startsWith(QStringLiteral("ello world")));
}

void TestVimHandler::dd_deletesLine()
{
    sendChar(QLatin1Char('d'));
    sendChar(QLatin1Char('d'));
    QVERIFY(m_editor->toPlainText().startsWith(QStringLiteral("foo bar baz")));
}

void TestVimHandler::dw_deletesWord()
{
    // dw at "hello world" should delete "hello "
    sendChar(QLatin1Char('d'));
    sendChar(QLatin1Char('w'));
    QVERIFY(m_editor->toPlainText().startsWith(QStringLiteral("world")));
}

void TestVimHandler::yy_yanksLine()
{
    sendChar(QLatin1Char('y'));
    sendChar(QLatin1Char('y'));
    // Cursor stays, text unchanged
    QVERIFY(m_editor->toPlainText().startsWith(QStringLiteral("hello world")));
    // After p, line should be duplicated
    sendChar(QLatin1Char('p'));
    int helloCount = m_editor->toPlainText().count(QStringLiteral("hello world"));
    QCOMPARE(helloCount, 2);
}

void TestVimHandler::p_pastesAfter()
{
    // yy then p duplicates line
    sendChar(QLatin1Char('y'));
    sendChar(QLatin1Char('y'));
    sendChar(QLatin1Char('p'));
    QCOMPARE(m_editor->toPlainText().count(QStringLiteral("hello world")), 2);
}

void TestVimHandler::P_pastesBefore()
{
    sendChar(QLatin1Char('j')); // line 2
    sendChar(QLatin1Char('y'));
    sendChar(QLatin1Char('y'));
    sendKey(Qt::Key_P, Qt::ShiftModifier, QStringLiteral("P"));
    // "foo bar baz" should now appear before line 2
    const QStringList lines = m_editor->toPlainText().split(QLatin1Char('\n'));
    QCOMPARE(lines.at(1), QStringLiteral("foo bar baz"));
}

void TestVimHandler::cc_changesLine()
{
    sendChar(QLatin1Char('c'));
    sendChar(QLatin1Char('c'));
    QCOMPARE(m_vim->mode(), VimHandler::Mode::Insert);
    // First line content should be deleted
    QVERIFY(m_editor->textCursor().block().text().isEmpty() ||
            !m_editor->toPlainText().startsWith(QStringLiteral("hello")));
}

// ── Numeric prefix ───────────────────────────────────────────────────────────

void TestVimHandler::count_motions()
{
    // 3l should move 3 right
    sendKey(Qt::Key_3, Qt::NoModifier, QStringLiteral("3"));
    sendChar(QLatin1Char('l'));
    QCOMPARE(m_editor->textCursor().position(), 3);
}

void TestVimHandler::count_dd()
{
    // 2dd should delete 2 lines
    sendKey(Qt::Key_2, Qt::NoModifier, QStringLiteral("2"));
    sendChar(QLatin1Char('d'));
    sendChar(QLatin1Char('d'));
    QVERIFY(m_editor->toPlainText().startsWith(QStringLiteral("third line")));
}

// ── Undo/Redo ────────────────────────────────────────────────────────────────

void TestVimHandler::u_undoes()
{
    const QString original = m_editor->toPlainText();
    sendChar(QLatin1Char('x')); // delete a char
    sendChar(QLatin1Char('u')); // undo
    QCOMPARE(m_editor->toPlainText(), original);
}

void TestVimHandler::ctrlR_redoes()
{
    const QString original = m_editor->toPlainText();
    sendChar(QLatin1Char('x')); // delete a char
    const QString afterDelete = m_editor->toPlainText();
    sendChar(QLatin1Char('u')); // undo
    QCOMPARE(m_editor->toPlainText(), original);
    sendKey(Qt::Key_R, Qt::ControlModifier); // redo
    QCOMPARE(m_editor->toPlainText(), afterDelete);
}

// ── Visual mode ──────────────────────────────────────────────────────────────

void TestVimHandler::visual_d_deletesSelection()
{
    sendChar(QLatin1Char('v'));
    sendChar(QLatin1Char('l'));
    sendChar(QLatin1Char('l'));
    sendChar(QLatin1Char('l'));
    sendChar(QLatin1Char('l'));
    sendChar(QLatin1Char('d'));
    // 4 l-presses select chars 0-3 ("hell"), leaving "o world..."
    QVERIFY(m_editor->toPlainText().startsWith(QStringLiteral("o world")));
    QCOMPARE(m_vim->mode(), VimHandler::Mode::Normal);
}

void TestVimHandler::visual_y_yanksSelection()
{
    sendChar(QLatin1Char('v'));
    sendChar(QLatin1Char('l'));
    sendChar(QLatin1Char('l'));
    sendChar(QLatin1Char('l'));
    sendChar(QLatin1Char('l'));
    sendChar(QLatin1Char('y'));
    QCOMPARE(m_vim->mode(), VimHandler::Mode::Normal);
    // Text unchanged
    QVERIFY(m_editor->toPlainText().startsWith(QStringLiteral("hello world")));
}

void TestVimHandler::visual_c_changesSelection()
{
    sendChar(QLatin1Char('v'));
    sendChar(QLatin1Char('l'));
    sendChar(QLatin1Char('l'));
    sendChar(QLatin1Char('c'));
    QCOMPARE(m_vim->mode(), VimHandler::Mode::Insert);
    // 2 l-presses select chars 0-1 ("he"), leaving "llo world..."
    QVERIFY(m_editor->toPlainText().startsWith(QStringLiteral("llo world")));
}

// ── Ex commands ──────────────────────────────────────────────────────────────

void TestVimHandler::exCommand_w_emitsSave()
{
    QSignalSpy spy(m_vim, &VimHandler::saveRequested);
    sendKey(Qt::Key_Colon);
    sendChar(QLatin1Char('w'));
    sendKey(Qt::Key_Return);
    QCOMPARE(spy.count(), 1);
}

void TestVimHandler::exCommand_q_emitsQuit()
{
    QSignalSpy spy(m_vim, &VimHandler::quitRequested);
    sendKey(Qt::Key_Colon);
    sendChar(QLatin1Char('q'));
    sendKey(Qt::Key_Return);
    QCOMPARE(spy.count(), 1);
}

void TestVimHandler::exCommand_wq_emitsBoth()
{
    QSignalSpy saveSpy(m_vim, &VimHandler::saveRequested);
    QSignalSpy quitSpy(m_vim, &VimHandler::quitRequested);
    sendKey(Qt::Key_Colon);
    sendChar(QLatin1Char('w'));
    sendChar(QLatin1Char('q'));
    sendKey(Qt::Key_Return);
    QCOMPARE(saveSpy.count(), 1);
    QCOMPARE(quitSpy.count(), 1);
}

void TestVimHandler::exCommand_lineNumber()
{
    sendKey(Qt::Key_Colon);
    sendKey(Qt::Key_3, Qt::NoModifier, QStringLiteral("3"));
    sendKey(Qt::Key_Return);
    QCOMPARE(m_editor->textCursor().blockNumber(), 2); // 0-indexed
}

void TestVimHandler::exCommand_e_emitsOpen()
{
    QSignalSpy spy(m_vim, &VimHandler::openFileRequested);
    sendKey(Qt::Key_Colon);
    sendChar(QLatin1Char('e'));
    sendChar(QLatin1Char(' '));
    sendChar(QLatin1Char('f'));
    sendChar(QLatin1Char('o'));
    sendChar(QLatin1Char('o'));
    sendKey(Qt::Key_Return);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toString(), QStringLiteral("foo"));
}

// ── Insert passthrough ───────────────────────────────────────────────────────

void TestVimHandler::insert_passesNormalKeys()
{
    sendChar(QLatin1Char('i'));
    QCOMPARE(m_vim->mode(), VimHandler::Mode::Insert);
    // Normal keys should NOT be handled by VimHandler (returns false)
    QKeyEvent press(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
    bool handled = m_vim->handleKeyPress(&press);
    QVERIFY(!handled);
}

QTEST_MAIN(TestVimHandler)
#include "test_vimhandler.moc"
