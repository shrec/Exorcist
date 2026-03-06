#include "editorview.h"

#include <QPainter>
#include <QTextBlock>

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

EditorView::EditorView(QWidget *parent)
    : QPlainTextEdit(parent),
      m_lineNumberArea(new LineNumberArea(this))
{
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setTabStopDistance(4 * fontMetrics().horizontalAdvance(' '));

    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &EditorView::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &EditorView::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &EditorView::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
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
}

void EditorView::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), QColor(26, 26, 26));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            const QString number = QString::number(blockNumber + 1);
            painter.setPen(QColor(140, 140, 140));
            painter.drawText(0, top, m_lineNumberArea->width() - 6,
                             fontMetrics().height(), Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
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
