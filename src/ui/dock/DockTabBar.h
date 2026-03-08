#pragma once

#include <QTabBar>

namespace exdock {

class DockArea;

/// Tab bar for a DockArea. Supports drag-out to undock/move tabs, close
/// buttons, and context menus.
class DockTabBar : public QTabBar
{
    Q_OBJECT

public:
    explicit DockTabBar(DockArea *area, QWidget *parent = nullptr);

signals:
    /// User dragged a tab out of the bar (to start a float / re-dock).
    void tabDragStarted(int index, const QPoint &globalPos);

    /// User requested close for a specific tab.
    void tabCloseRequested(int index);

    /// User right-clicked a tab.
    void tabContextMenu(int index, const QPoint &globalPos);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    DockArea *m_area;
    QPoint    m_dragStart;
    int       m_dragIndex = -1;
    bool      m_dragging  = false;
};

} // namespace exdock
