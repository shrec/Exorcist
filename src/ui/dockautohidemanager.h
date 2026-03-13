#pragma once

#include <QHash>
#include <QObject>
#include <QWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QDockWidget>
#include <QEvent>
#include <QPainter>
#include <QStyleOption>

class QMainWindow;

// ── AutoHideTab ───────────────────────────────────────────────────────────────
//
// A single tab button on the sidebar. Text is drawn vertically for left/right
// bars, horizontally for bottom bar.

class AutoHideTab : public QToolButton
{
    Q_OBJECT

public:
    explicit AutoHideTab(const QString &text, Qt::DockWidgetArea area,
                         QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    Qt::DockWidgetArea m_area;
};

// ── AutoHideSideBar ───────────────────────────────────────────────────────────
//
// A narrow bar that hosts AutoHideTab buttons for a single edge.

class AutoHideSideBar : public QWidget
{
    Q_OBJECT

public:
    explicit AutoHideSideBar(Qt::DockWidgetArea area, QWidget *parent = nullptr);

    AutoHideTab *addTab(const QString &title);
    void removeTab(AutoHideTab *tab);

    const QList<AutoHideTab *> &tabs() const { return m_tabs; }
    Qt::DockWidgetArea area() const { return m_area; }

private:
    Qt::DockWidgetArea   m_area;
    QBoxLayout          *m_layout;
    QList<AutoHideTab *> m_tabs;
};

// ── DockAutoHideManager ───────────────────────────────────────────────────────
//
// Coordinates auto-hide behavior for all dock widgets in a QMainWindow.
// When a dock is "unpinned":
//   - It is hidden from the normal dock layout
//   - A tab appears on the corresponding sidebar
//   - Clicking the tab shows the dock as a floating overlay
//   - Moving focus away auto-hides the dock again
//
// Persistence: saves auto-hidden dock names to QSettings.

class DockAutoHideManager : public QObject
{
    Q_OBJECT

public:
    explicit DockAutoHideManager(QMainWindow *mainWindow);

    /// Register a dock widget for auto-hide support.
    /// Call this for every dock that should have a pin/unpin button.
    void registerDock(QDockWidget *dock);

    /// Unpin a dock (collapse to sidebar tab).
    void unpinDock(QDockWidget *dock);

    /// Pin a dock (restore to normal dock layout).
    void pinDock(QDockWidget *dock);

    /// Is this dock currently auto-hidden?
    bool isAutoHidden(QDockWidget *dock) const;

    /// Save/restore auto-hide state to QSettings.
    void saveState();
    void restoreState();

    /// Access sidebars (for layout integration).
    AutoHideSideBar *sidebar(Qt::DockWidgetArea area) const;

    /// Reposition sidebars to match main window geometry.
    void repositionSidebars();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void showOverlay(QDockWidget *dock);
    void hideOverlay();
    void uncheckAllTabs();
    Qt::DockWidgetArea dockArea(QDockWidget *dock) const;

    struct AutoHideEntry {
        QDockWidget     *dock  = nullptr;
        AutoHideTab     *tab   = nullptr;
        Qt::DockWidgetArea area = Qt::LeftDockWidgetArea;
    };

    QMainWindow      *m_mainWindow;
    AutoHideSideBar  *m_leftBar;
    AutoHideSideBar  *m_rightBar;
    AutoHideSideBar  *m_bottomBar;

    QHash<QDockWidget *, AutoHideEntry> m_entries;
    QDockWidget      *m_activeOverlay = nullptr;
    QTimer            m_hideTimer;
};
