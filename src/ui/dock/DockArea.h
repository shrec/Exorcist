#pragma once

#include "DockTypes.h"

#include <QToolButton>
#include <QWidget>
#include <QList>

class QStackedWidget;

namespace exdock {

class DockTabBar;
class DockTitleBar;
class ExDockWidget;
class DockManager;

/// A dock area hosts one or more ExDockWidgets as tabs.
///
/// It contains a title bar (shown when single-widget), a tab bar
/// (shown when multiple widgets), and a stacked widget for contents.
class DockArea : public QWidget
{
    Q_OBJECT

public:
    explicit DockArea(DockManager *manager, QWidget *parent = nullptr);
    ~DockArea() override;

    /// Add a dock widget as a new tab.
    void addDockWidget(ExDockWidget *widget);

    /// Insert a dock widget at a specific tab position.
    void insertDockWidget(int index, ExDockWidget *widget);

    /// Remove a dock widget from this area. Returns true if the area is now empty.
    bool removeDockWidget(ExDockWidget *widget);

    /// Number of tabbed widgets.
    int count() const;

    /// Get dock widget at tab index.
    ExDockWidget *dockWidget(int index) const;

    /// Get index of a dock widget, or -1 if not in this area.
    int indexOf(ExDockWidget *widget) const;

    /// Get the currently visible dock widget.
    ExDockWidget *currentDockWidget() const;

    /// Set the current (visible) tab by index.
    void setCurrentIndex(int index);

    /// All dock widgets in this area.
    QList<ExDockWidget *> dockWidgets() const { return m_widgets; }

    /// Access sub-components.
    DockTabBar   *tabBar()   const { return m_tabBar; }
    DockTitleBar *titleBar() const { return m_titleBar; }

    /// The manager that owns this area.
    DockManager *manager() const { return m_manager; }

    // ── Size policy ──────────────────────────────────────────────────────

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    /// Emitted when the current tab changes.
    void currentChanged(int index);

    /// Emitted when all widgets have been removed (area is empty).
    void areaEmpty(DockArea *area);

    /// User initiated drag from a tab.
    void tabDragStarted(ExDockWidget *widget, const QPoint &globalPos);

    /// User initiated drag from the title bar.
    void titleDragStarted(const QPoint &globalPos);

    /// User wants to float this area.
    void floatRequested();

    /// User wants to auto-hide (pin) this area.
    void pinRequested();

    /// User wants to close this area's current tab.
    void closeRequested();

    /// User wants to close ALL tabs in this area (collapse panel).
    void closeAllRequested();

private slots:
    void onTabChanged(int index);
    void onTabCloseRequested(int index);
    void onTabDragStarted(int index, const QPoint &globalPos);
    void onTabContextMenu(int index, const QPoint &globalPos);

private:
    void updateTitleBarVisibility();

    DockManager    *m_manager;
    DockTitleBar   *m_titleBar;
    DockTabBar     *m_tabBar;
    QWidget        *m_tabRow;      // container: tab bar + collapse button
    QToolButton    *m_collapseBtn; // ∧ close-panel button shown in tab bar row
    QStackedWidget *m_stack;
    QList<ExDockWidget *> m_widgets;
};

} // namespace exdock
