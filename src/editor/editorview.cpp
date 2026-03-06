#include "editorview.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLineEdit>
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
      m_isLoadingChunk(false)
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

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

void EditorView::setLargeFilePreview(const QString &text, bool isPartial)
{
    setPlainText(text);
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

    const int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void EditorView::updateLineNumberAreaWidth(int)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
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
            painter.drawText(0, top, m_lineNumberArea->width() - 6,
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

    setExtraSelections(selections);
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
