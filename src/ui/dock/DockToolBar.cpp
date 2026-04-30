#include "DockToolBar.h"

#include <QApplication>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>

namespace exdock {

DockToolBar::DockToolBar(const QString &id, const QString &title,
                         QWidget *parent)
    : QToolBar(title, parent)
    , m_id(id)
    , m_title(title)
{
    setObjectName(QStringLiteral("exdock-toolbar-%1").arg(id));
    // Disable Qt's own drag/float — DockToolBarManager owns all movement.
    // Keeping these true causes Qt's QMainWindowLayout-coupled paint path
    // (PE_PanelToolBar / PE_IndicatorToolBarHandle) to run even when the
    // toolbar is NOT a direct QMainWindow child, which crashes the custom
    // style plugin (qmodernwindowsstyle) that blindly casts parentWidget()
    // to QMainWindow without a null-check.
    setMovable(false);
    setFloatable(false);
    setIconSize(QSize(16, 16));
    setToolButtonStyle(Qt::ToolButtonIconOnly);

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
    // Deliberately NOT calling QToolBar::paintEvent() — it calls
    // style()->drawPrimitive(PE_PanelToolBar) and
    // style()->drawPrimitive(PE_IndicatorToolBarHandle) which the custom
    // qmodernwindowsstyle plugin crashes on when the toolbar is not a
    // direct QMainWindow child.  We own the full paint here.
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    // VS2022 dark toolbar background — #2d2d30
    p.fillRect(rect(), QColor(45, 45, 48));

    // Bottom border line (1px) — #3c3c3c
    if (orientation() == Qt::Horizontal) {
        p.setPen(QColor(60, 60, 60));
        p.drawLine(0, height() - 1, width(), height() - 1);
    } else {
        p.setPen(QColor(60, 60, 60));
        p.drawLine(width() - 1, 0, width() - 1, height());
    }

    // VS-like grip handle (dotted pattern) on the non-action edge
    if (!m_locked && !actions().isEmpty()) {
        const bool horiz = (orientation() == Qt::Horizontal);
        const QColor gripColor(85, 85, 85); // subtle grip dots
        if (horiz) {
            const int x = 3;
            for (int y = 5; y < height() - 5; y += 3)
                p.fillRect(x, y, 2, 2, gripColor);
        } else {
            const int y = 3;
            for (int x = 5; x < width() - 5; x += 3)
                p.fillRect(x, y, 2, 2, gripColor);
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
