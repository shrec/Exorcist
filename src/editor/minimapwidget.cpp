#include "minimapwidget.h"

#include <QPainter>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextBlock>
#include <QMouseEvent>

MinimapWidget::MinimapWidget(QPlainTextEdit *editor, QWidget *parent)
    : QWidget(parent)
    , m_editor(editor)
{
    setFixedWidth(100);
    setCursor(Qt::PointingHandCursor);

    connect(m_editor->document(), &QTextDocument::contentsChanged,
            this, [this]() { m_dirty = true; update(); });
    connect(m_editor->verticalScrollBar(), &QAbstractSlider::valueChanged,
            this, [this]() { update(); });
}

void MinimapWidget::updateContent()
{
    m_dirty = true;
    update();
}

void MinimapWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(25, 25, 25));

    if (!m_editor || !m_editor->document())
        return;

    const int totalLines = m_editor->document()->blockCount();
    if (totalLines == 0)
        return;

    const int w = width();
    const int h = height();
    const double lineH = qMax(1.0, static_cast<double>(h) / totalLines);

    // Draw minimap lines
    if (m_dirty || m_cache.size() != size()) {
        m_cache = QPixmap(size());
        m_cache.fill(QColor(25, 25, 25));
        QPainter cp(&m_cache);

        QTextBlock block = m_editor->document()->begin();
        int y = 0;
        while (block.isValid()) {
            const QString text = block.text();
            const int len = qMin(text.length(), w);
            if (len > 0) {
                // Draw a thin colored line representing code density
                const int brightness = qBound(40, 60 + len, 130);
                cp.setPen(QColor(brightness, brightness, brightness));
                cp.drawLine(2, y, 2 + qMin(len, w - 4), y);
            }
            y += qMax(1, static_cast<int>(lineH));
            block = block.next();
        }
        m_dirty = false;
    }

    p.drawPixmap(0, 0, m_cache);

    // Draw viewport indicator
    const QScrollBar *vbar = m_editor->verticalScrollBar();
    const int firstVisible = vbar->value();
    const int visibleLines = m_editor->height() / m_editor->fontMetrics().lineSpacing();
    const int viewY = static_cast<int>(firstVisible * lineH);
    const int viewH = qMax(10, static_cast<int>(visibleLines * lineH));

    p.fillRect(0, viewY, w, viewH, QColor(255, 255, 255, 30));
    p.setPen(QColor(100, 100, 100, 80));
    p.drawRect(0, viewY, w - 1, viewH);
}

void MinimapWidget::mousePressEvent(QMouseEvent *event)
{
    scrollToY(static_cast<int>(event->position().y()));
}

void MinimapWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
        scrollToY(static_cast<int>(event->position().y()));
}

void MinimapWidget::wheelEvent(QWheelEvent *event)
{
    // Forward wheel events to the editor
    if (m_editor)
        m_editor->verticalScrollBar()->event(event);
}

void MinimapWidget::scrollToY(int y)
{
    if (!m_editor) return;
    const int totalLines = m_editor->document()->blockCount();
    if (totalLines == 0) return;
    const double lineH = qMax(1.0, static_cast<double>(height()) / totalLines);
    const int line = static_cast<int>(y / lineH);

    QScrollBar *vbar = m_editor->verticalScrollBar();
    vbar->setValue(qBound(0, line, vbar->maximum()));
}
