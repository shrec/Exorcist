#include "editorview.h"
#include "breakpointconditiondialog.h"
#include "codefoldingengine.h"
#include "minimapwidget.h"
#include "multicursorengine.h"
#include "piecetablebuffer.h"
#include "vimhandler.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QContextMenuEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QToolButton>
#include <QToolTip>

// ── LineNumberArea ──────────────────────────────────────────────────────────

class LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(EditorView *editor)
        : QWidget(editor),
          m_editor(editor)
    {
    }

    QSize sizeHint() const override
    {
        return QSize(m_editor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        m_editor->lineNumberAreaPaintEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        m_editor->lineNumberAreaMousePress(event);
    }

    void contextMenuEvent(QContextMenuEvent *event) override
    {
        m_editor->lineNumberAreaContextMenu(event);
    }

private:
    EditorView *m_editor;
};

// ── FindBar ─────────────────────────────────────────────────────────────────

class FindBar : public QWidget
{
    Q_OBJECT
public:
    explicit FindBar(EditorView *editor)
        : QWidget(editor),
          m_editor(editor),
          m_findInput(new QLineEdit(this)),
          m_replaceInput(new QLineEdit(this)),
          m_caseCheck(new QCheckBox(tr("Aa"), this)),
          m_wordCheck(new QCheckBox(tr("\\b"), this)),
          m_regexCheck(new QCheckBox(tr(".*"), this))
    {
        setAutoFillBackground(true);
        QPalette pal = palette();
        pal.setColor(QPalette::Window, QColor(40, 40, 40));
        setPalette(pal);

        m_findInput->setPlaceholderText(tr("Find"));
        m_findInput->setMinimumWidth(160);
        m_replaceInput->setPlaceholderText(tr("Replace"));
        m_replaceInput->setMinimumWidth(160);

        auto *prevBtn    = new QToolButton(this);
        auto *nextBtn    = new QToolButton(this);
        auto *replaceBtn = new QPushButton(tr("Replace"), this);
        auto *replaceAll = new QPushButton(tr("All"), this);
        auto *closeBtn   = new QToolButton(this);

        prevBtn->setArrowType(Qt::UpArrow);
        nextBtn->setArrowType(Qt::DownArrow);
        closeBtn->setText("x");

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(6, 4, 6, 4);
        layout->setSpacing(4);
        layout->addWidget(m_findInput);
        layout->addWidget(m_caseCheck);
        layout->addWidget(m_wordCheck);
        layout->addWidget(m_regexCheck);
        layout->addWidget(prevBtn);
        layout->addWidget(nextBtn);
        layout->addSpacing(8);
        layout->addWidget(m_replaceInput);
        layout->addWidget(replaceBtn);
        layout->addWidget(replaceAll);
        layout->addSpacing(4);
        layout->addWidget(closeBtn);
        setLayout(layout);

        connect(m_findInput, &QLineEdit::returnPressed, this, [this]() { findNext(); });
        connect(nextBtn, &QToolButton::clicked, this, [this]() { findNext(); });
        connect(prevBtn, &QToolButton::clicked, this, [this]() { findPrev(); });
        connect(replaceBtn, &QPushButton::clicked, this, &FindBar::replaceCurrent);
        connect(replaceAll, &QPushButton::clicked, this, &FindBar::replaceAll);
        connect(closeBtn, &QToolButton::clicked, this, [this]() { m_editor->hideFindBar(); });
    }

    void focusFind()
    {
        m_findInput->setFocus();
        m_findInput->selectAll();
    }

private:
    QTextDocument::FindFlags flags(bool backward = false) const
    {
        QTextDocument::FindFlags f;
        if (m_caseCheck->isChecked())
            f |= QTextDocument::FindCaseSensitively;
        if (m_wordCheck->isChecked())
            f |= QTextDocument::FindWholeWords;
        if (backward)
            f |= QTextDocument::FindBackward;
        return f;
    }

    void findNext()
    {
        const QString pattern = m_findInput->text();
        if (pattern.isEmpty())
            return;
        if (!m_editor->find(pattern, flags())) {
            QTextCursor cur = m_editor->textCursor();
            cur.movePosition(QTextCursor::Start);
            m_editor->setTextCursor(cur);
            m_editor->find(pattern, flags());
        }
    }

    void findPrev()
    {
        const QString pattern = m_findInput->text();
        if (pattern.isEmpty())
            return;
        if (!m_editor->find(pattern, flags(true))) {
            QTextCursor cur = m_editor->textCursor();
            cur.movePosition(QTextCursor::End);
            m_editor->setTextCursor(cur);
            m_editor->find(pattern, flags(true));
        }
    }

    void replaceCurrent()
    {
        const QString pattern     = m_findInput->text();
        const QString replacement = m_replaceInput->text();
        if (pattern.isEmpty())
            return;
        QTextCursor cur = m_editor->textCursor();
        if (cur.hasSelection() && cur.selectedText() == pattern) {
            cur.insertText(replacement);
        }
        findNext();
    }

    void replaceAll()
    {
        const QString pattern     = m_findInput->text();
        const QString replacement = m_replaceInput->text();
        if (pattern.isEmpty())
            return;

        QTextCursor cur = m_editor->textCursor();
        cur.movePosition(QTextCursor::Start);
        m_editor->setTextCursor(cur);

        while (m_editor->find(pattern, flags())) {
            m_editor->textCursor().insertText(replacement);
        }
    }

    EditorView *m_editor;
    QLineEdit  *m_findInput;
    QLineEdit  *m_replaceInput;
    QCheckBox  *m_caseCheck;
    QCheckBox  *m_wordCheck;
    QCheckBox  *m_regexCheck;
};

#include "editorview.moc"

// ── EditorView ───────────────────────────────────────────────────────────────

EditorView::EditorView(QWidget *parent)
    : QPlainTextEdit(parent),
      m_lineNumberArea(new LineNumberArea(this)),
      m_findBar(new FindBar(this)),
      m_isLargeFilePreview(false),
      m_isLoadingChunk(false),
      m_buffer(std::make_unique<PieceTableBuffer>()),
      m_multiCursor(std::make_unique<MultiCursorEngine>())
{
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setTabStopDistance(4 * fontMetrics().horizontalAdvance(' '));

    m_findBar->hide();

    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &EditorView::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &EditorView::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &EditorView::highlightCurrentLine);
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &EditorView::highlightMatchingBrackets);
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            this, &EditorView::handleScroll);

    // Sync PieceTableBuffer on every document edit
    connect(document(), &QTextDocument::contentsChange,
            this, [this](int pos, int removed, int added) {
                if (m_bufferSyncing)
                    return;
                m_bufferSyncing = true;
                if (removed > 0)
                    m_buffer->remove(pos, removed);
                if (added > 0) {
                    // Extract the newly-added text from the document
                    QTextCursor cur(document());
                    cur.setPosition(pos);
                    cur.setPosition(pos + added, QTextCursor::KeepAnchor);
                    m_buffer->insert(pos, cur.selectedText().replace(QChar::ParagraphSeparator, QLatin1Char('\n')));
                }
                m_bufferSyncing = false;
            });

    m_multiCursor->setDocument(document());

    m_foldingEngine = std::make_unique<CodeFoldingEngine>(this);
    m_foldingEngine->setDocument(document());

    // Debounced fold-region rebuild so we recompute on intra-line edits
    // (e.g. typing `{` or `}` mid-line, which doesn't change blockCount)
    // without thrashing on every keystroke.
    m_foldRecomputeTimer.setSingleShot(true);
    m_foldRecomputeTimer.setInterval(120);
    connect(&m_foldRecomputeTimer, &QTimer::timeout,
            this, &EditorView::updateFoldRegions);

    // Rebuild fold regions on document structure changes
    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &EditorView::updateFoldRegions);
    // Also rebuild on any content change (debounced) so adding/removing
    // braces or rewriting a line refreshes the fold triangles.
    connect(document(), &QTextDocument::contentsChange,
            this, [this](int, int, int) {
                m_foldRecomputeTimer.start();
            });
    connect(m_foldingEngine.get(), &CodeFoldingEngine::foldStateChanged,
            this, [this]() {
                document()->markContentsDirty(0, document()->characterCount());
                viewport()->update();
                if (m_lineNumberArea)
                    m_lineNumberArea->update();
            });

    m_vimHandler = std::make_unique<VimHandler>(this, this);

    // TODO/FIXME/HACK/NOTE marker scan — debounced 200ms on edits.
    // Initial scan happens after the first content load via the
    // debounced timer; rescan triggers on each contentsChange.
    m_todoRescanTimer.setSingleShot(true);
    m_todoRescanTimer.setInterval(200);
    connect(&m_todoRescanTimer, &QTimer::timeout,
            this, &EditorView::rescanTodoMarkers);
    connect(document(), &QTextDocument::contentsChange,
            this, [this](int, int, int) {
                m_todoRescanTimer.start();
            });

    // Inline color swatch scan — debounced 250ms on edits. Caches hex
    // codes per block so paintEvent doesn't have to re-run the regex on
    // every frame, and so mousePressEvent has a fast hit-test set.
    m_colorRescanTimer.setSingleShot(true);
    m_colorRescanTimer.setInterval(250);
    connect(&m_colorRescanTimer, &QTimer::timeout,
            this, [this]() {
                rescanColorSwatches();
                viewport()->update();
            });
    connect(document(), &QTextDocument::contentsChange,
            this, [this](int, int, int) {
                m_colorRescanTimer.start();
            });

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();

    // Minimap
    m_minimap = new MinimapWidget(this, this);

    // Tooltip events for QAbstractScrollArea subclasses are delivered to the
    // viewport, not to the EditorView itself. Filter the viewport so we can
    // intercept QEvent::ToolTip and surface debug variable values on hover.
    if (viewport()) {
        viewport()->installEventFilter(this);
        viewport()->setMouseTracking(true);
    }
}

EditorView::~EditorView() = default;

void EditorView::setMinimapVisible(bool visible)
{
    m_minimapVisible = visible;
    if (m_minimap)
        m_minimap->setVisible(visible);
    updateLineNumberAreaWidth(0);   // refresh right viewport margin
}

void EditorView::setIndentGuidesVisible(bool visible)
{
    m_showIndentGuides = visible;
    viewport()->update();
}

void EditorView::setWhitespaceVisible(bool visible)
{
    if (m_showWhitespace == visible)
        return;
    m_showWhitespace = visible;
    viewport()->update();
}

// ── Code folding ──────────────────────────────────────────────────────────────

CodeFoldingEngine *EditorView::foldingEngine() const
{
    return m_foldingEngine.get();
}

void EditorView::updateFoldRegions()
{
    if (m_foldingEngine) {
        m_foldingEngine->setLanguageId(m_languageId);
        m_foldingEngine->update();
        if (m_lineNumberArea)
            m_lineNumberArea->update();
    }
}

void EditorView::setLargeFilePreview(const QString &text, bool isPartial)
{
    // Reset the shadow buffer BEFORE setPlainText, which fires contentsChange
    m_buffer = std::make_unique<PieceTableBuffer>(text);
    m_bufferSyncing = true;  // suppress the contentsChange handler
    setPlainText(text);
    m_bufferSyncing = false;
    m_isLargeFilePreview = isPartial;
    setReadOnly(isPartial);
    m_isLoadingChunk = false;
}

void EditorView::appendLargeFileChunk(const QString &text, bool isFinal)
{
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
    m_isLargeFilePreview = !isFinal;
    if (isFinal) {
        setReadOnly(false);
    }
    m_isLoadingChunk = false;
}

bool EditorView::isLargeFilePreview() const
{
    return m_isLargeFilePreview;
}

void EditorView::showFindBar()
{
    m_findBar->show();
    repositionFindBar();
    m_findBar->focusFind();
}

void EditorView::hideFindBar()
{
    m_findBar->hide();
    setFocus();
}

int EditorView::lineNumberAreaWidth() const
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    // Minimum 3 digits so the gutter never gets too narrow
    digits = qMax(digits, 3);

    const int charW = fontMetrics().horizontalAdvance(QLatin1Char('9'));
    // 10px left margin (diagnostic dots / diff bars) + 8px right padding + 14px fold gutter
    int space = 10 + charW * digits + 8 + 14;

    // Extra width for blame annotations
    if (m_showBlame && !m_blameData.isEmpty())
        space += fontMetrics().horizontalAdvance(QLatin1String("author12 abc12345 "));

    return space;
}

void EditorView::updateLineNumberAreaWidth(int)
{
    if (!m_lineNumberArea)
        return;
    const int w = lineNumberAreaWidth();
    const int rightMargin = (m_minimap && m_minimap->isVisible()) ? m_minimap->width() : 0;
    setViewportMargins(w, 0, rightMargin, 0);
    // setViewportMargins shrinks the viewport but does NOT trigger resizeEvent on
    // the editor, so the LineNumberArea overlay must be repositioned explicitly.
    const QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(cr.left(), cr.top(), w, cr.height());
}

void EditorView::updateLineNumberArea(const QRect &rect, int dy)
{
    if (!m_lineNumberArea)
        return;
    if (dy) {
        m_lineNumberArea->scroll(0, dy);
    } else {
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth(0);
    }
}

void EditorView::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);

    if (!m_lineNumberArea)
        return;
    const QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(),
                                        lineNumberAreaWidth(), cr.height()));
    repositionFindBar();

    // Position minimap on right edge
    if (m_minimap && m_minimap->isVisible()) {
        const int mw = m_minimap->width();
        m_minimap->setGeometry(cr.right() - mw, cr.top(), mw, cr.height());
    }
}

void EditorView::keyPressEvent(QKeyEvent *event)
{
    // ── Vim handler (highest priority when enabled) ──────────────────────
    if (m_vimHandler && m_vimHandler->isEnabled()) {
        if (m_vimHandler->handleKeyPress(event))
            return;
    }

    if (event->key() == Qt::Key_F && event->modifiers() == Qt::ControlModifier) {
        m_findBar->isVisible() ? hideFindBar() : showFindBar();
        return;
    }
    if (event->key() == Qt::Key_Escape && m_findBar->isVisible()) {
        hideFindBar();
        return;
    }

    // ── Multi-cursor keybindings ──────────────────────────────────────────

    // Escape — clear secondary cursors (before ghost text check)
    if (event->key() == Qt::Key_Escape && m_multiCursor->hasMultipleCursors()) {
        m_multiCursor->clearSecondaryCursors();
        setTextCursor(m_multiCursor->primaryCursor());
        viewport()->update();
        event->accept();
        return;
    }

    // Ctrl+D — select next occurrence (VS Code style)
    // If primary cursor has no selection, promote it to the word under cursor first;
    // subsequent presses add the next match as a new cursor.
    if (event->key() == Qt::Key_D && event->modifiers() == Qt::ControlModifier) {
        QTextCursor primary = textCursor();
        if (!primary.hasSelection() && !m_multiCursor->hasMultipleCursors()) {
            primary.select(QTextCursor::WordUnderCursor);
            if (primary.hasSelection())
                setTextCursor(primary);
        }
        syncPrimaryCursorToEngine();
        if (m_multiCursor->addCursorAtNextOccurrence()) {
            // Show the last-added cursor by scrolling to it
            auto curs = m_multiCursor->cursors();
            if (!curs.empty()) {
                QTextCursor last = curs.back();
                setTextCursor(last);
            }
            viewport()->update();
        }
        event->accept();
        return;
    }

    // Ctrl+Shift+L — select all occurrences
    if (event->key() == Qt::Key_L &&
        event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
        syncPrimaryCursorToEngine();
        m_multiCursor->addCursorsAtAllOccurrences();
        viewport()->update();
        event->accept();
        return;
    }

    // ── Multi-cursor text dispatch ────────────────────────────────────────
    if (m_multiCursor->hasMultipleCursors()) {
        // Route text-modifying keys through the engine
        if (event->key() == Qt::Key_Backspace && event->modifiers() == Qt::NoModifier) {
            document()->blockSignals(false);
            m_multiCursor->backspace();
            viewport()->update();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Delete && event->modifiers() == Qt::NoModifier) {
            m_multiCursor->deleteChar();
            viewport()->update();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            m_multiCursor->insertText(QStringLiteral("\n"));
            viewport()->update();
            event->accept();
            return;
        }
        // Regular text input
        if (!event->text().isEmpty() && event->text().at(0).isPrint()) {
            m_multiCursor->insertText(event->text());
            viewport()->update();
            event->accept();
            return;
        }
    }

    // Ctrl+I — inline AI chat
    if (event->key() == Qt::Key_I && event->modifiers() == Qt::ControlModifier) {
        const QString sel = textCursor().selectedText();
        emit inlineChatRequested(sel, m_languageId);
        event->accept();
        return;
    }

    // ── Ghost text handling ───────────────────────────────────────────────
    if (hasGhostText()) {
        if (event->key() == Qt::Key_Tab && event->modifiers() == Qt::NoModifier) {
            acceptGhostText();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Right && event->modifiers() == Qt::ControlModifier) {
            acceptGhostTextWord();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Escape) {
            clearGhostText();
            event->accept();
            return;
        }
        // Any printable key or other editing key dismisses ghost text
        clearGhostText();
    }

    // Alt+\ — manual completion trigger
    if (event->key() == Qt::Key_Backslash && event->modifiers() == Qt::AltModifier) {
        emit manualCompletionRequested();
        event->accept();
        return;
    }

    // F12 — Go to Definition
    if (event->key() == Qt::Key_F12 && event->modifiers() == Qt::NoModifier) {
        emit goToDefinitionRequested();
        event->accept();
        return;
    }

    // Ctrl+F12 — Go to Declaration
    if (event->key() == Qt::Key_F12 && event->modifiers() == Qt::ControlModifier) {
        emit goToDeclarationRequested();
        event->accept();
        return;
    }

    // Shift+F12 — Find All References
    if (event->key() == Qt::Key_F12 && event->modifiers() == Qt::ShiftModifier) {
        emit findReferencesRequested();
        event->accept();
        return;
    }

    // F2 — Rename Symbol
    if (event->key() == Qt::Key_F2 && event->modifiers() == Qt::NoModifier) {
        emit renameSymbolRequested();
        event->accept();
        return;
    }

    // Ctrl+Shift+F — Format Document
    if (event->key() == Qt::Key_F &&
        event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
        emit formatDocumentRequested();
        event->accept();
        return;
    }

    // ── Smart indentation on Enter ────────────────────────────────────────
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && event->modifiers() == Qt::NoModifier)
    {
        QTextCursor cur = textCursor();
        const QString lineText = cur.block().text();
        const int col = cur.positionInBlock();

        // Extract leading whitespace from current line
        int indent = 0;
        while (indent < lineText.length() && lineText.at(indent).isSpace())
            ++indent;
        QString ws = lineText.left(indent);

        // Check if character before cursor is an opening bracket
        const QChar before = col > 0 ? lineText.at(col - 1) : QChar();
        const QChar after  = col < lineText.length() ? lineText.at(col) : QChar();
        const bool openBrace = (before == QLatin1Char('{')
                             || before == QLatin1Char('(')
                             || before == QLatin1Char('['));

        // Determine tab string (spaces matching current tab stop distance)
        const int tabWidth = qMax(1, qRound(tabStopDistance() / fontMetrics().horizontalAdvance(QLatin1Char(' '))));
        const QString tab = QString(tabWidth, QLatin1Char(' '));

        cur.beginEditBlock();
        if (openBrace && matchingBracket(before) == after) {
            // Cursor between matching brackets: {|} → {\n    |\n}
            cur.insertText(QLatin1Char('\n') + ws + tab + QLatin1Char('\n') + ws);
            // Position cursor on the indented middle line
            cur.movePosition(QTextCursor::Up);
            cur.movePosition(QTextCursor::EndOfBlock);
        } else if (openBrace) {
            // After opening bracket: extra indent
            cur.insertText(QLatin1Char('\n') + ws + tab);
        } else {
            // Normal Enter: preserve indentation
            cur.insertText(QLatin1Char('\n') + ws);
        }
        cur.endEditBlock();
        setTextCursor(cur);
        event->accept();
        return;
    }

    // ── Auto-close brackets and quotes ────────────────────────────────────
    if (event->modifiers() == Qt::NoModifier || event->modifiers() == Qt::ShiftModifier) {
        const QString text = event->text();
        if (text.length() == 1) {
            const QChar ch = text.at(0);
            const QChar closing = matchingBracket(ch);
            QTextCursor cur = textCursor();

            // Auto-close opening brackets: insert pair and place cursor inside
            if (closing != QChar() && (ch == QLatin1Char('{')
                                    || ch == QLatin1Char('(')
                                    || ch == QLatin1Char('[')))
            {
                // Only auto-close if there's no selection and next char is
                // whitespace, closing bracket, or end-of-line
                const QString lineText = cur.block().text();
                const int col = cur.positionInBlock();
                const QChar nextCh = col < lineText.length() ? lineText.at(col) : QChar();
                if (!cur.hasSelection()
                    && (nextCh.isNull() || nextCh.isSpace()
                        || nextCh == QLatin1Char(')') || nextCh == QLatin1Char(']')
                        || nextCh == QLatin1Char('}') || nextCh == QLatin1Char(',')
                        || nextCh == QLatin1Char(';')))
                {
                    cur.beginEditBlock();
                    cur.insertText(QString(ch) + QString(closing));
                    cur.movePosition(QTextCursor::Left);
                    cur.endEditBlock();
                    setTextCursor(cur);
                    event->accept();
                    return;
                }
            }

            // Auto-close quotes: only if not already inside a matching quote
            if (ch == QLatin1Char('"') || ch == QLatin1Char('\'')) {
                const QString lineText = cur.block().text();
                const int col = cur.positionInBlock();
                const QChar nextCh = col < lineText.length() ? lineText.at(col) : QChar();

                // If next char is the same quote, just skip over it
                if (nextCh == ch && !cur.hasSelection()) {
                    cur.movePosition(QTextCursor::Right);
                    setTextCursor(cur);
                    event->accept();
                    return;
                }

                // Auto-close: insert pair if next char allows it
                if (!cur.hasSelection()
                    && (nextCh.isNull() || nextCh.isSpace()
                        || nextCh == QLatin1Char(')') || nextCh == QLatin1Char(']')
                        || nextCh == QLatin1Char('}') || nextCh == QLatin1Char(',')
                        || nextCh == QLatin1Char(';')))
                {
                    // Count existing quotes on the line to determine if we're opening or closing
                    int quoteCount = 0;
                    for (int i = 0; i < col; ++i)
                        if (lineText.at(i) == ch) ++quoteCount;
                    // Only auto-close if quote count is even (opening a new pair)
                    if (quoteCount % 2 == 0) {
                        cur.beginEditBlock();
                        cur.insertText(QString(ch) + QString(ch));
                        cur.movePosition(QTextCursor::Left);
                        cur.endEditBlock();
                        setTextCursor(cur);
                        event->accept();
                        return;
                    }
                }
            }

            // Skip over closing bracket if it matches what was auto-inserted
            if (ch == QLatin1Char(')') || ch == QLatin1Char(']') || ch == QLatin1Char('}')) {
                const QString lineText = cur.block().text();
                const int col = cur.positionInBlock();
                if (col < lineText.length() && lineText.at(col) == ch) {
                    cur.movePosition(QTextCursor::Right);
                    setTextCursor(cur);
                    event->accept();
                    return;
                }
            }
        }
    }

    // ── Backspace: delete matching pair ───────────────────────────────────
    if (event->key() == Qt::Key_Backspace && event->modifiers() == Qt::NoModifier) {
        QTextCursor cur = textCursor();
        if (!cur.hasSelection()) {
            const QString lineText = cur.block().text();
            const int col = cur.positionInBlock();
            if (col > 0 && col < lineText.length()) {
                const QChar before = lineText.at(col - 1);
                const QChar after  = lineText.at(col);
                if (matchingBracket(before) == after
                    && (before == QLatin1Char('{') || before == QLatin1Char('(')
                        || before == QLatin1Char('[')
                        || before == QLatin1Char('"') || before == QLatin1Char('\'')))
                {
                    cur.beginEditBlock();
                    cur.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);
                    cur.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 2);
                    cur.removeSelectedText();
                    cur.endEditBlock();
                    setTextCursor(cur);
                    event->accept();
                    return;
                }
            }
        }
    }

    QPlainTextEdit::keyPressEvent(event);
}

// ── Mouse event handlers for multi-cursor ─────────────────────────────────────

void EditorView::mousePressEvent(QMouseEvent *event)
{
    // Color swatch click — open QColorDialog and replace literal in place.
    // Checked before any other modifier-aware handling so a plain left-click
    // on a swatch never starts a text selection.
    if (event->button() == Qt::LeftButton &&
        event->modifiers() == Qt::NoModifier)
    {
        if (handleColorSwatchClick(event->pos())) {
            event->accept();
            return;
        }
    }

    // Alt+Click — add a cursor at the click position
    if (event->button() == Qt::LeftButton &&
        event->modifiers() == Qt::AltModifier)
    {
        syncPrimaryCursorToEngine();
        QTextCursor clickCur = cursorForPosition(event->pos());
        m_multiCursor->addCursor(clickCur.position());
        viewport()->update();
        event->accept();
        return;
    }

    // Alt+Drag — start rectangular/column selection
    if (event->button() == Qt::LeftButton &&
        event->modifiers() == (Qt::AltModifier | Qt::ShiftModifier))
    {
        m_altDragging = true;
        m_altDragStart = event->pos();
        event->accept();
        return;
    }

    // Regular click clears multi-cursors
    if (event->button() == Qt::LeftButton && m_multiCursor->hasMultipleCursors() &&
        !(event->modifiers() & Qt::AltModifier))
    {
        m_multiCursor->clearAll();
    }

    QPlainTextEdit::mousePressEvent(event);
}

void EditorView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_altDragging) {
        // Update rectangular selection in real-time
        QTextCursor startCur = cursorForPosition(m_altDragStart);
        QTextCursor endCur   = cursorForPosition(event->pos());

        int startLine = startCur.blockNumber();
        int startCol  = startCur.positionInBlock();
        int endLine   = endCur.blockNumber();
        int endCol    = endCur.positionInBlock();

        m_multiCursor->setRectangularSelection(startLine, startCol, endLine, endCol);
        viewport()->update();
        event->accept();
        return;
    }

    QPlainTextEdit::mouseMoveEvent(event);
}

void EditorView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_altDragging && event->button() == Qt::LeftButton) {
        m_altDragging = false;
        event->accept();
        return;
    }

    QPlainTextEdit::mouseReleaseEvent(event);
}

void EditorView::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), QColor(26, 26, 26));

    // Build a fast lookup: line → worst severity
    QHash<int, DiagSeverity> lineToSeverity;
    for (const DiagnosticMark &d : std::as_const(m_diagMarks)) {
        if (!lineToSeverity.contains(d.line) ||
            static_cast<int>(d.severity) <
            static_cast<int>(lineToSeverity[d.line]))
        {
            lineToSeverity[d.line] = d.severity;
        }
    }

    // Build a fast lookup: line → diff type
    QHash<int, DiffType> lineToDiff;
    for (const DiffRange &dr : std::as_const(m_diffRanges)) {
        for (int i = 0; i < dr.lineCount; ++i)
            lineToDiff[dr.startLine + i] = dr.type;
    }

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top    = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    const int lineH = fontMetrics().height();

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            // Line number
            const QString number = QString::number(blockNumber + 1);
            painter.setPen(QColor(140, 140, 140));
            painter.drawText(10, top, m_lineNumberArea->width() - 18,
                             lineH, Qt::AlignRight, number);

            // Breakpoint indicator (red filled circle, 1-based line) — drawn
            // FIRST so the breakpoint zone is the leftmost gutter strip.
            if (m_breakpointLines.contains(blockNumber + 1)) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(0xE5, 0x1A, 0x1A));
                const int bpSize = 10;
                painter.drawEllipse(2, top + (lineH - bpSize) / 2,
                                    bpSize, bpSize);
            }

            // Diagnostic indicator — drawn as a vertical bar on the RIGHT
            // edge of the gutter (next to the code area) so it can't be
            // confused with the breakpoint dot on the left. Error = red,
            // warning = yellow, info = blue.
            if (lineToSeverity.contains(blockNumber)) {
                const DiagSeverity sev = lineToSeverity[blockNumber];
                QColor diag;
                switch (sev) {
                case DiagSeverity::Error:   diag = QColor(0xF4, 0x47, 0x47); break;
                case DiagSeverity::Warning: diag = QColor(0xFF, 0xCC, 0x00); break;
                default:                    diag = QColor(0x75, 0xBF, 0xFF); break;
                }
                painter.setPen(Qt::NoPen);
                painter.setBrush(diag);
                const int barW = 3;
                const int barX = m_lineNumberArea->width() - barW;
                painter.drawRect(barX, top, barW, lineH);
            }

            // Current debug line highlight (yellow arrow)
            if (m_currentDebugLine > 0 && blockNumber + 1 == m_currentDebugLine) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(0xFF, 0xCC, 0x00));
                const int arrowH = 10;
                const int arrowW = 7;
                const int ax = m_lineNumberArea->width() - arrowW - 2;
                const int ay = top + (lineH - arrowH) / 2;
                QPolygon arrow;
                arrow << QPoint(ax, ay)
                      << QPoint(ax + arrowW, ay + arrowH / 2)
                      << QPoint(ax, ay + arrowH);
                painter.drawPolygon(arrow);
            }

            // NES arrow indicator (→ pointing right, yellow-green)
            if (blockNumber == m_nesLine) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(0x4E, 0xC9, 0xB0));  // teal/green
                const int arrowH = 8;
                const int arrowW = 6;
                const int ax = m_lineNumberArea->width() - arrowW - 2;
                const int ay = top + (lineH - arrowH) / 2;
                QPolygon arrow;
                arrow << QPoint(ax, ay)
                      << QPoint(ax + arrowW, ay + arrowH / 2)
                      << QPoint(ax, ay + arrowH);
                painter.drawPolygon(arrow);
            }

            // Inline diff gutter bar (2px colored bar on left edge)
            if (lineToDiff.contains(blockNumber)) {
                QColor barColor;
                switch (lineToDiff[blockNumber]) {
                case DiffType::Added:    barColor = QColor(0x50, 0xC8, 0x78); break;
                case DiffType::Modified: barColor = QColor(0x00, 0x7A, 0xCC); break;
                case DiffType::Deleted:  barColor = QColor(0xF4, 0x47, 0x47); break;
                }
                painter.setPen(Qt::NoPen);
                painter.setBrush(barColor);
                painter.drawRect(0, top, 2, bottom - top);
            }

            // Blame annotation (author + short hash left of line number)
            const int lineNum1 = blockNumber + 1;
            if (m_showBlame && m_blameData.contains(lineNum1)) {
                const auto &bi = m_blameData[lineNum1];
                const QString blameTxt = bi.author.left(10) + QLatin1Char(' ') + bi.hash.left(8);
                painter.setPen(QColor(90, 90, 90));
                painter.drawText(8, top, m_lineNumberArea->width() - 14,
                                 lineH, Qt::AlignLeft, blameTxt);
            }

            // TODO/FIXME/HACK/NOTE marker dot — small colored circle placed
            // just inside the right edge of the gutter, between the line
            // number and the diagnostic bar / fold triangle. Uses 1-based
            // line keys to match the rest of the marker storage scheme.
            const int markerLine1 = blockNumber + 1;
            if (m_todoMarkers.contains(markerLine1)) {
                const QString &kw = m_todoMarkers[markerLine1];
                QColor c;
                if (kw == QLatin1String("TODO"))       c = QColor(0xDC, 0xDC, 0xAA);
                else if (kw == QLatin1String("FIXME")) c = QColor(0xF4, 0x47, 0x47);
                else if (kw == QLatin1String("HACK"))  c = QColor(0xCE, 0x91, 0x78);
                else                                   c = QColor(0x75, 0xBF, 0xFF); // NOTE
                painter.setPen(Qt::NoPen);
                painter.setBrush(c);
                const int dotSize = 4;
                // Place dot 10px from the right edge so it sits between the
                // line number text and the fold/diagnostic strip on the right.
                const int dx = m_lineNumberArea->width() - 10;
                const int dy = top + (lineH - dotSize) / 2;
                painter.drawEllipse(dx, dy, dotSize, dotSize);
            }

            // Code folding indicators (▾ expanded / ▸ collapsed)
            if (m_foldingEngine && m_foldingEngine->isFoldable(blockNumber)) {
                const int foldX = m_lineNumberArea->width() - 13;
                const int foldY = top + (lineH - 8) / 2;
                painter.setPen(Qt::NoPen);
                // VS Code dark-modern fold guide color
                painter.setBrush(QColor(0x85, 0x85, 0x85));
                QPolygon tri;
                if (m_foldingEngine->isFolded(blockNumber)) {
                    // ▸ right-pointing triangle (collapsed)
                    tri << QPoint(foldX, foldY)
                        << QPoint(foldX + 7, foldY + 4)
                        << QPoint(foldX, foldY + 8);
                } else {
                    // ▾ down-pointing triangle (expanded)
                    tri << QPoint(foldX, foldY)
                        << QPoint(foldX + 8, foldY)
                        << QPoint(foldX + 4, foldY + 7);
                }
                painter.drawPolygon(tri);
            }
        }

        block = block.next();
        if (!block.isValid())
            break;
        top    = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

// Scan the document for TODO/FIXME/HACK/NOTE markers. The regex requires the
// keyword to be preceded by a non-alphanumeric (or start of line) so we don't
// match inside identifiers like "MYTODO" or "fixmeup". Followed by ":" to
// avoid matching the bare word inside string literals or prose. Matches are
// stored in m_todoMarkers keyed by 1-based line number.
// ── Inline color swatches ──────────────────────────────────────────────────
//
// Render a small filled square next to each `#rgb`, `#rrggbb` or `#rrggbbaa`
// literal. Clicking the square opens QColorDialog and replaces the literal
// with the picked color. The matched range is cached per block so paintEvent
// stays O(visible blocks * matches/block) and mouse hit-test is O(matches).

namespace {

// Regex used both for paint-time scan and for click-time replacement.
// `\b` word-boundary on the trailing side keeps us from gobbling characters
// from an adjacent identifier (e.g. `#deadbeefxyz`).
const QRegularExpression &colorSwatchRegex()
{
    static const QRegularExpression re(QStringLiteral("#[0-9A-Fa-f]{3,8}\\b"));
    return re;
}

// Decode a hex literal (3, 4, 6, or 8 hex digits) into a QColor.
// Returns invalid QColor for unsupported lengths (e.g. 5 or 7).
QColor colorFromHex(const QString &hex)
{
    if (hex.size() < 2 || hex[0] != QLatin1Char('#'))
        return QColor();
    const int n = hex.size() - 1;
    if (n != 3 && n != 4 && n != 6 && n != 8)
        return QColor();
    // QColor accepts #rgb, #rrggbb, #aarrggbb. For #rrggbbaa (8-digit, alpha
    // last) we have to swap the alpha to the front because Qt expects ARGB.
    if (n == 8) {
        const QString rgb = hex.mid(1, 6);
        const QString aa  = hex.mid(7, 2);
        QColor c;
        c.setNamedColor(QStringLiteral("#") + aa + rgb);
        return c;
    }
    if (n == 4) {
        // #rgba (4-digit, alpha last) → expand and swap to #aarrggbb.
        const QString r = hex.mid(1, 1);
        const QString g = hex.mid(2, 1);
        const QString b = hex.mid(3, 1);
        const QString a = hex.mid(4, 1);
        QColor c;
        c.setNamedColor(QStringLiteral("#") + a + a + r + r + g + g + b + b);
        return c;
    }
    QColor c;
    c.setNamedColor(hex);
    return c;
}

// Format a QColor as a hex string with the same shape as `original`. We
// preserve case (uppercase/lowercase) and length (3/4/6/8 digits) so a
// round-trip through the swatch picker doesn't cause noisy diffs.
QString hexFromColor(const QColor &color, const QString &original)
{
    const bool upper = std::any_of(
        original.cbegin(), original.cend(),
        [](QChar ch) { return ch.isLetter() && ch.isUpper(); });
    const int n = original.size() - 1;

    auto fmt = [upper](int v) {
        return QString(QStringLiteral("%1"))
            .arg(v & 0xff, 2, 16, QLatin1Char('0'))
            .toUpper();
    };
    auto fmtLower = [](int v) {
        return QString(QStringLiteral("%1"))
            .arg(v & 0xff, 2, 16, QLatin1Char('0'));
    };

    QString rr, gg, bb, aa;
    if (upper) {
        rr = fmt(color.red()); gg = fmt(color.green()); bb = fmt(color.blue());
        aa = fmt(color.alpha());
    } else {
        rr = fmtLower(color.red()); gg = fmtLower(color.green());
        bb = fmtLower(color.blue()); aa = fmtLower(color.alpha());
    }

    // Try to keep the original length: 3/6 digits drop alpha; 4/8 keep it.
    if (n == 3) {
        // Best-effort short form: only emit #rgb if each channel's two
        // hex digits are equal; otherwise widen to 6 digits.
        if (rr[0] == rr[1] && gg[0] == gg[1] && bb[0] == bb[1])
            return QStringLiteral("#") + rr.left(1) + gg.left(1) + bb.left(1);
        return QStringLiteral("#") + rr + gg + bb;
    }
    if (n == 4) {
        if (rr[0] == rr[1] && gg[0] == gg[1] && bb[0] == bb[1] && aa[0] == aa[1])
            return QStringLiteral("#") + rr.left(1) + gg.left(1) + bb.left(1) + aa.left(1);
        return QStringLiteral("#") + rr + gg + bb + aa;
    }
    if (n == 8 || color.alpha() != 255) {
        return QStringLiteral("#") + rr + gg + bb + aa;
    }
    return QStringLiteral("#") + rr + gg + bb;
}

// Swatch geometry (kept small and consistent with VS Code's color preview).
constexpr int kSwatchSize    = 12;   // px, both width and height
constexpr int kSwatchPadding = 3;    // px gap to the left of the literal

} // namespace

void EditorView::rescanColorSwatches()
{
    QHash<int, QList<QPair<int, QString>>> next;
    const QRegularExpression &re = colorSwatchRegex();

    QTextBlock block = document()->firstBlock();
    while (block.isValid()) {
        const QString text = block.text();
        if (text.contains(QLatin1Char('#'))) {
            QList<QPair<int, QString>> matches;
            QRegularExpressionMatchIterator it = re.globalMatch(text);
            while (it.hasNext()) {
                QRegularExpressionMatch m = it.next();
                // Skip lengths Qt won't accept (5 or 7 hex digits).
                const int digits = m.capturedLength() - 1;
                if (digits != 3 && digits != 4 && digits != 6 && digits != 8)
                    continue;
                matches.append(qMakePair(m.capturedStart(), m.captured()));
            }
            if (!matches.isEmpty())
                next.insert(block.blockNumber(), std::move(matches));
        }
        block = block.next();
    }

    if (next != m_colorSwatches)
        m_colorSwatches = std::move(next);
}

bool EditorView::handleColorSwatchClick(const QPoint &viewportPos)
{
    if (m_colorSwatches.isEmpty())
        return false;

    QTextCursor clickCur = cursorForPosition(viewportPos);
    if (clickCur.isNull())
        return false;

    const QTextBlock block = clickCur.block();
    if (!block.isValid())
        return false;

    const auto it = m_colorSwatches.constFind(block.blockNumber());
    if (it == m_colorSwatches.constEnd())
        return false;

    const QFontMetrics fm(font());
    const QRectF blockGeo = blockBoundingGeometry(block).translated(contentOffset());
    const int swatchY = qRound(blockGeo.top() + (blockGeo.height() - kSwatchSize) / 2.0);

    for (const auto &entry : it.value()) {
        const int   col = entry.first;
        const QString &hex = entry.second;
        const QString prefix = block.text().left(col);
        const int charX = qRound(blockGeo.left() + fm.horizontalAdvance(prefix));
        const QRect swatchRect(charX - kSwatchSize - kSwatchPadding,
                               swatchY, kSwatchSize, kSwatchSize);
        if (!swatchRect.contains(viewportPos))
            continue;

        QColor current = colorFromHex(hex);
        if (!current.isValid())
            current = Qt::white;
        QColor picked = QColorDialog::getColor(
            current, this, tr("Pick Color"),
            QColorDialog::ShowAlphaChannel);
        if (!picked.isValid())
            return true;  // user cancelled — still consumed the click

        const QString replacement = hexFromColor(picked, hex);
        if (replacement.isEmpty() || replacement == hex)
            return true;

        // Replace the matched range in the document.
        QTextCursor edit(document());
        edit.setPosition(block.position() + col);
        edit.setPosition(block.position() + col + hex.size(),
                         QTextCursor::KeepAnchor);
        edit.beginEditBlock();
        edit.insertText(replacement);
        edit.endEditBlock();
        return true;
    }
    return false;
}

void EditorView::rescanTodoMarkers()
{
    static const QRegularExpression re(
        QStringLiteral("(^|[^A-Za-z0-9_])(TODO|FIXME|HACK|NOTE):"));

    QHash<int, QString> next;
    QTextBlock block = document()->firstBlock();
    while (block.isValid()) {
        const QString text = block.text();
        QRegularExpressionMatch m = re.match(text);
        if (m.hasMatch()) {
            // 1-based line; capture group 2 is the keyword
            next.insert(block.blockNumber() + 1, m.captured(2));
        }
        block = block.next();
    }

    if (next != m_todoMarkers) {
        m_todoMarkers = std::move(next);
        if (m_lineNumberArea)
            m_lineNumberArea->update();
    }
}

void EditorView::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;
    QTextEdit::ExtraSelection selection;
    selection.format.setBackground(QColor(36, 36, 36));
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor();
    selection.cursor.clearSelection();
    extraSelections.append(selection);

    // Append bracket matching highlights
    extraSelections.append(m_bracketSelections);

    setExtraSelections(extraSelections);
}

void EditorView::handleScroll(int)
{
    if (!m_isLargeFilePreview || m_isLoadingChunk) {
        return;
    }

    QScrollBar *bar = verticalScrollBar();
    const int remaining = bar->maximum() - bar->value();
    if (remaining < 50) {
        m_isLoadingChunk = true;
        emit requestMoreData();
    }
}

void EditorView::repositionFindBar()
{
    if (!m_findBar || !m_findBar->isVisible()) {
        return;
    }
    const QRect vp = viewport()->geometry();
    m_findBar->adjustSize();
    const int barW = qMin(m_findBar->sizeHint().width(), vp.width());
    const int barH = m_findBar->sizeHint().height();
    m_findBar->setGeometry(vp.right() - barW, vp.top(), barW, barH);
}

// ── LSP: Diagnostics ──────────────────────────────────────────────────────────

void EditorView::setDiagnostics(const QJsonArray &lspDiagnostics)
{
    m_diagMarks.clear();
    for (const QJsonValue &v : lspDiagnostics) {
        const QJsonObject d = v.toObject();
        const QJsonObject range = d["range"].toObject();
        const QJsonObject start = range["start"].toObject();
        const QJsonObject end   = range["end"].toObject();

        DiagnosticMark mark;
        mark.line      = start["line"].toInt();
        mark.startChar = start["character"].toInt();
        mark.endChar   = end["character"].toInt();
        mark.severity  = static_cast<DiagSeverity>(d["severity"].toInt(1));
        mark.message   = d["message"].toString();
        m_diagMarks.append(mark);
    }
    refreshDiagnosticSelections();
    m_lineNumberArea->update();
}

// ── Inlay Hints ──────────────────────────────────────────────────────────────

void EditorView::setInlayHints(const QList<InlayHint> &hints)
{
    m_inlayHints = hints;
    viewport()->update();
}

void EditorView::clearInlayHints()
{
    m_inlayHints.clear();
    viewport()->update();
}

void EditorView::setInlayHintsVisible(bool visible)
{
    if (m_showInlayHints == visible) return;
    m_showInlayHints = visible;
    viewport()->update();
}

int EditorView::firstVisibleBlockNumber() const
{
    return firstVisibleBlock().blockNumber();
}

void EditorView::refreshDiagnosticSelections()
{
    // Keep current-line highlight, then add diagnostic underlines on top
    QList<QTextEdit::ExtraSelection> selections;

    // Current line highlight (copy from highlightCurrentLine)
    {
        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(QColor(36, 36, 36));
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        sel.cursor = textCursor();
        sel.cursor.clearSelection();
        selections.append(sel);
    }

    // Diagnostic underlines
    for (const DiagnosticMark &mark : std::as_const(m_diagMarks)) {
        QTextBlock block = document()->findBlockByNumber(mark.line);
        if (!block.isValid()) continue;

        const int blockStart = block.position();
        const int startPos   = blockStart + qMin(mark.startChar, block.length() - 1);
        int       endPos     = blockStart + qMin(mark.endChar,   block.length() - 1);
        if (endPos <= startPos) endPos = startPos + 1;

        QTextEdit::ExtraSelection sel;

        QColor waveColor;
        switch (mark.severity) {
        case DiagSeverity::Error:   waveColor = QColor(0xF4, 0x47, 0x47); break;
        case DiagSeverity::Warning: waveColor = QColor(0xFF, 0xCC, 0x00); break;
        default:                    waveColor = QColor(0x75, 0xBF, 0xFF); break;
        }

        sel.format.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
        sel.format.setUnderlineColor(waveColor);
        sel.format.setToolTip(mark.message);

        sel.cursor = textCursor();
        sel.cursor.setPosition(startPos);
        sel.cursor.setPosition(endPos, QTextCursor::KeepAnchor);
        selections.append(sel);
    }

    // Review annotations (yellow-tinted background with tooltip)
    selections.append(m_reviewSelections);

    // Bracket-match highlights (kept stable across diagnostic refreshes so
    // the highlight does not disappear when the LSP pushes new diagnostics)
    selections.append(m_bracketSelections);

    setExtraSelections(selections);
}

// ── Inline review annotations ────────────────────────────────────────────────

void EditorView::setReviewAnnotations(const QList<ReviewAnnotation> &annotations)
{
    m_reviewAnnotations = annotations;
    m_reviewSelections.clear();

    for (const ReviewAnnotation &ann : annotations) {
        QTextBlock block = document()->findBlockByNumber(ann.line - 1);
        if (!block.isValid()) continue;

        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(QColor(50, 50, 20, 60));
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        sel.format.setToolTip(ann.comment);
        sel.cursor = QTextCursor(block);
        sel.cursor.clearSelection();
        m_reviewSelections.append(sel);
    }

    refreshDiagnosticSelections();
}

void EditorView::clearReviewAnnotations()
{
    m_reviewAnnotations.clear();
    m_reviewSelections.clear();
    m_currentReviewIdx = -1;
    refreshDiagnosticSelections();
}

void EditorView::nextReviewAnnotation()
{
    if (m_reviewAnnotations.isEmpty()) return;
    m_currentReviewIdx = (m_currentReviewIdx + 1) % m_reviewAnnotations.size();
    const auto &ann = m_reviewAnnotations[m_currentReviewIdx];
    QTextBlock block = document()->findBlockByNumber(ann.line - 1);
    if (block.isValid()) {
        QTextCursor cur(block);
        setTextCursor(cur);
        centerCursor();
    }
}

void EditorView::prevReviewAnnotation()
{
    if (m_reviewAnnotations.isEmpty()) return;
    m_currentReviewIdx = (m_currentReviewIdx - 1 + m_reviewAnnotations.size())
                         % m_reviewAnnotations.size();
    const auto &ann = m_reviewAnnotations[m_currentReviewIdx];
    QTextBlock block = document()->findBlockByNumber(ann.line - 1);
    if (block.isValid()) {
        QTextCursor cur(block);
        setTextCursor(cur);
        centerCursor();
    }
}

void EditorView::applyReviewAnnotation(int index)
{
    if (index < 0 || index >= m_reviewAnnotations.size()) return;
    const auto ann = m_reviewAnnotations[index];
    emit reviewAnnotationApplied(ann.line, ann.comment);
    m_reviewAnnotations.removeAt(index);
    if (m_currentReviewIdx >= m_reviewAnnotations.size())
        m_currentReviewIdx = m_reviewAnnotations.isEmpty() ? -1 : 0;
    setReviewAnnotations(m_reviewAnnotations); // rebuild selections
}

void EditorView::discardReviewAnnotation(int index)
{
    if (index < 0 || index >= m_reviewAnnotations.size()) return;
    const auto ann = m_reviewAnnotations[index];
    emit reviewAnnotationDiscarded(ann.line, ann.comment);
    m_reviewAnnotations.removeAt(index);
    if (m_currentReviewIdx >= m_reviewAnnotations.size())
        m_currentReviewIdx = m_reviewAnnotations.isEmpty() ? -1 : 0;
    setReviewAnnotations(m_reviewAnnotations); // rebuild selections
}

// ── Next Edit Suggestion (NES) gutter arrow ──────────────────────────────────

void EditorView::setNesLine(int line)
{
    m_nesLine = line;
    m_lineNumberArea->update();
}

void EditorView::clearNesLine()
{
    if (m_nesLine >= 0) {
        m_nesLine = -1;
        m_lineNumberArea->update();
    }
}

void EditorView::setDiffRanges(const QList<DiffRange> &ranges)
{
    m_diffRanges = ranges;
    if (m_lineNumberArea)
        m_lineNumberArea->update();
}

void EditorView::clearDiffRanges()
{
    m_diffRanges.clear();
    if (m_lineNumberArea)
        m_lineNumberArea->update();
}

void EditorView::setBlameData(const QHash<int, BlameInfo> &blameByLine)
{
    m_blameData = blameByLine;
    updateLineNumberAreaWidth(0);
    if (m_lineNumberArea)
        m_lineNumberArea->update();
}

void EditorView::clearBlameData()
{
    m_blameData.clear();
    m_showBlame = false;
    updateLineNumberAreaWidth(0);
    if (m_lineNumberArea)
        m_lineNumberArea->update();
}

void EditorView::setBlameVisible(bool visible)
{
    m_showBlame = visible;
    updateLineNumberAreaWidth(0);
    if (m_lineNumberArea)
        m_lineNumberArea->update();
}

// ── LSP: Format / apply text edits ───────────────────────────────────────────

void EditorView::applyTextEdits(const QJsonArray &edits)
{
    if (edits.isEmpty()) return;

    // Collect and sort edits bottom-to-top so earlier positions aren't shifted
    struct Edit {
        int     startLine, startChar, endLine, endChar;
        QString newText;
    };
    QVector<Edit> sorted;
    for (const QJsonValue &v : edits) {
        const QJsonObject obj   = v.toObject();
        const QJsonObject range = obj["range"].toObject();
        const QJsonObject start = range["start"].toObject();
        const QJsonObject end_  = range["end"].toObject();
        Edit e;
        e.startLine = start["line"].toInt();
        e.startChar = start["character"].toInt();
        e.endLine   = end_["line"].toInt();
        e.endChar   = end_["character"].toInt();
        e.newText   = obj["newText"].toString();
        sorted.append(e);
    }
    // Sort descending so we apply from bottom
    std::sort(sorted.begin(), sorted.end(), [](const Edit &a, const Edit &b) {
        if (a.startLine != b.startLine) return a.startLine > b.startLine;
        return a.startChar > b.startChar;
    });

    QTextCursor cur = textCursor();
    cur.beginEditBlock();
    for (const Edit &e : std::as_const(sorted)) {
        QTextBlock startBlock = document()->findBlockByNumber(e.startLine);
        QTextBlock endBlock   = document()->findBlockByNumber(e.endLine);
        if (!startBlock.isValid() || !endBlock.isValid()) continue;

        const int startPos = startBlock.position() +
                             qMin(e.startChar, startBlock.length() - 1);
        const int endPos   = endBlock.position() +
                             qMin(e.endChar,   endBlock.length() - 1);

        cur.setPosition(startPos);
        cur.setPosition(endPos, QTextCursor::KeepAnchor);
        cur.insertText(e.newText);
    }
    cur.endEditBlock();
    setTextCursor(cur);
}

// ── Ghost text (inline completion) ───────────────────────────────────────────

void EditorView::setGhostText(const QString &text)
{
    if (text.isEmpty())
        return;

    m_ghostText   = text;
    m_ghostBlock  = textCursor().blockNumber();
    m_ghostColumn = textCursor().columnNumber();
    viewport()->update();
}

void EditorView::clearGhostText()
{
    if (m_ghostText.isEmpty())
        return;

    m_ghostText.clear();
    m_ghostBlock  = -1;
    m_ghostColumn = -1;
    viewport()->update();
    emit ghostTextDismissed();
}

void EditorView::acceptGhostText()
{
    if (m_ghostText.isEmpty())
        return;

    const QString text = m_ghostText;
    m_ghostText.clear();
    m_ghostBlock  = -1;
    m_ghostColumn = -1;

    QTextCursor cur = textCursor();
    cur.insertText(text);
    setTextCursor(cur);
    viewport()->update();
    emit ghostTextAccepted();
}

void EditorView::acceptGhostTextWord()
{
    if (m_ghostText.isEmpty())
        return;

    // Find the end of the first word in the ghost text
    int pos = 0;
    const int len = m_ghostText.length();

    // Skip leading whitespace
    while (pos < len && m_ghostText[pos].isSpace())
        ++pos;
    // Advance through the word
    while (pos < len && !m_ghostText[pos].isSpace())
        ++pos;

    if (pos == 0)
        pos = 1; // accept at least one character

    const QString word = m_ghostText.left(pos);
    m_ghostText = m_ghostText.mid(pos);

    QTextCursor cur = textCursor();
    cur.insertText(word);
    setTextCursor(cur);

    if (m_ghostText.isEmpty()) {
        m_ghostBlock  = -1;
        m_ghostColumn = -1;
        viewport()->update();
        emit ghostTextAccepted();
    } else {
        // Update ghost position to new cursor
        m_ghostBlock  = textCursor().blockNumber();
        m_ghostColumn = textCursor().columnNumber();
        viewport()->update();
        emit ghostTextPartiallyAccepted();
    }
}

void EditorView::acceptNextWord()
{
    acceptGhostTextWord();
}

void EditorView::paintEvent(QPaintEvent *event)
{
    // Draw the normal editor content first
    QPlainTextEdit::paintEvent(event);

    // ── Indent guides ─────────────────────────────────────────────────────
    // Faint vertical lines at every indent level less than the line's actual
    // indentation (VS Code style). For empty/whitespace-only blocks we use
    // the indent level of the closest non-empty neighbouring block so the
    // guides remain visually continuous through blank-line gaps.
    if (m_showIndentGuides) {
        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing, false);

        const QFontMetrics fm(font());
        const int spaceW = qMax(1, fm.horizontalAdvance(QLatin1Char(' ')));
        const int tabW = qMax(1, qRound(tabStopDistance() / spaceW));
        const int guideStep = tabW * spaceW;  // pixel step per indent level
        const qreal baseX = contentOffset().x();  // where text begins in viewport

        const QColor guideColor(0x3e, 0x3e, 0x42);
        QPen guidePen(guideColor);
        guidePen.setStyle(Qt::SolidLine);
        guidePen.setWidth(1);
        painter.setPen(guidePen);

        // Lambda: count effective indent columns (tabs expand to next tab stop).
        // Returns -1 for blocks that are entirely whitespace (or empty), so
        // callers can fall back to a neighbouring block's indent level.
        auto indentColumns = [tabW](const QString &text) -> int {
            int spaces = 0;
            for (int i = 0; i < text.length(); ++i) {
                const QChar ch = text[i];
                if (ch == QLatin1Char(' ')) {
                    ++spaces;
                } else if (ch == QLatin1Char('\t')) {
                    spaces += tabW - (spaces % tabW);
                } else {
                    return spaces;
                }
            }
            return -1;  // blank or whitespace-only
        };

        const int viewportH = viewport()->height();
        QTextBlock block = firstVisibleBlock();
        while (block.isValid()) {
            const QRectF geo = blockBoundingGeometry(block).translated(contentOffset());
            if (geo.top() > viewportH)
                break;
            if (geo.bottom() < 0) {
                block = block.next();
                continue;
            }

            int spaces = indentColumns(block.text());
            if (spaces < 0) {
                // Blank line: search forward then backward for the nearest
                // non-empty block to inherit its indent. Cap the lookahead so
                // a giant blank region doesn't make us walk the document.
                constexpr int kMaxNeighbourScan = 200;
                int forward = -1;
                {
                    QTextBlock fb = block.next();
                    int steps = 0;
                    while (fb.isValid() && steps < kMaxNeighbourScan) {
                        const int s = indentColumns(fb.text());
                        if (s >= 0) { forward = s; break; }
                        fb = fb.next();
                        ++steps;
                    }
                }
                int backward = -1;
                {
                    QTextBlock bb = block.previous();
                    int steps = 0;
                    while (bb.isValid() && steps < kMaxNeighbourScan) {
                        const int s = indentColumns(bb.text());
                        if (s >= 0) { backward = s; break; }
                        bb = bb.previous();
                        ++steps;
                    }
                }
                spaces = qMax(forward, backward);
                if (spaces < 0)
                    spaces = 0;
            }

            const int levels = spaces / tabW;
            // Draw guides ONLY for indent levels strictly LESS THAN the
            // block's own indentation — never at the same level the line
            // is indented to.
            const int top = qRound(geo.top());
            const int bottom = qRound(geo.bottom());
            for (int lvl = 1; lvl < levels; ++lvl) {
                const int x = qRound(baseX + lvl * guideStep);
                if (x < 0)
                    continue;
                if (x > viewport()->width())
                    break;
                painter.drawLine(x, top, x, bottom);
            }

            block = block.next();
        }
    }

    // ── Whitespace markers (Render Whitespace: all) ──────────────────────
    // Overlay dim-gray glyphs on top of spaces (·) and tabs (→). This is a
    // pure decoration: text in the document is unchanged, selection still
    // works exactly as before because we draw AFTER the base paint pass
    // and never replace the original characters.
    if (m_showWhitespace) {
        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setFont(font());

        const QFontMetrics fm(font());
        const QColor wsColor(0x3e, 0x3e, 0x42);
        painter.setPen(wsColor);

        const QString spaceGlyph = QString(QChar(0x00B7)); // middle dot ·
        const QString tabGlyph   = QString(QChar(0x2192)); // right arrow →

        // Pre-compute centering offset so the · glyph sits in the middle of
        // a space cell instead of left-aligned.
        const int spaceCellW = qMax(1, fm.horizontalAdvance(QLatin1Char(' ')));
        const int dotW = fm.horizontalAdvance(spaceGlyph);
        const int dotOffsetX = qMax(0, (spaceCellW - dotW) / 2);

        const qreal baseX = contentOffset().x();
        const int viewportH = viewport()->height();
        const int ascent = fm.ascent();

        QTextBlock block = firstVisibleBlock();
        while (block.isValid()) {
            const QRectF geo = blockBoundingGeometry(block).translated(contentOffset());
            if (geo.top() > viewportH)
                break;
            if (geo.bottom() < 0) {
                block = block.next();
                continue;
            }

            const QString text = block.text();
            if (text.isEmpty()) {
                block = block.next();
                continue;
            }

            const qreal y = geo.top() + ascent;
            // Use horizontalAdvance(text.left(i)) to obtain accurate X for
            // each character, honouring tab stops, kerning, and proportional
            // glyph widths if the editor font happens to be non-monospaced.
            for (int i = 0; i < text.length(); ++i) {
                const QChar ch = text.at(i);
                if (ch == QLatin1Char(' ')) {
                    const int x = qRound(baseX + fm.horizontalAdvance(text.left(i)));
                    painter.drawText(QPointF(x + dotOffsetX, y), spaceGlyph);
                } else if (ch == QLatin1Char('\t')) {
                    const int x = qRound(baseX + fm.horizontalAdvance(text.left(i)));
                    painter.drawText(QPointF(x + 2, y), tabGlyph);
                }
            }

            block = block.next();
        }
    }

    // ── Inline color swatches ────────────────────────────────────────────
    // Draw a small filled square in front of every cached `#hex` literal
    // on a visible block. The cache (m_colorSwatches) is rebuilt out of
    // band on contentsChange so this loop just walks visible blocks and
    // paints — no regex per frame.
    if (!m_colorSwatches.isEmpty()) {
        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QFontMetrics fm(font());
        const int viewportH = viewport()->height();
        const QPen borderPen(QColor(0x33, 0x33, 0x33));

        QTextBlock block = firstVisibleBlock();
        while (block.isValid()) {
            const QRectF geo = blockBoundingGeometry(block).translated(contentOffset());
            if (geo.top() > viewportH)
                break;
            if (geo.bottom() < 0) {
                block = block.next();
                continue;
            }

            const auto it = m_colorSwatches.constFind(block.blockNumber());
            if (it == m_colorSwatches.constEnd()) {
                block = block.next();
                continue;
            }

            const int swatchY = qRound(geo.top() + (geo.height() - kSwatchSize) / 2.0);
            const QString text = block.text();

            for (const auto &entry : it.value()) {
                const int   col = entry.first;
                const QString &hex = entry.second;
                if (col < 0 || col > text.size())
                    continue;
                const int charX = qRound(geo.left() + fm.horizontalAdvance(text.left(col)));
                const QRect swatchRect(charX - kSwatchSize - kSwatchPadding,
                                       swatchY, kSwatchSize, kSwatchSize);

                QColor fill = colorFromHex(hex);
                if (!fill.isValid())
                    continue;

                painter.setPen(borderPen);
                painter.setBrush(fill);
                painter.drawRoundedRect(swatchRect, 2, 2);
            }

            block = block.next();
        }
    }

    // Draw secondary multi-cursor carets and selections
    if (m_multiCursor->hasMultipleCursors()) {
        QPainter painter(viewport());
        paintMultiCursors(painter);
    }

    // ── Inlay hints ───────────────────────────────────────────────────────
    if (m_showInlayHints && !m_inlayHints.isEmpty()) {
        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing, false);

        QFont hintFont = font();
        hintFont.setPointSizeF(hintFont.pointSizeF() * 0.85);
        painter.setFont(hintFont);
        const QFontMetrics hfm(hintFont);
        const QFontMetrics efm(font());

        const QColor bgColor(70, 70, 70, 180);
        const QColor fgColor(180, 180, 220);

        for (const InlayHint &hint : std::as_const(m_inlayHints)) {
            QTextBlock block = document()->findBlockByNumber(hint.line);
            if (!block.isValid()) continue;

            const QRectF geo = blockBoundingGeometry(block).translated(contentOffset());
            if (geo.top() > viewport()->height() || geo.bottom() < 0)
                continue;

            // x position: character offset within the line
            const QString lineText = block.text().left(hint.character);
            const int xPos = efm.horizontalAdvance(lineText);

            // Build label with optional padding
            QString label;
            if (hint.paddingLeft)  label += QLatin1Char(' ');
            label += hint.label;
            if (hint.paddingRight) label += QLatin1Char(' ');

            const int labelW = hfm.horizontalAdvance(label) + 6; // 3px padding each side
            const int labelH = hfm.height();
            const int yTop = qRound(geo.top() + (geo.height() - labelH) / 2.0);

            // Background rounded rect
            const QRect bgRect(xPos + 2, yTop, labelW, labelH);
            painter.setPen(Qt::NoPen);
            painter.setBrush(bgColor);
            painter.drawRoundedRect(bgRect, 3, 3);

            // Text
            painter.setPen(fgColor);
            painter.drawText(bgRect.adjusted(3, 0, -3, 0),
                             Qt::AlignVCenter | Qt::AlignLeft, label);
        }
    }

    if (m_ghostText.isEmpty() || m_ghostBlock < 0)
        return;

    // Find the block where the ghost should be drawn
    QTextBlock block = document()->findBlockByNumber(m_ghostBlock);
    if (!block.isValid())
        return;

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Use the editor font at lower opacity for ghost text
    QFont ghostFont = font();
    painter.setFont(ghostFont);
    painter.setPen(QColor(128, 128, 128, 140));

    const QFontMetrics fm(ghostFont);

    // Split ghost text into lines for multi-line completions
    const QStringList ghostLines = m_ghostText.split(QLatin1Char('\n'));

    // First line: drawn after the cursor position on the same line
    const QRectF blockGeo = blockBoundingGeometry(block).translated(contentOffset());

    // Calculate the x position at the cursor column
    const QString lineText = block.text().left(m_ghostColumn);
    const int xOffset = fm.horizontalAdvance(lineText);

    // Draw first ghost line inline after the cursor
    if (!ghostLines.isEmpty()) {
        const qreal y = blockGeo.top() + fm.ascent();
        painter.drawText(QPointF(xOffset, y), ghostLines.first());
    }

    // Draw subsequent ghost lines below
    const int lineH = fm.height();
    for (int i = 1; i < ghostLines.size(); ++i) {
        const qreal y = blockGeo.top() + fm.ascent() + lineH * i;
        if (y > viewport()->height())
            break;
        painter.drawText(QPointF(0, y), ghostLines[i]);
    }
}

// ── Context menu ──────────────────────────────────────────────────────────────

void EditorView::contextMenuEvent(QContextMenuEvent *event)
{
    // Move cursor to click position so include detection & LSP work on the right line
    QTextCursor clickCursor = cursorForPosition(event->pos());
    if (!textCursor().hasSelection())
        setTextCursor(clickCursor);

    QMenu *menu = createStandardContextMenu();
    const bool hasSelection = textCursor().hasSelection();
    const QString lang = m_languageId;

    // Languages that have LSP support (clangd)
    const bool hasLsp = (lang == QLatin1String("cpp") || lang == QLatin1String("c"));
    // C/C++ family for #include
    const bool isCpp  = hasLsp;

    // ── AI actions (top, like VS) ─────────────────────────────────────────
    {
        menu->insertSeparator(menu->actions().isEmpty() ? nullptr : menu->actions().first());

        QMenu *aiMenu = new QMenu(tr("Copilot"), menu);
        const QString sel = textCursor().selectedText();
        const QString fp  = property("filePath").toString();

        QAction *explainAct = aiMenu->addAction(tr("Explain This"));
        connect(explainAct, &QAction::triggered, this, [this, sel, fp, lang]() {
            emit aiExplainRequested(sel, fp, lang);
        });
        QAction *reviewAct = aiMenu->addAction(tr("Review This"));
        connect(reviewAct, &QAction::triggered, this, [this, sel, fp, lang]() {
            emit aiReviewRequested(sel, fp, lang);
        });
        QAction *fixAct = aiMenu->addAction(tr("Fix This"));
        connect(fixAct, &QAction::triggered, this, [this, sel, fp, lang]() {
            emit aiFixRequested(sel, fp, lang);
        });
        QAction *testAct = aiMenu->addAction(tr("Generate Tests"));
        connect(testAct, &QAction::triggered, this, [this, sel, fp, lang]() {
            emit aiTestRequested(sel, fp, lang);
        });
        QAction *docAct = aiMenu->addAction(tr("Add Documentation"));
        connect(docAct, &QAction::triggered, this, [this, sel, fp, lang]() {
            emit aiDocRequested(sel, fp, lang);
        });

        menu->insertMenu(menu->actions().first(), aiMenu);
    }

    menu->addSeparator();

    // ── Navigation (LSP-dependent) ────────────────────────────────────────
    if (hasLsp) {
        QAction *gotoDef = menu->addAction(tr("Go to Definition\tF12"));
        connect(gotoDef, &QAction::triggered, this, &EditorView::goToDefinitionRequested);

        QAction *gotoDecl = menu->addAction(tr("Go to Declaration\tCtrl+F12"));
        connect(gotoDecl, &QAction::triggered, this, &EditorView::goToDeclarationRequested);

        QAction *peekDef = menu->addAction(tr("Peek Definition\tAlt+F12"));
        connect(peekDef, &QAction::triggered, this, &EditorView::goToDefinitionRequested);

        QAction *findRefs = menu->addAction(tr("Find All References\tShift+F12"));
        connect(findRefs, &QAction::triggered, this, &EditorView::findReferencesRequested);

        QAction *rename = menu->addAction(tr("Rename Symbol\tF2"));
        connect(rename, &QAction::triggered, this, &EditorView::renameSymbolRequested);
    }

    // Open #include file (C/C++ only)
    if (isCpp) {
        const QString includePath = includePathUnderCursor();
        if (!includePath.isEmpty()) {
            QAction *openInclude = menu->addAction(tr("Open \"%1\"").arg(includePath));
            connect(openInclude, &QAction::triggered, this, [this, includePath]() {
                emit openIncludeRequested(includePath);
            });
        }
    }

    menu->addSeparator();

    // ── Edit operations ───────────────────────────────────────────────────
    if (hasLsp) {
        QAction *format = menu->addAction(tr("Format Document\tCtrl+Shift+F"));
        connect(format, &QAction::triggered, this, &EditorView::formatDocumentRequested);
    }

    // Toggle Comment (Ctrl+/) — determine comment prefix by language
    {
        QString commentPrefix = QStringLiteral("// ");
        if (lang == QLatin1String("python") || lang == QLatin1String("shellscript")
            || lang == QLatin1String("yaml") || lang == QLatin1String("cmake")
            || lang == QLatin1String("ruby") || lang == QLatin1String("powershell"))
            commentPrefix = QStringLiteral("# ");
        else if (lang == QLatin1String("html") || lang == QLatin1String("xml"))
            commentPrefix = QStringLiteral("<!-- ");  // simplified
        else if (lang == QLatin1String("lua"))
            commentPrefix = QStringLiteral("-- ");
        else if (lang == QLatin1String("sql"))
            commentPrefix = QStringLiteral("-- ");

        const QString pfx = commentPrefix.trimmed(); // "//" or "#" for matching

        QAction *commentAct = menu->addAction(tr("Toggle Comment\tCtrl+/"));
        connect(commentAct, &QAction::triggered, this, [this, pfx, commentPrefix]() {
            QTextCursor cur = textCursor();
            cur.beginEditBlock();
            int startBlock = document()->findBlock(cur.selectionStart()).blockNumber();
            int endBlock   = document()->findBlock(cur.selectionEnd()).blockNumber();

            bool allCommented = true;
            for (int i = startBlock; i <= endBlock; ++i) {
                const QString text = document()->findBlockByNumber(i).text().trimmed();
                if (!text.startsWith(pfx)) {
                    allCommented = false;
                    break;
                }
            }

            for (int i = startBlock; i <= endBlock; ++i) {
                QTextBlock block = document()->findBlockByNumber(i);
                QTextCursor bc(block);
                if (allCommented) {
                    int pos = block.text().indexOf(pfx);
                    if (pos >= 0) {
                        bc.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, pos);
                        bc.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, pfx.length());
                        bc.removeSelectedText();
                        // Remove trailing space
                        if (block.text().length() > pos
                            && block.text().at(pos) == QLatin1Char(' ')) {
                            QTextCursor sc(block);
                            sc.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, pos);
                            sc.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
                            sc.removeSelectedText();
                        }
                    }
                } else {
                    bc.movePosition(QTextCursor::StartOfBlock);
                    bc.insertText(commentPrefix);
                }
            }
            cur.endEditBlock();
        });
    }

    // Find / Replace (Ctrl+F)
    QAction *findReplace = menu->addAction(tr("Find / Replace\tCtrl+F"));
    connect(findReplace, &QAction::triggered, this, &EditorView::showFindBar);

    menu->addSeparator();

    // ── Debug ─────────────────────────────────────────────────────────────
    QAction *bpAct = menu->addAction(tr("Toggle Breakpoint\tF9"));
    connect(bpAct, &QAction::triggered, this, [this]() {
        int line = textCursor().blockNumber() + 1;
        toggleBreakpoint(line);
    });

    menu->exec(event->globalPos());
    menu->deleteLater();
}

QString EditorView::includePathUnderCursor() const
{
    static const QRegularExpression includeRx(
        QStringLiteral("^\\s*#\\s*include\\s*[<\"]([^>\"]+)[>\"]"));

    const QString lineText = textCursor().block().text();
    const QRegularExpressionMatch match = includeRx.match(lineText);
    if (match.hasMatch())
        return match.captured(1);
    return {};
}

// ── Bracket matching ────────────────────────────────────────────────────────

QChar EditorView::matchingBracket(QChar ch)
{
    switch (ch.unicode()) {
    case '(':  return QLatin1Char(')');
    case ')':  return QLatin1Char('(');
    case '[':  return QLatin1Char(']');
    case ']':  return QLatin1Char('[');
    case '{':  return QLatin1Char('}');
    case '}':  return QLatin1Char('{');
    case '"':  return QLatin1Char('"');
    case '\'': return QLatin1Char('\'');
    default:   return QChar();
    }
}

// Scan limit (in characters) so very large files do not freeze the UI.
static constexpr int kBracketScanLimit = 10000;

namespace {
// Lightweight forward classifier for "is position `i` inside a string/char
// literal or comment relative to the surrounding line?"  We rebuild state from
// the start of the current block (cheap for typical line lengths) and walk to
// `i`.  This is intentionally simple — no preprocessor-aware logic — and it is
// good enough for skipping bracket matches inside obvious literals.
bool isInsideLiteralOrComment(const QTextDocument *doc, int absPos)
{
    if (absPos < 0 || absPos >= doc->characterCount())
        return false;
    const QTextBlock block = doc->findBlock(absPos);
    if (!block.isValid())
        return false;

    const int blockStart = block.position();
    const QString line   = block.text();
    const int col        = absPos - blockStart;
    if (col < 0 || col > line.size())
        return false;

    enum class S { Code, SLine, SBlock, SStr, SChar };
    S st = S::Code;
    for (int i = 0; i < col; ++i) {
        const QChar c  = line.at(i);
        const QChar nx = (i + 1 < line.size()) ? line.at(i + 1) : QChar();
        switch (st) {
        case S::Code:
            if (c == QLatin1Char('/') && nx == QLatin1Char('/')) { st = S::SLine; ++i; }
            else if (c == QLatin1Char('/') && nx == QLatin1Char('*')) { st = S::SBlock; ++i; }
            else if (c == QLatin1Char('"'))  st = S::SStr;
            else if (c == QLatin1Char('\'')) st = S::SChar;
            break;
        case S::SBlock:
            if (c == QLatin1Char('*') && nx == QLatin1Char('/')) { st = S::Code; ++i; }
            break;
        case S::SStr:
            if (c == QLatin1Char('\\')) { ++i; }
            else if (c == QLatin1Char('"')) st = S::Code;
            break;
        case S::SChar:
            if (c == QLatin1Char('\\')) { ++i; }
            else if (c == QLatin1Char('\'')) st = S::Code;
            break;
        case S::SLine:
            // line comment runs to EOL; we only ever scan within one line here
            break;
        }
    }
    return st != S::Code;
}
}

int EditorView::scanForMatchingBracket(int pos, QChar open, QChar close,
                                       bool forward) const
{
    const QTextDocument *doc = document();
    const int len = doc->characterCount();
    int depth = 1;

    if (forward) {
        const int stop = qMin(len, pos + 1 + kBracketScanLimit);
        for (int i = pos + 1; i < stop && depth > 0; ++i) {
            if (isInsideLiteralOrComment(doc, i))
                continue;
            const QChar c = doc->characterAt(i);
            if (c == open) ++depth;
            else if (c == close) --depth;
            if (depth == 0) return i;
        }
    } else {
        const int stop = qMax(-1, pos - 1 - kBracketScanLimit);
        for (int i = pos - 1; i > stop && depth > 0; --i) {
            if (isInsideLiteralOrComment(doc, i))
                continue;
            const QChar c = doc->characterAt(i);
            if (c == close) ++depth;
            else if (c == open) --depth;
            if (depth == 0) return i;
        }
    }
    return -1;
}

void EditorView::highlightMatchingBrackets()
{
    m_bracketSelections.clear();

    QTextCursor cur = textCursor();
    const QTextDocument *doc = document();
    const int pos = cur.position();

    // Check character at cursor and before cursor
    auto tryMatch = [&](int checkPos) -> bool {
        if (checkPos < 0 || checkPos >= doc->characterCount())
            return false;
        const QChar ch = doc->characterAt(checkPos);
        const QChar mate = matchingBracket(ch);
        if (mate.isNull() || ch == QLatin1Char('"') || ch == QLatin1Char('\''))
            return false;

        const bool isOpen = (ch == QLatin1Char('(') || ch == QLatin1Char('[')
                          || ch == QLatin1Char('{'));
        const int matchPos = scanForMatchingBracket(
            checkPos, isOpen ? ch : mate, isOpen ? mate : ch, isOpen);

        if (matchPos < 0)
            return false;

        // Subtle green-tinted background per UI/UX spec, with bold weight so
        // the bracket pair pops without distracting from surrounding code.
        const QColor hlColor(0x26, 0x4f, 0x4d);
        const QColor borderColor(0x3a, 0x6e, 0x6b);

        QTextEdit::ExtraSelection sel1;
        sel1.format.setBackground(hlColor);
        sel1.format.setFontWeight(QFont::Bold);
        sel1.format.setProperty(QTextFormat::OutlinePen,
                                QVariant::fromValue(QPen(borderColor)));
        sel1.cursor = QTextCursor(doc->findBlock(checkPos));
        sel1.cursor.setPosition(checkPos);
        sel1.cursor.setPosition(checkPos + 1, QTextCursor::KeepAnchor);

        QTextEdit::ExtraSelection sel2;
        sel2.format.setBackground(hlColor);
        sel2.format.setFontWeight(QFont::Bold);
        sel2.format.setProperty(QTextFormat::OutlinePen,
                                QVariant::fromValue(QPen(borderColor)));
        sel2.cursor = QTextCursor(doc->findBlock(matchPos));
        sel2.cursor.setPosition(matchPos);
        sel2.cursor.setPosition(matchPos + 1, QTextCursor::KeepAnchor);

        m_bracketSelections.append(sel1);
        m_bracketSelections.append(sel2);
        return true;
    };

    // Try character at cursor position first, then character before
    if (!tryMatch(pos))
        tryMatch(pos - 1);

    // Merge bracket selections with current line highlight
    highlightCurrentLine();
}

// ── PieceTableBuffer shadow buffer ──────────────────────────────────────────

PieceTableBuffer *EditorView::buffer() const
{
    return m_buffer.get();
}

QString EditorView::bufferText() const
{
    return m_buffer->text();
}

QString EditorView::bufferSlice(int start, int length) const
{
    return m_buffer->slice(start, length);
}

// ── Breakpoint gutter ─────────────────────────────────────────────────────────

void EditorView::toggleBreakpoint(int line)
{
    if (m_breakpointLines.contains(line)) {
        m_breakpointLines.remove(line);
        const QString fp = property("filePath").toString();
        emit breakpointToggled(fp, line, false);
    } else {
        m_breakpointLines.insert(line);
        const QString fp = property("filePath").toString();
        emit breakpointToggled(fp, line, true);
    }
    m_lineNumberArea->update();
}

void EditorView::setBreakpointLines(const QSet<int> &lines)
{
    m_breakpointLines = lines;
    m_lineNumberArea->update();
}

void EditorView::setCurrentDebugLine(int line)
{
    m_currentDebugLine = line;
    m_lineNumberArea->update();

    // Highlight the debug line with a yellow background
    if (line > 0) {
        QTextBlock blk = document()->findBlockByNumber(line - 1);
        if (blk.isValid()) {
            QTextCursor cursor(blk);
            cursor.movePosition(QTextCursor::EndOfBlock);
            setTextCursor(cursor);
            ensureCursorVisible();
        }
    }
}

void EditorView::lineNumberAreaMousePress(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    const int clickX = static_cast<int>(event->position().x());
    const int foldZoneStart = m_lineNumberArea->width() - 14;

    // Determine which line the user clicked on
    QTextBlock block = firstVisibleBlock();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid()) {
        if (event->position().y() >= top && event->position().y() < bottom) {
            // Fold gutter click
            if (clickX >= foldZoneStart && m_foldingEngine &&
                m_foldingEngine->toggleFold(block.blockNumber())) {
                return;
            }
            toggleBreakpoint(block.blockNumber() + 1); // 1-based
            return;
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
    }
}

int EditorView::lineFromGutterY(int y) const
{
    QTextBlock block = firstVisibleBlock();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    while (block.isValid()) {
        if (y >= top && y < bottom)
            return block.blockNumber() + 1; // 1-based
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
    }
    return 0; // below last block
}

void EditorView::lineNumberAreaContextMenu(QContextMenuEvent *event)
{
    const int line = lineFromGutterY(event->pos().y());
    if (line <= 0) {
        event->ignore();
        return;
    }
    showBreakpointContextMenu(line, event->globalPos());
    event->accept();
}

void EditorView::showBreakpointContextMenu(int line, const QPoint &globalPos)
{
    const bool hasBp = m_breakpointLines.contains(line);
    const bool enabled = isBreakpointEnabled(line);
    const QString existingCond = m_breakpointConditions.value(line);

    QMenu menu(this);
    // VS dark menu styling — keep consistent with rest of editor
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background-color: #252526; color: #d4d4d4; "
        "border: 1px solid #3c3c3c; }"
        "QMenu::item { padding: 5px 22px; }"
        "QMenu::item:selected { background-color: #094771; }"
        "QMenu::item:disabled { color: #6d6d6d; }"
        "QMenu::separator { height: 1px; background: #3c3c3c; margin: 4px 6px; }"
    ));

    QAction *toggleAct = menu.addAction(
        hasBp ? tr("Toggle Breakpoint (remove)") : tr("Toggle Breakpoint (add)"));
    QAction *editCondAct = menu.addAction(
        existingCond.isEmpty() ? tr("Add Condition…") : tr("Edit Condition…"));
    QAction *enableAct = menu.addAction(
        enabled ? tr("Disable Breakpoint") : tr("Enable Breakpoint"));
    menu.addSeparator();
    QAction *removeAct = menu.addAction(tr("Remove Breakpoint"));

    // If there's no breakpoint on this line, only "Toggle" makes sense.
    editCondAct->setEnabled(hasBp);
    enableAct->setEnabled(hasBp);
    removeAct->setEnabled(hasBp);

    QAction *chosen = menu.exec(globalPos);
    if (!chosen) return;

    if (chosen == toggleAct) {
        toggleBreakpoint(line);
    } else if (chosen == editCondAct) {
        // If no breakpoint exists yet, adding a condition implies adding the bp.
        if (!hasBp)
            toggleBreakpoint(line);
        editBreakpointCondition(line);
    } else if (chosen == enableAct) {
        setBreakpointEnabled(line, !enabled);
    } else if (chosen == removeAct) {
        if (hasBp) {
            // Reuse toggleBreakpoint: it will remove and emit breakpointToggled(false)
            toggleBreakpoint(line);
        }
    }
}

void EditorView::editBreakpointCondition(int line)
{
    const QString existing = m_breakpointConditions.value(line);
    BreakpointConditionDialog dlg(this, existing, line);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const QString newCond = dlg.condition();
    if (newCond == existing)
        return;
    setBreakpointCondition(line, newCond);
}

void EditorView::setBreakpointCondition(int line, const QString &condition)
{
    if (condition.isEmpty()) {
        if (m_breakpointConditions.remove(line) == 0)
            return;
    } else {
        if (m_breakpointConditions.value(line) == condition)
            return;
        m_breakpointConditions.insert(line, condition);
    }
    const QString fp = property("filePath").toString();
    // TODO: full integration — PostPluginBootstrap should listen and re-add the
    // breakpoint via IDebugAdapter so GDB picks up `-break-insert -c "<expr>"`.
    emit breakpointConditionChanged(fp, line, condition);
    m_lineNumberArea->update();
}

void EditorView::setBreakpointEnabled(int line, bool enabled)
{
    const bool wasEnabled = !m_disabledBreakpoints.contains(line);
    if (wasEnabled == enabled)
        return;
    if (enabled)
        m_disabledBreakpoints.remove(line);
    else
        m_disabledBreakpoints.insert(line);
    const QString fp = property("filePath").toString();
    // TODO: full integration — listener should call IDebugAdapter break-enable/disable.
    emit breakpointEnabledChanged(fp, line, enabled);
    m_lineNumberArea->update();
}

// ── Multi-cursor helpers ──────────────────────────────────────────────────────

MultiCursorEngine *EditorView::multiCursorEngine() const
{
    return m_multiCursor.get();
}

VimHandler *EditorView::vimHandler() const
{
    return m_vimHandler.get();
}

bool EditorView::hasMultipleCursors() const
{
    return m_multiCursor->hasMultipleCursors();
}

void EditorView::syncPrimaryCursorToEngine()
{
    // Ensure the engine knows about the current editor cursor as primary.
    if (m_multiCursor->cursorCount() == 0)
        m_multiCursor->setPrimaryCursor(textCursor());
    else {
        // Update the primary cursor position from the editor widget.
        auto curs = m_multiCursor->cursors();
        bool found = false;
        QTextCursor editorCur = textCursor();
        for (auto &c : curs) {
            if (c.position() == editorCur.position() &&
                c.anchor() == editorCur.anchor()) {
                found = true;
                break;
            }
        }
        if (!found)
            m_multiCursor->setPrimaryCursor(editorCur);
    }
}

void EditorView::paintMultiCursors(QPainter &painter)
{
    if (!m_multiCursor->hasMultipleCursors())
        return;

    painter.setRenderHint(QPainter::Antialiasing, false);

    const QFontMetrics fm(font());
    const int cursorWidth = 2;
    const QColor cursorColor(0xAE, 0xAF, 0xAD);          // light gray caret
    const QColor selectionColor(38, 79, 120, 160);         // blue selection with alpha

    for (auto &cur : m_multiCursor->cursors()) {
        QTextBlock block = document()->findBlock(cur.position());
        if (!block.isValid())
            continue;

        const QRectF blockGeo = blockBoundingGeometry(block).translated(contentOffset());
        const int col = cur.position() - block.position();
        const int xPos = fm.horizontalAdvance(block.text().left(col));

        // Draw selection background if cursor has a selection
        if (cur.hasSelection()) {
            int selStart = qMin(cur.anchor(), cur.position());
            int selEnd   = qMax(cur.anchor(), cur.position());

            // Iterate through blocks in the selection
            QTextBlock selBlock = document()->findBlock(selStart);
            while (selBlock.isValid() && selBlock.position() < selEnd) {
                const QRectF selBlockGeo = blockBoundingGeometry(selBlock).translated(contentOffset());
                int startInBlock = qMax(selStart - selBlock.position(), 0);
                int endInBlock   = qMin(selEnd - selBlock.position(), selBlock.length() - 1);

                int x1 = fm.horizontalAdvance(selBlock.text().left(startInBlock));
                int x2 = fm.horizontalAdvance(selBlock.text().left(endInBlock));

                QRectF selRect(x1, selBlockGeo.top(), x2 - x1, selBlockGeo.height());
                painter.fillRect(selRect, selectionColor);

                selBlock = selBlock.next();
            }
        }

        // Draw caret line
        QRectF caretRect(xPos, blockGeo.top(), cursorWidth, blockGeo.height());
        painter.fillRect(caretRect, cursorColor);
    }
}

// ── Debug variable hover tooltip ────────────────────────────────────────────

void EditorView::setLocalsSnapshot(const QHash<QString, QString> &snapshot)
{
    m_debugLocals = snapshot;
    // Invalidate cached tooltip identifier so the next hover re-resolves.
    m_lastTooltipIdent.clear();
}

void EditorView::clearLocalsSnapshot()
{
    m_debugLocals.clear();
    m_lastTooltipIdent.clear();
    QToolTip::hideText();
}

QString EditorView::identifierAt(const QPoint &viewportPos) const
{
    QTextCursor cur = cursorForPosition(viewportPos);
    if (cur.isNull())
        return {};

    // Manually expand to a C-style identifier [A-Za-z_][A-Za-z0-9_]*.
    // Doing it manually (rather than QTextCursor::WordUnderCursor) lets a
    // future change support member-access expressions like a.b.c.
    const QTextBlock block = cur.block();
    const QString    text  = block.text();
    int              col   = cur.positionInBlock();

    if (text.isEmpty())
        return {};
    if (col >= text.size())
        col = text.size() - 1;
    if (col < 0)
        return {};

    auto isIdentChar = [](QChar ch) {
        return ch.isLetterOrNumber() || ch == QLatin1Char('_');
    };

    // If we're not currently on an identifier char, try the char to the left
    // so that hovering just past the end of a name still resolves.
    if (!isIdentChar(text.at(col))) {
        if (col > 0 && isIdentChar(text.at(col - 1))) {
            --col;
        } else {
            return {};
        }
    }

    int start = col;
    while (start > 0 && isIdentChar(text.at(start - 1)))
        --start;
    int end = col;
    while (end + 1 < text.size() && isIdentChar(text.at(end + 1)))
        ++end;

    QString ident = text.mid(start, end - start + 1);
    if (ident.isEmpty())
        return {};
    // First char must not be a digit.
    if (ident.at(0).isDigit())
        return {};
    return ident;
}

bool EditorView::maybeShowDebugValueTooltip(const QPoint &viewportPos,
                                            const QPoint &globalPos)
{
    // No debug session active or no locals snapshot -> fall through to default
    // Qt tooltip handling.
    if (m_debugLocals.isEmpty())
        return false;

    const QString ident = identifierAt(viewportPos);
    if (ident.isEmpty()) {
        // Hovering over whitespace/punct -> drop any stale cached tooltip.
        m_lastTooltipIdent.clear();
        return false;
    }

    auto it = m_debugLocals.constFind(ident);
    if (it == m_debugLocals.constEnd()) {
        m_lastTooltipIdent.clear();
        return false;
    }

    // Cache: don't re-show the exact same tooltip if it's already up.
    if (ident == m_lastTooltipIdent && QToolTip::isVisible())
        return true;

    m_lastTooltipIdent = ident;
    const QString body = QStringLiteral("%1 = %2").arg(ident, it.value());
    QToolTip::showText(globalPos, body, viewport());
    return true;
}

bool EditorView::event(QEvent *ev)
{
    // QPlainTextEdit forwards tooltip events from the viewport up here as
    // ToolTip events too in some Qt versions; handle defensively.
    if (ev && ev->type() == QEvent::ToolTip) {
        auto *helpEvent = static_cast<QHelpEvent *>(ev);
        const QPoint viewportPos = viewport()
            ? viewport()->mapFromGlobal(helpEvent->globalPos())
            : helpEvent->pos();
        if (maybeShowDebugValueTooltip(viewportPos, helpEvent->globalPos())) {
            ev->accept();
            return true;
        }
    }
    return QPlainTextEdit::event(ev);
}

bool EditorView::eventFilter(QObject *watched, QEvent *ev)
{
    if (watched == viewport() && ev && ev->type() == QEvent::ToolTip) {
        auto *helpEvent = static_cast<QHelpEvent *>(ev);
        if (maybeShowDebugValueTooltip(helpEvent->pos(), helpEvent->globalPos())) {
            ev->accept();
            return true;
        }
        // If we have a snapshot but identifier didn't match, suppress stale
        // tooltips. Otherwise let Qt's default tooltip pipeline run.
        if (!m_debugLocals.isEmpty())
            QToolTip::hideText();
    }
    return QPlainTextEdit::eventFilter(watched, ev);
}
