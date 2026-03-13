#include <QTest>
#include <QSignalSpy>

#include "terminal/terminalscreen.h"

class TestTerminalScreen : public QObject
{
    Q_OBJECT

private slots:

    // ── Construction ──────────────────────────────────────────────────────

    void defaultSize()
    {
        TerminalScreen ts;
        QCOMPARE(ts.cols(), 80);
        QCOMPARE(ts.rows(), 24);
        QCOMPARE(ts.cursorPos(), QPoint(0, 0));
        QVERIFY(ts.cursorVisible());
        QCOMPARE(ts.scrollbackLines(), 0);
    }

    void customSize()
    {
        TerminalScreen ts(120, 40);
        QCOMPARE(ts.cols(), 120);
        QCOMPARE(ts.rows(), 40);
    }

    // ── Basic text output ─────────────────────────────────────────────────

    void feedASCII()
    {
        TerminalScreen ts(10, 5);
        ts.feed("Hello");
        QCOMPARE(ts.cellAt(0, 0).cp, U'H');
        QCOMPARE(ts.cellAt(1, 0).cp, U'e');
        QCOMPARE(ts.cellAt(4, 0).cp, U'o');
        QCOMPARE(ts.cursorPos(), QPoint(5, 0));
    }

    void feedUTF8()
    {
        TerminalScreen ts(20, 5);
        ts.feed(QString::fromUtf8("café").toUtf8());
        QCOMPARE(ts.cellAt(0, 0).cp, U'c');
        QCOMPARE(ts.cellAt(1, 0).cp, U'a');
        QCOMPARE(ts.cellAt(2, 0).cp, U'f');
        QCOMPARE(ts.cellAt(3, 0).cp, U'é');
    }

    // ── Line wrap ─────────────────────────────────────────────────────────

    void lineWrap()
    {
        TerminalScreen ts(5, 3);
        ts.feed("ABCDE"); // fills row 0
        QCOMPARE(ts.cursorPos().x(), 4); // at last col, wrap pending
        ts.feed("F"); // wraps to row 1
        QCOMPARE(ts.cellAt(0, 1).cp, U'F');
        QCOMPARE(ts.cursorPos(), QPoint(1, 1));
    }

    // ── Newline / CR ──────────────────────────────────────────────────────

    void carriageReturn()
    {
        TerminalScreen ts(10, 5);
        ts.feed("ABC\rX");
        QCOMPARE(ts.cellAt(0, 0).cp, U'X');
        QCOMPARE(ts.cellAt(1, 0).cp, U'B');
    }

    void lineFeed()
    {
        TerminalScreen ts(10, 5);
        ts.feed("AB\nCD");
        QCOMPARE(ts.cellAt(0, 0).cp, U'A');
        QCOMPARE(ts.cellAt(1, 0).cp, U'B');
        QCOMPARE(ts.cellAt(2, 1).cp, U'C');
        QCOMPARE(ts.cellAt(3, 1).cp, U'D');
    }

    void crLf()
    {
        TerminalScreen ts(10, 5);
        ts.feed("AB\r\nCD");
        QCOMPARE(ts.cellAt(0, 0).cp, U'A');
        QCOMPARE(ts.cellAt(0, 1).cp, U'C');
    }

    // ── Backspace ─────────────────────────────────────────────────────────

    void backspace()
    {
        TerminalScreen ts(10, 5);
        ts.feed("AB\bX");
        QCOMPARE(ts.cellAt(0, 0).cp, U'A');
        QCOMPARE(ts.cellAt(1, 0).cp, U'X'); // B overwritten
    }

    void backspaceAtStart()
    {
        TerminalScreen ts(10, 5);
        ts.feed(QByteArray("\b")); // at col 0 — no-op
        QCOMPARE(ts.cursorPos(), QPoint(0, 0));
    }

    // ── Tab ───────────────────────────────────────────────────────────────

    void horizontalTab()
    {
        TerminalScreen ts(80, 5);
        ts.feed("A\t");
        QCOMPARE(ts.cursorPos().x(), 8); // next tab stop
    }

    void horizontalTabMultiple()
    {
        TerminalScreen ts(80, 5);
        ts.feed("\t\t");
        QCOMPARE(ts.cursorPos().x(), 16);
    }

    // ── CSI cursor movement ───────────────────────────────────────────────

    void csiCursorUp()
    {
        TerminalScreen ts(10, 10);
        ts.feed("\n\n\n"); // row 3
        ts.feed("\x1b[2A"); // move up 2
        QCOMPARE(ts.cursorPos().y(), 1);
    }

    void csiCursorDown()
    {
        TerminalScreen ts(10, 10);
        ts.feed("\x1b[3B"); // move down 3
        QCOMPARE(ts.cursorPos().y(), 3);
    }

    void csiCursorForward()
    {
        TerminalScreen ts(10, 5);
        ts.feed("\x1b[5C"); // move right 5
        QCOMPARE(ts.cursorPos().x(), 5);
    }

    void csiCursorBack()
    {
        TerminalScreen ts(10, 5);
        ts.feed("ABCDE\x1b[3D"); // right 5, back 3
        QCOMPARE(ts.cursorPos().x(), 2);
    }

    void csiCursorPosition()
    {
        TerminalScreen ts(80, 24);
        ts.feed("\x1b[5;10H"); // row 5, col 10 (1-based)
        QCOMPARE(ts.cursorPos(), QPoint(9, 4)); // 0-based
    }

    void csiCursorPosition_default()
    {
        TerminalScreen ts(80, 24);
        ts.feed("XXXXXXXXXX\x1b[H"); // CUP with no args → home
        QCOMPARE(ts.cursorPos(), QPoint(0, 0));
    }

    // ── Erase ─────────────────────────────────────────────────────────────

    void eraseToEndOfLine()
    {
        TerminalScreen ts(10, 5);
        ts.feed("ABCDEFGHIJ");
        ts.feed("\x1b[H\x1b[5C"); // go to col 5
        ts.feed("\x1b[K"); // erase from cursor to end (mode 0)
        QCOMPARE(ts.cellAt(4, 0).cp, U'E');
        QCOMPARE(ts.cellAt(5, 0).cp, U' ');
        QCOMPARE(ts.cellAt(9, 0).cp, U' ');
    }

    void eraseToBeginOfLine()
    {
        TerminalScreen ts(10, 5);
        ts.feed("ABCDEFGHIJ");
        ts.feed("\x1b[H\x1b[5C"); // col 5
        ts.feed("\x1b[1K"); // erase from start to cursor
        QCOMPARE(ts.cellAt(0, 0).cp, U' ');
        QCOMPARE(ts.cellAt(5, 0).cp, U' ');
        QCOMPARE(ts.cellAt(6, 0).cp, U'G');
    }

    void eraseEntireLine()
    {
        TerminalScreen ts(10, 5);
        ts.feed("ABCDEFGHIJ\x1b[H\x1b[2K");
        for (int c = 0; c < 10; ++c)
            QCOMPARE(ts.cellAt(c, 0).cp, U' ');
    }

    void eraseDisplay_below()
    {
        TerminalScreen ts(5, 3);
        ts.feed("AAAAA\r\nBBBBB\r\nCCCCC");
        ts.feed("\x1b[2;1H"); // row 2 col 1 (1-based)
        ts.feed("\x1b[J"); // erase below
        QCOMPARE(ts.cellAt(0, 0).cp, U'A');
        QCOMPARE(ts.cellAt(0, 1).cp, U' ');
        QCOMPARE(ts.cellAt(0, 2).cp, U' ');
    }

    void eraseDisplay_all()
    {
        TerminalScreen ts(5, 3);
        ts.feed("AAAAA\nBBBBB\nCCCCC");
        ts.feed("\x1b[2J"); // erase all
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 5; ++c)
                QCOMPARE(ts.cellAt(c, r).cp, U' ');
    }

    // ── SGR (colors & attributes) ─────────────────────────────────────────

    void sgrBold()
    {
        TerminalScreen ts(10, 5);
        ts.feed("\x1b[1mX");
        QVERIFY(ts.cellAt(0, 0).bold);
    }

    void sgrUnderline()
    {
        TerminalScreen ts(10, 5);
        ts.feed("\x1b[4mX");
        QVERIFY(ts.cellAt(0, 0).ul);
    }

    void sgrReverse()
    {
        TerminalScreen ts(10, 5);
        ts.feed("\x1b[7mX");
        QVERIFY(ts.cellAt(0, 0).rev);
    }

    void sgrReset()
    {
        TerminalScreen ts(10, 5);
        ts.feed("\x1b[1;4;7m\x1b[0mX");
        QVERIFY(!ts.cellAt(0, 0).bold);
        QVERIFY(!ts.cellAt(0, 0).ul);
        QVERIFY(!ts.cellAt(0, 0).rev);
    }

    void sgrForegroundColor()
    {
        TerminalScreen ts(10, 5);
        ts.feed("\x1b[31mX"); // red
        QCOMPARE(ts.cellAt(0, 0).fg, (QRgb)qRgba(128, 0, 0, 255));
    }

    void sgrBackgroundColor()
    {
        TerminalScreen ts(10, 5);
        ts.feed("\x1b[42mX"); // green bg
        QCOMPARE(ts.cellAt(0, 0).bg, (QRgb)qRgba(0, 128, 0, 255));
    }

    void sgr256Color()
    {
        TerminalScreen ts(10, 5);
        ts.feed("\x1b[38;5;9mX"); // bright red
        QCOMPARE(ts.cellAt(0, 0).fg, (QRgb)qRgba(255, 85, 85, 255));
    }

    void sgrTrueColor()
    {
        TerminalScreen ts(10, 5);
        ts.feed("\x1b[38;2;100;150;200mX");
        QCOMPARE(ts.cellAt(0, 0).fg, (QRgb)qRgba(100, 150, 200, 255));
    }

    void sgrBrightFg()
    {
        TerminalScreen ts(10, 5);
        ts.feed("\x1b[91mX"); // bright red
        QCOMPARE(ts.cellAt(0, 0).fg, (QRgb)qRgba(255, 85, 85, 255));
    }

    void sgrDefaultFg()
    {
        TerminalScreen ts(10, 5);
        ts.feed("\x1b[31m\x1b[39mX"); // set red, then default
        QCOMPARE(ts.cellAt(0, 0).fg, kTermDefaultFg);
    }

    // ── Scrolling ─────────────────────────────────────────────────────────

    void scrollUp_addsToScrollback()
    {
        TerminalScreen ts(5, 3);
        ts.feed("AAA\r\nBBB\r\nCCC\r\nDDD"); // 4 lines in 3-row screen → scroll
        QVERIFY(ts.scrollbackLines() > 0);
        // First scrollback row was the original row 0
        const TermCell cell = ts.cellAt(0, -1);
        QCOMPARE(cell.cp, U'A');
    }

    void scrollDown_csi()
    {
        TerminalScreen ts(5, 5);
        ts.feed("AAA\nBBB\nCCC");
        ts.feed("\x1b[T"); // scroll down 1
        QCOMPARE(ts.cellAt(0, 0).cp, U' '); // blank row inserted at top
    }

    // ── Resize ────────────────────────────────────────────────────────────

    void resizeGrow()
    {
        TerminalScreen ts(5, 3);
        ts.feed("ABC");

        QSignalSpy spy(&ts, &TerminalScreen::damaged);
        ts.resize(10, 5);
        QCOMPARE(ts.cols(), 10);
        QCOMPARE(ts.rows(), 5);
        QCOMPARE(ts.cellAt(0, 0).cp, U'A');
        QCOMPARE(spy.count(), 1);
    }

    void resizeShrink()
    {
        TerminalScreen ts(10, 5);
        ts.feed("ABCDEFGHIJ");
        ts.resize(5, 3);
        QCOMPARE(ts.cols(), 5);
        QCOMPARE(ts.rows(), 3);
        // Cursor clamped
        QVERIFY(ts.cursorPos().x() < 5);
        QVERIFY(ts.cursorPos().y() < 3);
    }

    void resizeNoOp()
    {
        TerminalScreen ts(80, 24);
        QSignalSpy spy(&ts, &TerminalScreen::damaged);
        ts.resize(80, 24); // same size
        QCOMPARE(spy.count(), 0);
    }

    // ── OSC title ─────────────────────────────────────────────────────────

    void oscTitle()
    {
        TerminalScreen ts(80, 24);
        QSignalSpy spy(&ts, &TerminalScreen::titleChanged);
        ts.feed("\x1b]2;My Terminal\x07"); // OSC 2 + BEL
        QCOMPARE(ts.title(), QStringLiteral("My Terminal"));
        QCOMPARE(spy.count(), 1);
    }

    void oscTitle_escST()
    {
        TerminalScreen ts(80, 24);
        // OSC 0 ; title ESC \ (ST) — the ESC starts a new sequence,
        // then '\\' is the String Terminator
        QByteArray seq;
        seq.append("\x1b]0;Window Title");
        seq.append("\x1b\\"); // ESC + backslash as ST
        ts.feed(seq);
        QCOMPARE(ts.title(), QStringLiteral("Window Title"));
    }

    // ── Bell ──────────────────────────────────────────────────────────────

    void bellSignal()
    {
        TerminalScreen ts(80, 24);
        QSignalSpy spy(&ts, &TerminalScreen::bell);
        ts.feed("\x07");
        QCOMPARE(spy.count(), 1);
    }

    // ── Cursor visibility ─────────────────────────────────────────────────

    void cursorHideShow()
    {
        TerminalScreen ts(80, 24);
        QVERIFY(ts.cursorVisible());
        ts.feed("\x1b[?25l"); // hide
        QVERIFY(!ts.cursorVisible());
        ts.feed("\x1b[?25h"); // show
        QVERIFY(ts.cursorVisible());
    }

    // ── Alt screen ────────────────────────────────────────────────────────

    void alternateScreen()
    {
        TerminalScreen ts(10, 5);
        ts.feed("PRIMARY");
        ts.feed("\x1b[?1049h"); // enter alt screen (saves cursor + erases)
        // Cursor position carries over; move to origin for clean write
        ts.feed("\x1b[H");
        ts.feed("ALT");
        QCOMPARE(ts.cellAt(0, 0).cp, U'A');
        QCOMPARE(ts.cellAt(1, 0).cp, U'L');
        QCOMPARE(ts.cellAt(2, 0).cp, U'T');
        ts.feed("\x1b[?1049l"); // exit alt screen
        // Primary screen content should be restored
        QCOMPARE(ts.cellAt(0, 0).cp, U'P');
        QCOMPARE(ts.cellAt(6, 0).cp, U'Y');
    }

    // ── Save/restore cursor ───────────────────────────────────────────────

    void saveRestoreCursor()
    {
        TerminalScreen ts(80, 24);
        ts.feed("\x1b[5;10H"); // move to (9,4)
        ts.feed("\x1b[s");     // save
        ts.feed("\x1b[1;1H");  // home
        ts.feed("\x1b[u");     // restore
        QCOMPARE(ts.cursorPos(), QPoint(9, 4));
    }

    void saveRestoreCursor_escSeq()
    {
        TerminalScreen ts(80, 24);
        ts.feed("\x1b[3;7H"); // move to (6,2)
        ts.feed("\x1b""7"); // ESC 7 = save
        ts.feed("\x1b[1;1H");
        ts.feed("\x1b""8"); // ESC 8 = restore
        QCOMPARE(ts.cursorPos(), QPoint(6, 2));
    }

    // ── Insert/Delete lines ───────────────────────────────────────────────

    void insertLine()
    {
        TerminalScreen ts(5, 4);
        ts.feed("AAAA\r\nBBBB\r\nCCCC\r\nDDDD");
        ts.feed("\x1b[2;1H"); // row 2
        ts.feed("\x1b[L");    // insert 1 line
        QCOMPARE(ts.cellAt(0, 0).cp, U'A');
        QCOMPARE(ts.cellAt(0, 1).cp, U' '); // blank inserted
        QCOMPARE(ts.cellAt(0, 2).cp, U'B');
    }

    void deleteLine()
    {
        TerminalScreen ts(5, 4);
        ts.feed("AAAA\r\nBBBB\r\nCCCC\r\nDDDD");
        ts.feed("\x1b[2;1H"); // row 2
        ts.feed("\x1b[M");    // delete 1 line
        QCOMPARE(ts.cellAt(0, 0).cp, U'A');
        QCOMPARE(ts.cellAt(0, 1).cp, U'C'); // B deleted
    }

    // ── Delete char / Insert char ─────────────────────────────────────────

    void deleteChar()
    {
        TerminalScreen ts(10, 5);
        ts.feed("ABCDE\x1b[1;2H"); // col 2 (0-based: 1)
        ts.feed("\x1b[P");          // delete 1 char
        QCOMPARE(ts.cellAt(0, 0).cp, U'A');
        QCOMPARE(ts.cellAt(1, 0).cp, U'C');
        QCOMPARE(ts.cellAt(2, 0).cp, U'D');
    }

    void insertChar()
    {
        TerminalScreen ts(10, 5);
        ts.feed("ABCDE\x1b[1;2H"); // col 2
        ts.feed("\x1b[@");          // insert 1 char
        QCOMPARE(ts.cellAt(0, 0).cp, U'A');
        QCOMPARE(ts.cellAt(1, 0).cp, U' '); // blank inserted
        QCOMPARE(ts.cellAt(2, 0).cp, U'B');
    }

    // ── Erase char ────────────────────────────────────────────────────────

    void eraseChar()
    {
        TerminalScreen ts(10, 5);
        ts.feed("ABCDE\x1b[1;2H");
        ts.feed("\x1b[2X"); // erase 2 chars at cursor
        QCOMPARE(ts.cellAt(0, 0).cp, U'A');
        QCOMPARE(ts.cellAt(1, 0).cp, U' ');
        QCOMPARE(ts.cellAt(2, 0).cp, U' ');
        QCOMPARE(ts.cellAt(3, 0).cp, U'D');
    }

    // ── Scroll region ─────────────────────────────────────────────────────

    void scrollRegion()
    {
        TerminalScreen ts(10, 5);
        // Use CUP to place text on each row explicitly
        ts.feed("\x1b[1;1H" "11111");
        ts.feed("\x1b[2;1H" "22222");
        ts.feed("\x1b[3;1H" "33333");
        ts.feed("\x1b[4;1H" "44444");
        ts.feed("\x1b[5;1H" "55555");
        ts.feed("\x1b[2;4r");     // set scroll region rows 2–4
        ts.feed("\x1b[4;1H");     // move to row 4 (bottom of region)
        ts.feed("\n");            // LF in region → scroll within region
        QCOMPARE(ts.cellAt(0, 0).cp, U'1'); // row 1 unaffected
        QCOMPARE(ts.cellAt(0, 4).cp, U'5'); // row 5 unaffected
    }

    // ── Full reset ────────────────────────────────────────────────────────

    void fullReset()
    {
        TerminalScreen ts(10, 5);
        ts.feed("ABCDE\x1b[1;4;7m");
        ts.feed("\x1b""c"); // ESC c = RIS
        QCOMPARE(ts.cursorPos(), QPoint(0, 0));
        for (int c = 0; c < 10; ++c)
            QCOMPARE(ts.cellAt(c, 0).cp, U' ');
    }

    // ── recentText / recentOutput ─────────────────────────────────────────

    void recentText_basic()
    {
        TerminalScreen ts(10, 3);
        ts.feed("Hello\r\nWorld");
        const QString text = ts.recentText(5);
        QVERIFY(text.contains("Hello"));
        QVERIFY(text.contains("World"));
    }

    void recentOutput_basic()
    {
        TerminalScreen ts(10, 3);
        ts.feed("Line1\r\nLine2\r\nLine3");
        const QString out = ts.recentOutput(2);
        QVERIFY(out.contains("Line2") || out.contains("Line3"));
    }

    void recentText_zeroLines()
    {
        TerminalScreen ts(10, 3);
        ts.feed("Hello");
        QVERIFY(ts.recentText(0).isEmpty());
    }

    // ── Out of bounds cellAt ──────────────────────────────────────────────

    void cellAt_outOfBounds()
    {
        TerminalScreen ts(10, 5);
        TermCell cell = ts.cellAt(-1, 0);
        QCOMPARE(cell.cp, U' '); // default TermCell
        cell = ts.cellAt(100, 0);
        QCOMPARE(cell.cp, U' ');
        cell = ts.cellAt(0, 100);
        QCOMPARE(cell.cp, U' ');
    }

    // ── Damaged signal fires on feed ──────────────────────────────────────

    void damagedSignal()
    {
        TerminalScreen ts(10, 5);
        QSignalSpy spy(&ts, &TerminalScreen::damaged);
        ts.feed("test");
        QCOMPARE(spy.count(), 1);
    }

    // ── CNL / CPL ─────────────────────────────────────────────────────────

    void csiCNL()
    {
        TerminalScreen ts(10, 10);
        ts.feed("XXXX\x1b[2E"); // CNL: move down 2, col 0
        QCOMPARE(ts.cursorPos().x(), 0);
        QCOMPARE(ts.cursorPos().y(), 2);
    }

    void csiCPL()
    {
        TerminalScreen ts(10, 10);
        ts.feed("\n\n\nXXX\x1b[2F"); // CPL: move up 2, col 0
        QCOMPARE(ts.cursorPos().x(), 0);
        QCOMPARE(ts.cursorPos().y(), 1);
    }

    // ── CHA (absolute column) ─────────────────────────────────────────────

    void csiCHA()
    {
        TerminalScreen ts(80, 24);
        ts.feed("ABCDE\x1b[3G"); // CHA: col 3 (1-based)
        QCOMPARE(ts.cursorPos().x(), 2);
    }
};

QTEST_MAIN(TestTerminalScreen)
#include "test_terminalscreen.moc"
