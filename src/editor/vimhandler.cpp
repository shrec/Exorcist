#include "vimhandler.h"

#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>

VimHandler::VimHandler(QPlainTextEdit *editor, QObject *parent)
    : QObject(parent), m_editor(editor)
{
}

bool VimHandler::isEnabled() const { return m_enabled; }

void VimHandler::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (enabled) {
        setMode(Mode::Normal);
        m_editor->setCursorWidth(m_editor->fontMetrics().averageCharWidth());
    } else {
        m_editor->setCursorWidth(1);
    }
}

VimHandler::Mode VimHandler::mode() const { return m_mode; }

QString VimHandler::modeString() const
{
    switch (m_mode) {
    case Mode::Normal:     return tr("NORMAL");
    case Mode::Insert:     return tr("INSERT");
    case Mode::Visual:     return tr("VISUAL");
    case Mode::VisualLine: return tr("V-LINE");
    case Mode::Command:    return tr("COMMAND");
    }
    return {};
}

bool VimHandler::handleKeyPress(QKeyEvent *event)
{
    if (!m_enabled) return false;

    switch (m_mode) {
    case Mode::Normal:     return handleNormal(event);
    case Mode::Insert:     return handleInsert(event);
    case Mode::Visual:
    case Mode::VisualLine: return handleVisual(event);
    case Mode::Command:    return handleCommand(event);
    }
    return false;
}

void VimHandler::reset()
{
    m_pendingOp = Operator::None;
    m_count = 0;
    m_pendingKeys.clear();
    m_commandLine.clear();
}

void VimHandler::setMode(Mode m)
{
    if (m_mode == m) return;
    m_mode = m;

    if (m == Mode::Normal) {
        m_editor->setCursorWidth(m_editor->fontMetrics().averageCharWidth());
        // Clear selection
        auto tc = m_editor->textCursor();
        tc.clearSelection();
        m_editor->setTextCursor(tc);
    } else if (m == Mode::Insert) {
        m_editor->setCursorWidth(1);
    }

    reset();
    emit modeChanged(m);
}

// ── Normal mode ──────────────────────────────────────────────────────────────

bool VimHandler::handleNormal(QKeyEvent *event)
{
    const int key = event->key();
    const auto mods = event->modifiers();
    const QString text = event->text();

    // Numeric prefix (but '0' alone is start-of-line, not count)
    if (key >= Qt::Key_1 && key <= Qt::Key_9 && mods == Qt::NoModifier) {
        m_count = m_count * 10 + (key - Qt::Key_0);
        return true;
    }
    if (key == Qt::Key_0 && m_count > 0 && mods == Qt::NoModifier) {
        m_count = m_count * 10;
        return true;
    }

    const int count = (m_count > 0) ? m_count : 1;

    // ── Pending "g" prefix ───────────────────────────────────────────────
    if (m_pendingKeys == QStringLiteral("g")) {
        m_pendingKeys.clear();
        if (key == Qt::Key_G) {
            // gg — go to top
            if (m_pendingOp != Operator::None) {
                applyOperator(m_pendingOp, QStringLiteral("gg"));
            } else {
                moveCursor(0);
            }
            m_count = 0;
            return true;
        }
        m_count = 0;
        return true;
    }

    // ── Pending operator — expect motion ─────────────────────────────────
    if (m_pendingOp != Operator::None) {
        // dd / yy / cc — linewise double
        if ((key == Qt::Key_D && m_pendingOp == Operator::Delete) ||
            (key == Qt::Key_Y && m_pendingOp == Operator::Yank) ||
            (key == Qt::Key_C && m_pendingOp == Operator::Change)) {
            applyLinewiseOperator(m_pendingOp, count);
            m_count = 0;
            return true;
        }
        if (key == Qt::Key_G && mods == Qt::NoModifier) {
            m_pendingKeys = QStringLiteral("g");
            return true;
        }
        // Motion after operator
        const QString motionKey = text;
        if (!motionKey.isEmpty()) {
            for (int i = 0; i < count; ++i)
                applyOperator(m_pendingOp, motionKey);
            m_count = 0;
        }
        return true;
    }

    // ── Motions ──────────────────────────────────────────────────────────
    if (key == Qt::Key_H || key == Qt::Key_J || key == Qt::Key_K || key == Qt::Key_L ||
        key == Qt::Key_W || key == Qt::Key_B || key == Qt::Key_E ||
        key == Qt::Key_0 || (key == Qt::Key_4 && mods == Qt::ShiftModifier) || // $
        key == Qt::Key_AsciiCircum) {

        for (int i = 0; i < count; ++i)
            executeMotion(text);
        m_count = 0;
        return true;
    }

    // G — go to end (or line N if count set)
    if (key == Qt::Key_G && mods == Qt::ShiftModifier) {
        auto tc = m_editor->textCursor();
        if (m_count > 0) {
            // Go to line N
            auto blk = m_editor->document()->findBlockByNumber(m_count - 1);
            if (blk.isValid())
                tc.setPosition(blk.position());
        } else {
            tc.movePosition(QTextCursor::End);
        }
        m_editor->setTextCursor(tc);
        m_count = 0;
        return true;
    }

    // g prefix
    if (key == Qt::Key_G && mods == Qt::NoModifier) {
        m_pendingKeys = QStringLiteral("g");
        return true;
    }

    // f / F — find char on line
    if ((key == Qt::Key_F && mods == Qt::NoModifier) ||
        (key == Qt::Key_F && mods == Qt::ShiftModifier)) {
        // Need next char — store state and wait
        m_lastFindForward = (mods == Qt::NoModifier);
        m_pendingKeys = text;
        return true;
    }

    // If pending f/F, consume the target char
    if (m_pendingKeys == QStringLiteral("f") || m_pendingKeys == QStringLiteral("F")) {
        bool forward = (m_pendingKeys == QStringLiteral("f"));
        m_pendingKeys.clear();
        if (!text.isEmpty()) {
            m_lastFindChar = text.at(0);
            m_lastFindForward = forward;
            auto tc = m_editor->textCursor();
            int pos = findCharOnLine(m_lastFindChar, forward, tc.position());
            if (pos >= 0)
                moveCursor(pos);
        }
        m_count = 0;
        return true;
    }

    // % — matching bracket
    if (key == Qt::Key_Percent) {
        auto tc = m_editor->textCursor();
        int pos = matchingBracket(tc.position());
        if (pos >= 0)
            moveCursor(pos);
        m_count = 0;
        return true;
    }

    // Ctrl+D / Ctrl+U — half page scroll
    if (key == Qt::Key_D && mods == Qt::ControlModifier) {
        auto tc = m_editor->textCursor();
        int halfPage = m_editor->height() / m_editor->fontMetrics().lineSpacing() / 2;
        tc.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, halfPage * count);
        m_editor->setTextCursor(tc);
        m_count = 0;
        return true;
    }
    if (key == Qt::Key_U && mods == Qt::ControlModifier) {
        auto tc = m_editor->textCursor();
        int halfPage = m_editor->height() / m_editor->fontMetrics().lineSpacing() / 2;
        tc.movePosition(QTextCursor::Up, QTextCursor::MoveAnchor, halfPage * count);
        m_editor->setTextCursor(tc);
        m_count = 0;
        return true;
    }

    // ── Operators ────────────────────────────────────────────────────────
    if (key == Qt::Key_D && mods == Qt::NoModifier) {
        m_pendingOp = Operator::Delete;
        return true;
    }
    if (key == Qt::Key_C && mods == Qt::NoModifier) {
        m_pendingOp = Operator::Change;
        return true;
    }
    if (key == Qt::Key_Y && mods == Qt::NoModifier) {
        m_pendingOp = Operator::Yank;
        return true;
    }

    // x — delete char under cursor
    if (key == Qt::Key_X && mods == Qt::NoModifier) {
        auto tc = m_editor->textCursor();
        for (int i = 0; i < count; ++i) {
            if (!tc.atEnd()) {
                tc.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
                m_yankBuffer = tc.selectedText();
                m_yankLinewise = false;
                tc.removeSelectedText();
            }
        }
        m_editor->setTextCursor(tc);
        m_count = 0;
        return true;
    }

    // X — delete char before cursor
    if (key == Qt::Key_X && mods == Qt::ShiftModifier) {
        auto tc = m_editor->textCursor();
        for (int i = 0; i < count; ++i) {
            if (!tc.atBlockStart()) {
                tc.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);
                m_yankBuffer = tc.selectedText();
                m_yankLinewise = false;
                tc.removeSelectedText();
            }
        }
        m_editor->setTextCursor(tc);
        m_count = 0;
        return true;
    }

    // p / P — paste
    if (key == Qt::Key_P && mods == Qt::NoModifier) {
        if (!m_yankBuffer.isEmpty()) {
            auto tc = m_editor->textCursor();
            if (m_yankLinewise) {
                tc.movePosition(QTextCursor::EndOfBlock);
                tc.insertText(QStringLiteral("\n") + m_yankBuffer);
            } else {
                tc.movePosition(QTextCursor::Right);
                tc.insertText(m_yankBuffer);
            }
            m_editor->setTextCursor(tc);
        }
        m_count = 0;
        return true;
    }
    if (key == Qt::Key_P && mods == Qt::ShiftModifier) {
        if (!m_yankBuffer.isEmpty()) {
            auto tc = m_editor->textCursor();
            if (m_yankLinewise) {
                tc.movePosition(QTextCursor::StartOfBlock);
                tc.insertText(m_yankBuffer + QStringLiteral("\n"));
            } else {
                tc.insertText(m_yankBuffer);
            }
            m_editor->setTextCursor(tc);
        }
        m_count = 0;
        return true;
    }

    // ── Mode transitions ──────────────────────────────────────────────────
    if (key == Qt::Key_I && mods == Qt::NoModifier) {
        setMode(Mode::Insert);
        return true;
    }
    if (key == Qt::Key_I && mods == Qt::ShiftModifier) {
        auto tc = m_editor->textCursor();
        tc.movePosition(QTextCursor::StartOfBlock);
        // Skip leading whitespace
        const QString line = tc.block().text();
        int indent = 0;
        while (indent < line.length() && line.at(indent).isSpace()) ++indent;
        tc.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, indent);
        m_editor->setTextCursor(tc);
        setMode(Mode::Insert);
        return true;
    }
    if (key == Qt::Key_A && mods == Qt::NoModifier) {
        auto tc = m_editor->textCursor();
        if (!tc.atBlockEnd())
            tc.movePosition(QTextCursor::Right);
        m_editor->setTextCursor(tc);
        setMode(Mode::Insert);
        return true;
    }
    if (key == Qt::Key_A && mods == Qt::ShiftModifier) {
        auto tc = m_editor->textCursor();
        tc.movePosition(QTextCursor::EndOfBlock);
        m_editor->setTextCursor(tc);
        setMode(Mode::Insert);
        return true;
    }
    if (key == Qt::Key_O && mods == Qt::NoModifier) {
        auto tc = m_editor->textCursor();
        tc.movePosition(QTextCursor::EndOfBlock);
        tc.insertText(QStringLiteral("\n"));
        m_editor->setTextCursor(tc);
        setMode(Mode::Insert);
        return true;
    }
    if (key == Qt::Key_O && mods == Qt::ShiftModifier) {
        auto tc = m_editor->textCursor();
        tc.movePosition(QTextCursor::StartOfBlock);
        tc.insertText(QStringLiteral("\n"));
        tc.movePosition(QTextCursor::Up);
        m_editor->setTextCursor(tc);
        setMode(Mode::Insert);
        return true;
    }

    // v — visual mode
    if (key == Qt::Key_V && mods == Qt::NoModifier) {
        setMode(Mode::Visual);
        return true;
    }
    // V — visual line mode
    if (key == Qt::Key_V && mods == Qt::ShiftModifier) {
        auto tc = m_editor->textCursor();
        tc.movePosition(QTextCursor::StartOfBlock);
        tc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        m_editor->setTextCursor(tc);
        setMode(Mode::VisualLine);
        return true;
    }

    // : — command line mode
    if (key == Qt::Key_Colon || (key == Qt::Key_Semicolon && mods == Qt::ShiftModifier)) {
        m_commandLine.clear();
        setMode(Mode::Command);
        emit statusMessage(QStringLiteral(":"));
        return true;
    }

    // u — undo
    if (key == Qt::Key_U && mods == Qt::NoModifier) {
        for (int i = 0; i < count; ++i)
            m_editor->undo();
        m_count = 0;
        return true;
    }

    // Ctrl+R — redo
    if (key == Qt::Key_R && mods == Qt::ControlModifier) {
        for (int i = 0; i < count; ++i)
            m_editor->redo();
        m_count = 0;
        return true;
    }

    // . — repeat not implemented yet, just consume
    if (key == Qt::Key_Period && mods == Qt::NoModifier) {
        m_count = 0;
        return true;
    }

    m_count = 0;
    return false;
}

// ── Insert mode ──────────────────────────────────────────────────────────────

bool VimHandler::handleInsert(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape ||
        (event->key() == Qt::Key_BracketLeft && event->modifiers() == Qt::ControlModifier)) {
        setMode(Mode::Normal);
        // Move cursor left (vim behavior after leaving insert)
        auto tc = m_editor->textCursor();
        if (!tc.atBlockStart())
            tc.movePosition(QTextCursor::Left);
        m_editor->setTextCursor(tc);
        return true;
    }
    // Let the editor handle all other keys normally
    return false;
}

// ── Visual mode ──────────────────────────────────────────────────────────────

bool VimHandler::handleVisual(QKeyEvent *event)
{
    const int key = event->key();
    const auto mods = event->modifiers();
    const QString text = event->text();

    if (key == Qt::Key_Escape) {
        setMode(Mode::Normal);
        return true;
    }

    const int count = (m_count > 0) ? m_count : 1;

    // Motions with selection
    if (key == Qt::Key_H || key == Qt::Key_J || key == Qt::Key_K || key == Qt::Key_L ||
        key == Qt::Key_W || key == Qt::Key_B || key == Qt::Key_E ||
        key == Qt::Key_0 || (key == Qt::Key_4 && mods == Qt::ShiftModifier)) {

        for (int i = 0; i < count; ++i)
            executeMotion(text, true);
        m_count = 0;
        return true;
    }

    // d — delete selection
    if (key == Qt::Key_D || key == Qt::Key_X) {
        auto tc = m_editor->textCursor();
        if (tc.hasSelection()) {
            m_yankBuffer = tc.selectedText();
            m_yankLinewise = (m_mode == Mode::VisualLine);
            tc.removeSelectedText();
            m_editor->setTextCursor(tc);
        }
        setMode(Mode::Normal);
        return true;
    }

    // y — yank selection
    if (key == Qt::Key_Y) {
        auto tc = m_editor->textCursor();
        if (tc.hasSelection()) {
            m_yankBuffer = tc.selectedText();
            m_yankLinewise = (m_mode == Mode::VisualLine);
        }
        setMode(Mode::Normal);
        return true;
    }

    // c — change selection
    if (key == Qt::Key_C) {
        auto tc = m_editor->textCursor();
        if (tc.hasSelection()) {
            m_yankBuffer = tc.selectedText();
            m_yankLinewise = false;
            tc.removeSelectedText();
            m_editor->setTextCursor(tc);
        }
        setMode(Mode::Insert);
        return true;
    }

    // G — extend to end
    if (key == Qt::Key_G && mods == Qt::ShiftModifier) {
        auto tc = m_editor->textCursor();
        tc.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        m_editor->setTextCursor(tc);
        m_count = 0;
        return true;
    }

    // g prefix (gg)
    if (key == Qt::Key_G && mods == Qt::NoModifier) {
        if (m_pendingKeys == QStringLiteral("g")) {
            m_pendingKeys.clear();
            auto tc = m_editor->textCursor();
            tc.movePosition(QTextCursor::Start, QTextCursor::KeepAnchor);
            m_editor->setTextCursor(tc);
            return true;
        }
        m_pendingKeys = QStringLiteral("g");
        return true;
    }

    // Numeric prefix
    if (key >= Qt::Key_1 && key <= Qt::Key_9 && mods == Qt::NoModifier) {
        m_count = m_count * 10 + (key - Qt::Key_0);
        return true;
    }

    m_count = 0;
    return false;
}

// ── Command mode ─────────────────────────────────────────────────────────────

bool VimHandler::handleCommand(QKeyEvent *event)
{
    const int key = event->key();

    if (key == Qt::Key_Escape) {
        setMode(Mode::Normal);
        emit statusMessage(QString());
        return true;
    }

    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        executeExCommand(m_commandLine);
        setMode(Mode::Normal);
        return true;
    }

    if (key == Qt::Key_Backspace) {
        if (m_commandLine.isEmpty()) {
            setMode(Mode::Normal);
            emit statusMessage(QString());
        } else {
            m_commandLine.chop(1);
            emit statusMessage(QStringLiteral(":") + m_commandLine);
        }
        return true;
    }

    const QString text = event->text();
    if (!text.isEmpty() && text.at(0).isPrint()) {
        m_commandLine += text;
        emit statusMessage(QStringLiteral(":") + m_commandLine);
    }
    return true;
}

// ── Motion execution ─────────────────────────────────────────────────────────

bool VimHandler::executeMotion(const QString &key, bool select)
{
    auto tc = m_editor->textCursor();
    auto moveMode = select ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor;

    if (key == QStringLiteral("h")) {
        if (!tc.atBlockStart())
            tc.movePosition(QTextCursor::Left, moveMode);
    } else if (key == QStringLiteral("j")) {
        tc.movePosition(QTextCursor::Down, moveMode);
    } else if (key == QStringLiteral("k")) {
        tc.movePosition(QTextCursor::Up, moveMode);
    } else if (key == QStringLiteral("l")) {
        if (!tc.atBlockEnd())
            tc.movePosition(QTextCursor::Right, moveMode);
    } else if (key == QStringLiteral("w")) {
        int pos = findWordForward(tc.position());
        tc.setPosition(pos, moveMode);
    } else if (key == QStringLiteral("b")) {
        int pos = findWordBackward(tc.position());
        tc.setPosition(pos, moveMode);
    } else if (key == QStringLiteral("e")) {
        int pos = findWordEnd(tc.position());
        tc.setPosition(pos, moveMode);
    } else if (key == QStringLiteral("0")) {
        tc.movePosition(QTextCursor::StartOfBlock, moveMode);
    } else if (key == QStringLiteral("$")) {
        tc.movePosition(QTextCursor::EndOfBlock, moveMode);
    } else {
        return false;
    }

    m_editor->setTextCursor(tc);
    return true;
}

// ── Operator + motion ────────────────────────────────────────────────────────

void VimHandler::applyOperator(Operator op, const QString &motion)
{
    auto tc = m_editor->textCursor();
    int startPos = tc.position();

    // Apply motion with selection
    executeMotion(motion, true);

    tc = m_editor->textCursor();
    if (!tc.hasSelection()) {
        m_pendingOp = Operator::None;
        return;
    }

    m_yankBuffer = tc.selectedText();
    m_yankLinewise = false;

    switch (op) {
    case Operator::Delete:
        tc.removeSelectedText();
        break;
    case Operator::Change:
        tc.removeSelectedText();
        m_editor->setTextCursor(tc);
        m_pendingOp = Operator::None;
        setMode(Mode::Insert);
        return;
    case Operator::Yank:
        tc.setPosition(startPos);
        break;
    case Operator::None:
        break;
    }

    m_editor->setTextCursor(tc);
    m_pendingOp = Operator::None;
}

void VimHandler::applyLinewiseOperator(Operator op, int lines)
{
    auto tc = m_editor->textCursor();
    tc.movePosition(QTextCursor::StartOfBlock);
    for (int i = 1; i < lines; ++i)
        tc.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor);
    tc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);

    // Include the newline
    if (!tc.atEnd())
        tc.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);

    m_yankBuffer = tc.selectedText();
    // Remove paragraph separator → newline
    m_yankBuffer.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
    m_yankLinewise = true;

    switch (op) {
    case Operator::Delete:
        tc.removeSelectedText();
        break;
    case Operator::Change:
        tc.removeSelectedText();
        m_editor->setTextCursor(tc);
        setMode(Mode::Insert);
        return;
    case Operator::Yank:
        tc.setPosition(tc.anchor());
        break;
    case Operator::None:
        break;
    }

    m_editor->setTextCursor(tc);
    m_pendingOp = Operator::None;
}

// ── Ex-command execution ─────────────────────────────────────────────────────

void VimHandler::executeExCommand(const QString &cmd)
{
    const QString trimmed = cmd.trimmed();

    if (trimmed == QStringLiteral("w")) {
        emit saveRequested();
        emit statusMessage(tr("File saved"));
    } else if (trimmed == QStringLiteral("q")) {
        emit quitRequested();
    } else if (trimmed == QStringLiteral("wq") || trimmed == QStringLiteral("x")) {
        emit saveRequested();
        emit quitRequested();
    } else if (trimmed == QStringLiteral("q!")) {
        emit quitRequested();
    } else if (trimmed.startsWith(QStringLiteral("e "))) {
        const QString path = trimmed.mid(2).trimmed();
        if (!path.isEmpty())
            emit openFileRequested(path);
    } else {
        // Try as line number
        bool ok = false;
        int lineNum = trimmed.toInt(&ok);
        if (ok && lineNum > 0) {
            auto blk = m_editor->document()->findBlockByNumber(lineNum - 1);
            if (blk.isValid()) {
                auto tc = m_editor->textCursor();
                tc.setPosition(blk.position());
                m_editor->setTextCursor(tc);
            }
        } else {
            emit statusMessage(tr("Unknown command: %1").arg(trimmed));
        }
    }
}

// ── Helpers ──────────────────────────────────────────────────────────────────

void VimHandler::moveCursor(int position, bool select)
{
    auto tc = m_editor->textCursor();
    tc.setPosition(position, select ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
    m_editor->setTextCursor(tc);
}

int VimHandler::findWordForward(int pos) const
{
    const QString text = m_editor->toPlainText();
    if (pos >= text.length()) return pos;

    int i = pos;
    // Skip current word chars
    if (i < text.length() && (text.at(i).isLetterOrNumber() || text.at(i) == QLatin1Char('_'))) {
        while (i < text.length() && (text.at(i).isLetterOrNumber() || text.at(i) == QLatin1Char('_')))
            ++i;
    } else if (i < text.length() && !text.at(i).isSpace()) {
        // Skip punctuation
        while (i < text.length() && !text.at(i).isSpace() &&
               !(text.at(i).isLetterOrNumber() || text.at(i) == QLatin1Char('_')))
            ++i;
    }
    // Skip whitespace
    while (i < text.length() && text.at(i).isSpace())
        ++i;

    return i;
}

int VimHandler::findWordBackward(int pos) const
{
    const QString text = m_editor->toPlainText();
    if (pos <= 0) return 0;

    int i = pos - 1;
    // Skip whitespace
    while (i > 0 && text.at(i).isSpace())
        --i;

    if (i >= 0 && (text.at(i).isLetterOrNumber() || text.at(i) == QLatin1Char('_'))) {
        while (i > 0 && (text.at(i - 1).isLetterOrNumber() || text.at(i - 1) == QLatin1Char('_')))
            --i;
    } else if (i >= 0 && !text.at(i).isSpace()) {
        while (i > 0 && !text.at(i - 1).isSpace() &&
               !(text.at(i - 1).isLetterOrNumber() || text.at(i - 1) == QLatin1Char('_')))
            --i;
    }

    return i;
}

int VimHandler::findWordEnd(int pos) const
{
    const QString text = m_editor->toPlainText();
    if (pos >= text.length() - 1) return pos;

    int i = pos + 1;
    // Skip whitespace
    while (i < text.length() && text.at(i).isSpace())
        ++i;

    if (i < text.length() && (text.at(i).isLetterOrNumber() || text.at(i) == QLatin1Char('_'))) {
        while (i < text.length() - 1 &&
               (text.at(i + 1).isLetterOrNumber() || text.at(i + 1) == QLatin1Char('_')))
            ++i;
    } else if (i < text.length()) {
        while (i < text.length() - 1 && !text.at(i + 1).isSpace() &&
               !(text.at(i + 1).isLetterOrNumber() || text.at(i + 1) == QLatin1Char('_')))
            ++i;
    }

    return i;
}

int VimHandler::findCharOnLine(QChar ch, bool forward, int pos) const
{
    const QString text = m_editor->toPlainText();
    auto tc = m_editor->textCursor();
    tc.setPosition(pos);
    const int lineStart = tc.block().position();
    const int lineEnd = lineStart + tc.block().length() - 1;

    if (forward) {
        for (int i = pos + 1; i <= lineEnd && i < text.length(); ++i) {
            if (text.at(i) == ch) return i;
        }
    } else {
        for (int i = pos - 1; i >= lineStart; --i) {
            if (text.at(i) == ch) return i;
        }
    }
    return -1;
}

int VimHandler::matchingBracket(int pos) const
{
    const QString text = m_editor->toPlainText();
    if (pos >= text.length()) return -1;

    const QChar ch = text.at(pos);
    QChar match;
    int dir = 0;

    static const QHash<QChar, QChar> openBrackets = {
        {QLatin1Char('('), QLatin1Char(')')},
        {QLatin1Char('['), QLatin1Char(']')},
        {QLatin1Char('{'), QLatin1Char('}')}
    };
    static const QHash<QChar, QChar> closeBrackets = {
        {QLatin1Char(')'), QLatin1Char('(')},
        {QLatin1Char(']'), QLatin1Char('[')},
        {QLatin1Char('}'), QLatin1Char('{')}
    };

    if (openBrackets.contains(ch)) {
        match = openBrackets.value(ch);
        dir = 1;
    } else if (closeBrackets.contains(ch)) {
        match = closeBrackets.value(ch);
        dir = -1;
    } else {
        return -1;
    }

    int depth = 1;
    for (int i = pos + dir; i >= 0 && i < text.length(); i += dir) {
        if (text.at(i) == ch) ++depth;
        else if (text.at(i) == match) --depth;
        if (depth == 0) return i;
    }
    return -1;
}
