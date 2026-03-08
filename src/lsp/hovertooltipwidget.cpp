#include "hovertooltipwidget.h"
#include "../agent/markdownrenderer.h"

#include <QApplication>
#include <QEvent>
#include <QLabel>
#include <QScreen>
#include <QVBoxLayout>

HoverTooltipWidget::HoverTooltipWidget(QWidget *parent)
    : QFrame(parent, Qt::ToolTip | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
    setFrameShape(QFrame::StyledPanel);

    setStyleSheet(QStringLiteral(
        "HoverTooltipWidget {"
        "  background-color: #252526;"
        "  border: 1px solid #454545;"
        "  border-radius: 4px;"
        "  padding: 0px;"
        "}"
    ));

    m_label = new QLabel(this);
    m_label->setTextFormat(Qt::RichText);
    m_label->setWordWrap(true);
    m_label->setOpenExternalLinks(false);
    m_label->setMaximumWidth(560);
    m_label->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  color: #cccccc;"
        "  font-family: 'Segoe UI', sans-serif;"
        "  font-size: 13px;"
        "  padding: 6px 10px;"
        "  background: transparent;"
        "}"
        "code {"
        "  font-family: 'Cascadia Code', 'Consolas', monospace;"
        "  font-size: 12px;"
        "  background: #1e1e1e;"
        "  padding: 1px 4px;"
        "  border-radius: 3px;"
        "  color: #4ec9b0;"
        "}"
        "pre {"
        "  font-family: 'Cascadia Code', 'Consolas', monospace;"
        "  font-size: 12px;"
        "  background: #1e1e1e;"
        "  padding: 8px;"
        "  border-radius: 4px;"
        "  margin: 4px 0;"
        "}"
        "a { color: #4daafc; }"
        "b { color: #dcdcaa; }"
    ));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_label);
}

void HoverTooltipWidget::showTooltip(const QPoint &globalPos,
                                     const QString &markdown,
                                     QWidget *anchor)
{
    if (markdown.isEmpty()) {
        hideTooltip();
        return;
    }

    const QString html = MarkdownRenderer::toHtml(markdown);
    m_label->setText(html);

    adjustSize();

    // Position near the cursor, clamped to screen bounds
    QScreen *screen = anchor ? anchor->screen() : QApplication::primaryScreen();
    const QRect screenRect = screen ? screen->availableGeometry()
                                    : QRect(0, 0, 1920, 1080);

    int x = globalPos.x();
    int y = globalPos.y() + 20; // slightly below cursor

    const QSize sz = sizeHint();
    if (x + sz.width() > screenRect.right())
        x = screenRect.right() - sz.width();
    if (y + sz.height() > screenRect.bottom())
        y = globalPos.y() - sz.height() - 4; // flip above cursor
    if (x < screenRect.left()) x = screenRect.left();
    if (y < screenRect.top())  y = screenRect.top();

    move(x, y);
    show();
    raise();
}

void HoverTooltipWidget::hideTooltip()
{
    hide();
}

bool HoverTooltipWidget::isTooltipVisible() const
{
    return isVisible();
}

bool HoverTooltipWidget::event(QEvent *e)
{
    // Auto-hide on leave
    if (e->type() == QEvent::Leave) {
        hideTooltip();
        return true;
    }
    return QFrame::event(e);
}
