#include "editorview.h"
#include "minimapwidget.h"
#include "piecetablebuffer.h"

#include <QCheckBox>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QToolButton>

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
      m_buffer(std::make_unique<PieceTableBuffer>())
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

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();

    // Minimap
    m_minimap = new MinimapWidget(this, this);
}

EditorView::~EditorView() = default;

void EditorView::setMinimapVisible(bool visible)
{
    m_minimapVisible = visible;
    if (m_minimap)
        m_minimap->setVisible(visible);
    updateLineNumberAreaWidth(0);   // refresh right viewport margin
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
    // 10px left margin (diagnostic dots / diff bars) + 8px right padding
    int space = 10 + charW * digits + 8;

    // Extra width for blame annotations
    if (m_showBlame && !m_blameData.isEmpty())
        space += fontMetrics().horizontalAdvance(QLatin1String("author12 abc12345 "));

    return space;
}

void EditorView::updateLineNumberAreaWidth(int)
{
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
    if (event->key() == Qt::Key_F && event->modifiers() == Qt::ControlModifier) {
        m_findBar->isVisible() ? hideFindBar() : showFindBar();
        return;
    }
    if (event->key() == Qt::Key_Escape && m_findBar->isVisible()) {
        hideFindBar();
        return;
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

    QPlainTextEdit::keyPressEvent(event);
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

            // Diagnostic dot (4 px wide, vertically centered)
            if (lineToSeverity.contains(blockNumber)) {
                const DiagSeverity sev = lineToSeverity[blockNumber];
                QColor dot;
                switch (sev) {
                case DiagSeverity::Error:   dot = QColor(0xF4, 0x47, 0x47); break;
                case DiagSeverity::Warning: dot = QColor(0xFF, 0xCC, 0x00); break;
                default:                    dot = QColor(0x75, 0xBF, 0xFF); break;
                }
                painter.setPen(Qt::NoPen);
                painter.setBrush(dot);
                const int dotSize = 5;
                painter.drawEllipse(2, top + (lineH - dotSize) / 2,
                                    dotSize, dotSize);
            }

            // Breakpoint indicator (red filled circle, 1-based line)
            if (m_breakpointLines.contains(blockNumber + 1)) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(0xE5, 0x1A, 0x1A));
                const int bpSize = 10;
                painter.drawEllipse(2, top + (lineH - bpSize) / 2,
                                    bpSize, bpSize);
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
        }

        block = block.next();
        top    = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
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
    if (!m_findBar->isVisible()) {
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
    m_lineNumberArea->update();
}

void EditorView::clearDiffRanges()
{
    m_diffRanges.clear();
    m_lineNumberArea->update();
}

void EditorView::setBlameData(const QHash<int, BlameInfo> &blameByLine)
{
    m_blameData = blameByLine;
    updateLineNumberAreaWidth(0);
    m_lineNumberArea->update();
}

void EditorView::clearBlameData()
{
    m_blameData.clear();
    m_showBlame = false;
    updateLineNumberAreaWidth(0);
    m_lineNumberArea->update();
}

void EditorView::setBlameVisible(bool visible)
{
    m_showBlame = visible;
    updateLineNumberAreaWidth(0);
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

    // Determine which line the user clicked on
    QTextBlock block = firstVisibleBlock();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid()) {
        if (event->position().y() >= top && event->position().y() < bottom) {
            toggleBreakpoint(block.blockNumber() + 1); // 1-based
            return;
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
    }
}
