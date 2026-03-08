#pragma once

#include "DockTypes.h"

#include <QObject>
#include <QHash>
#include <QList>
#include <QJsonObject>

class QMainWindow;

namespace exdock {

class ExDockWidget;
class DockArea;
class DockSplitter;
class DockSideBar;
class DockOverlayPanel;

/// DockManager is the top-level orchestrator for the Exorcist
/// custom docking system.
///
/// It owns and coordinates:
/// - ExDockWidgets (all panels) — registered by unique string ID
/// - DockAreas (tabbed containers)
/// - DockSplitter (split layout tree)
/// - DockSideBars (auto-hide tabs at each edge)
/// - DockOverlayPanel (slide-out overlay for auto-hidden docks)
///
/// Pin/Unpin lifecycle (like Visual Studio):
///   Pinned   → dock is visible in the main layout (DockArea inside DockSplitter)
///   Unpinned → dock collapses to a sidebar tab; click to slide out an overlay
///   Close    → dock is fully hidden (no tab, no panel)
///
/// Plugins and MainWindow interact with DockManager to:
/// - Register/unregister dock widgets
/// - Pin, unpin, close, show docks
/// - Save and restore the full layout state
class DockManager : public QObject
{
    Q_OBJECT

public:
    explicit DockManager(QMainWindow *mainWindow, QObject *parent = nullptr);
    ~DockManager() override;

    /// The main window this manager is attached to.
    QMainWindow *mainWindow() const { return m_mainWindow; }

    // ── Dock widget management ──────────────────────────────────────

    /// Register a new dock widget. The manager takes ownership of the
    /// widget's lifecycle (pin/unpin/close). The widget must have a
    /// unique dockId set.
    void addDockWidget(ExDockWidget *dock, SideBarArea preferredSide,
                       bool startPinned = true);

    /// Unregister and delete a dock widget.
    void removeDockWidget(ExDockWidget *dock);

    /// Find a dock widget by ID. Returns nullptr if not found.
    ExDockWidget *dockWidget(const QString &id) const;

    /// All registered dock widget IDs.
    QStringList dockWidgetIds() const;

    /// All registered dock widgets.
    QList<ExDockWidget *> dockWidgets() const;

    // ── Pin / Unpin / Close ─────────────────────────────────────────

    /// Pin a dock widget (restore to visible layout in DockArea).
    void pinDock(ExDockWidget *dock);

    /// Unpin a dock widget (collapse to sidebar tab, auto-hide).
    void unpinDock(ExDockWidget *dock, SideBarArea side = SideBarArea::None);

    /// Close a dock widget (hide completely, no sidebar tab).
    void closeDock(ExDockWidget *dock);

    /// Show a previously closed dock (adds it pinned).
    void showDock(ExDockWidget *dock, SideBarArea preferredSide = SideBarArea::Left);

    /// Toggle dock visibility.
    void toggleDock(ExDockWidget *dock);

    /// Is the dock currently pinned (visible in layout)?
    bool isPinned(ExDockWidget *dock) const;

    /// Is the dock currently auto-hidden (has a sidebar tab)?
    bool isAutoHidden(ExDockWidget *dock) const;

    // ── Central content ─────────────────────────────────────────────

    /// Set the main editor/content widget (placed in the center of
    /// the splitter layout). Must be called before adding docks.
    void setCentralContent(QWidget *widget);

    /// Access the root splitter (for advanced layout).
    DockSplitter *rootSplitter() const { return m_rootSplitter; }

    /// Query the preferred side for a dock widget.
    SideBarArea inferSide(ExDockWidget *dock) const;

    // ── Sidebar access ──────────────────────────────────────────────

    /// Get the sidebar at a specific edge.
    DockSideBar *sideBar(SideBarArea area) const;

    /// Reposition sidebars to match the main window geometry.
    void repositionSideBars();

    // ── Layout state ────────────────────────────────────────────────

    /// Save the entire dock layout to JSON.
    QJsonObject saveState() const;

    /// Restore the dock layout from JSON.
    void restoreState(const QJsonObject &state);

signals:
    void dockPinned(ExDockWidget *dock);
    void dockUnpinned(ExDockWidget *dock);
    void dockClosed(ExDockWidget *dock);
    void dockShown(ExDockWidget *dock);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onSideBarTabClicked(ExDockWidget *dock, bool checked);
    void onOverlayPinRequested(ExDockWidget *dock);
    void onOverlayCloseRequested(ExDockWidget *dock);
    void onOverlayHidden();

private:
    void setupSideBars();
    DockArea *createDockArea(SideBarArea side);

    struct DockEntry {
        ExDockWidget *dock          = nullptr;
        SideBarArea   preferredSide = SideBarArea::Left;
        DockState     state         = DockState::Closed;
    };

    QMainWindow                    *m_mainWindow;
    QHash<QString, DockEntry>       m_docks;

    // Sidebars (auto-hide tabs) — left, right, bottom edges only.
    // Top edge is reserved for toolbars (VS standard).
    DockSideBar                    *m_leftBar   = nullptr;
    DockSideBar                    *m_rightBar  = nullptr;
    DockSideBar                    *m_bottomBar = nullptr;

    // Overlay panel for auto-hidden docks
    DockOverlayPanel               *m_overlay   = nullptr;

    // Main content layout
    DockSplitter                   *m_rootSplitter   = nullptr;
    DockSplitter                   *m_centerSplitter = nullptr;  // vertical: top|editor|bottom
    QWidget                        *m_centralWidget  = nullptr;

    // DockAreas by side (for initial placement)
    QHash<SideBarArea, DockArea *>  m_sideAreas;
};

} // namespace exdock
