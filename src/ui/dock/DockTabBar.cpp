#include "DockTabBar.h"
#include "DockArea.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QMouseEvent>

namespace exdock {

DockTabBar::DockTabBar(DockArea *area, QWidget *parent)
    : QTabBar(parent), m_area(area)
{
    setObjectName(QStringLiteral("exdock-tabbar"));
    setMovable(false);
    setTabsClosable(false);  // VS-style: close via title bar or context menu
    setExpanding(false);
    setElideMode(Qt::ElideRight);
    setUsesScrollButtons(true);
    setDocumentMode(true);
    setDrawBase(false);
    setAttribute(Qt::WA_Hover, true);

    connect(this, &QTabBar::tabCloseRequested,
            this, [this](int idx) { emit tabCloseRequested(idx); });
}

void DockTabBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStart = event->globalPosition().toPoint();
        m_dragIndex = tabAt(event->pos());
        m_dragging  = false;
    }
    QTabBar::mousePressEvent(event);
}

void DockTabBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragIndex >= 0 && !m_dragging &&
        (event->globalPosition().toPoint() - m_dragStart).manhattanLength()
            > QApplication::startDragDistance() * 2) {
        m_dragging = true;
        emit tabDragStarted(m_dragIndex, event->globalPosition().toPoint());
        return;
    }
    if (!m_dragging)
        QTabBar::mouseMoveEvent(event);
}

void DockTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragIndex = -1;
    m_dragging  = false;
    QTabBar::mouseReleaseEvent(event);
}

void DockTabBar::contextMenuEvent(QContextMenuEvent *event)
{
    const int idx = tabAt(event->pos());
    if (idx >= 0)
        emit tabContextMenu(idx, event->globalPos());
}

} // namespace exdock
