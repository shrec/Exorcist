#include "DockToolBar.h"

#include <QApplication>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOption>

namespace exdock {

DockToolBar::DockToolBar(const QString &id, const QString &title,
                         QWidget *parent)
    : QToolBar(title, parent)
    , m_id(id)
    , m_title(title)
{
    setObjectName(QStringLiteral("exdock-toolbar-%1").arg(id));
    setMovable(true);
    setFloatable(true);
    setIconSize(QSize(16, 16));

    // VS-style: compact spacing, no border by default
    setContentsMargins(0, 0, 0, 0);
    layout()->setSpacing(1);
    layout()->setContentsMargins(4, 0, 2, 0);
}

void DockToolBar::setTitle(const QString &title)
{
    m_title = title;
    setWindowTitle(title);
}

void DockToolBar::setLocked(bool locked)
{
    m_locked = locked;
    setMovable(!locked);
}

QAction *DockToolBar::toggleViewAction() const
{
    return QToolBar::toggleViewAction();
}

// ── Mouse handling for drag ──────────────────────────────────────────────────

void DockToolBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !m_locked) {
        m_dragStart = event->globalPosition().toPoint();
        m_dragging  = false;
    }
    QToolBar::mousePressEvent(event);
}

void DockToolBar::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_locked && (event->buttons() & Qt::LeftButton) && !m_dragging) {
        if ((event->globalPosition().toPoint() - m_dragStart).manhattanLength()
                > QApplication::startDragDistance()) {
            m_dragging = true;
            emit dragStarted(this, event->globalPosition().toPoint());
        }
    }
    if (!m_dragging)
        QToolBar::mouseMoveEvent(event);
}

void DockToolBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_dragging) {
        m_dragging = false;
        emit dropped(this);
    }
    QToolBar::mouseReleaseEvent(event);
}

void DockToolBar::paintEvent(QPaintEvent *event)
{
    QToolBar::paintEvent(event);

    // Draw VS-like grip handle (dotted pattern) on the drag area
    if (!m_locked && !actions().isEmpty()) {
        QPainter p(this);
        const bool horiz = (orientation() == Qt::Horizontal);
        const QColor gripColor = palette().color(QPalette::Mid);

        if (horiz) {
            // Vertical dots on the left
            const int x = 3;
            for (int y = 4; y < height() - 4; y += 3) {
                p.fillRect(x, y, 2, 2, gripColor);
            }
        } else {
            // Horizontal dots on the top
            const int y = 3;
            for (int x = 4; x < width() - 4; x += 3) {
                p.fillRect(x, y, 2, 2, gripColor);
            }
        }
    }
}

void DockToolBar::showEvent(QShowEvent *event)
{
    QToolBar::showEvent(event);
    emit visibilityChanged(this, true);
}

void DockToolBar::hideEvent(QHideEvent *event)
{
    QToolBar::hideEvent(event);
    emit visibilityChanged(this, false);
}

void DockToolBar::resizeEvent(QResizeEvent *event)
{
    QToolBar::resizeEvent(event);
}

} // namespace exdock
