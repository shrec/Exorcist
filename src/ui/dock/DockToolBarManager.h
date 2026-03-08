#pragma once

#include "DockTypes.h"

#include <QObject>
#include <QHash>
#include <QJsonObject>

class QMainWindow;
class QMenu;

namespace exdock {

class DockToolBar;
class DockToolBarArea;

/// DockToolBarManager orchestrates all four DockToolBarAreas
/// (top, bottom, left, right) on a QMainWindow.
///
/// Responsibilities:
/// - Creates and manages DockToolBar instances by unique ID
/// - Handles drag-and-drop repositioning between edges and bands
/// - Provides a right-click context menu listing all toolbars
/// - Saves/restores toolbar layout state (JSON)
/// - Exposes API for plugins via IToolBarService
///
/// Toolbars are widgets that contain actions (buttons, widgets).
/// Each toolbar has a unique string ID for state persistence.
class DockToolBarManager : public QObject
{
    Q_OBJECT

public:
    explicit DockToolBarManager(QMainWindow *mainWindow, QObject *parent = nullptr);

    /// Create a new toolbar with the given ID and title.
    /// Returns nullptr if a toolbar with that ID already exists.
    DockToolBar *createToolBar(const QString &id, const QString &title,
                               ToolBarEdge edge = ToolBarEdge::Top,
                               int band = 0, int position = -1);

    /// Get a toolbar by ID. Returns nullptr if not found.
    DockToolBar *toolBar(const QString &id) const;

    /// Remove and delete a toolbar by ID. Returns true if found.
    bool removeToolBar(const QString &id);

    /// All managed toolbar IDs.
    QStringList toolBarIds() const;

    /// All managed toolbars.
    QList<DockToolBar *> toolBars() const;

    /// Move a toolbar to a different edge/band/position.
    void moveToolBar(const QString &id, ToolBarEdge edge,
                     int band = 0, int position = -1);

    /// Show or hide a toolbar by ID.
    void setToolBarVisible(const QString &id, bool visible);

    /// Lock/unlock all toolbars (prevent/allow dragging).
    void setAllLocked(bool locked);
    bool allLocked() const { return m_allLocked; }

    /// Get the area widget for a specific edge.
    DockToolBarArea *area(ToolBarEdge edge) const;

    /// Build a context menu with toggle actions for all toolbars.
    QMenu *createContextMenu(QWidget *parent = nullptr) const;

    /// Save/restore layout state.
    QJsonObject saveState() const;
    void restoreState(const QJsonObject &state);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void toolBarCreated(DockToolBar *bar);
    void toolBarRemoved(const QString &id);
    void toolBarMoved(DockToolBar *bar, ToolBarEdge fromEdge, ToolBarEdge toEdge);

private slots:
    void onToolBarDragStarted(DockToolBar *bar, const QPoint &globalPos);
    void onToolBarDropped(DockToolBar *bar);

private:
    void installAreas();
    void repositionAreas();
    DockToolBarArea *areaAt(const QPoint &globalPos) const;
    ToolBarEdge edgeForArea(DockToolBarArea *area) const;

    QMainWindow                       *m_mainWindow;
    QHash<ToolBarEdge, DockToolBarArea *> m_areas;
    QHash<QString, DockToolBar *>     m_toolBars;
    bool                               m_allLocked = false;
};

} // namespace exdock
