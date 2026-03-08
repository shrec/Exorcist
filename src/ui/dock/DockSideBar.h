#pragma once

#include "DockTypes.h"

#include <QWidget>
#include <QList>

class QBoxLayout;

namespace exdock {

class DockSideTab;
class ExDockWidget;

/// A narrow sidebar at one window edge that holds DockSideTabs
/// for auto-hidden dock panels.
///
/// Tabs are added when a dock is unpinned, removed when pinned back.
/// Clicking a tab signals the DockManager to show/hide the overlay.
/// The bar auto-hides itself when it has no tabs.
class DockSideBar : public QWidget
{
    Q_OBJECT

public:
    explicit DockSideBar(SideBarArea area, QWidget *parent = nullptr);

    SideBarArea area() const { return m_area; }

    /// Add a tab for an auto-hidden dock. Returns the created tab.
    DockSideTab *addTab(ExDockWidget *dock);

    /// Remove the tab for a dock. Hides the bar if empty.
    void removeTab(ExDockWidget *dock);

    /// Find the tab for a specific dock widget.
    DockSideTab *tabFor(ExDockWidget *dock) const;

    /// Uncheck all tabs (called when overlay is hidden).
    void uncheckAll();

    /// All tabs in this sidebar.
    const QList<DockSideTab *> &tabs() const { return m_tabs; }

    /// Is the sidebar empty?
    bool isEmpty() const { return m_tabs.isEmpty(); }

signals:
    /// Emitted when a tab is clicked (checked).
    void tabClicked(ExDockWidget *dock, bool checked);

private:
    SideBarArea            m_area;
    QBoxLayout            *m_layout;
    QList<DockSideTab *>   m_tabs;
};

} // namespace exdock
