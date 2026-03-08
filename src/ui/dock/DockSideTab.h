#pragma once

#include "DockTypes.h"

#include <QToolButton>

namespace exdock {

class ExDockWidget;

/// A tab button on the auto-hide sidebar.
///
/// Draws text vertically for left/right sidebars, horizontally for
/// top/bottom. Themed via palette (no hard-coded colours).
/// Clicking the tab signals the DockSideBar to show the overlay panel.
class DockSideTab : public QToolButton
{
    Q_OBJECT

public:
    explicit DockSideTab(ExDockWidget *dock, SideBarArea area,
                         QWidget *parent = nullptr);

    ExDockWidget *dockWidget() const { return m_dock; }
    SideBarArea   area()       const { return m_area; }

    QSize sizeHint()        const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    ExDockWidget *m_dock;
    SideBarArea   m_area;
};

} // namespace exdock
