#pragma once

#include "DockTypes.h"

#include <QToolBar>
#include <QPoint>

namespace exdock {

class DockToolBarManager;

/// A Visual Studio–style dockable toolbar.
///
/// Unlike a standard QToolBar, DockToolBar:
/// - Can be docked into bands (rows) at any window edge
/// - Supports drag repositioning between edges and bands
/// - Has a drag handle (grip) on the left/top
/// - Has a chevron overflow button when actions don't fit
/// - Can float as a small top-level palette window
/// - Supports show/hide via context menu
/// - Is fully themeable via QSS object names
///
/// Toolbar identity is preserved via a unique string ID so that
/// layout state can be saved and restored across sessions.
class DockToolBar : public QToolBar
{
    Q_OBJECT
    Q_PROPERTY(QString toolBarId READ toolBarId CONSTANT)
    Q_PROPERTY(bool    locked    READ isLocked  WRITE setLocked)

public:
    explicit DockToolBar(const QString &id, const QString &title,
                         QWidget *parent = nullptr);

    /// Unique toolbar ID (for state save/restore, plugin reference).
    QString toolBarId() const { return m_id; }

    /// Display title (shown in context menus, customisation).
    QString title() const { return m_title; }
    void setTitle(const QString &title);

    /// The edge this toolbar is currently docked to.
    ToolBarEdge edge() const { return m_edge; }

    /// The band (row) index at this edge. 0 = topmost/leftmost band.
    int band() const { return m_band; }

    /// Position within the band (left-to-right or top-to-bottom).
    int bandPosition() const { return m_bandPos; }

    /// Whether the toolbar is locked (cannot be dragged/repositioned).
    bool isLocked() const { return m_locked; }
    void setLocked(bool locked);

    /// Whether the toolbar is currently floating.
    bool isToolBarFloating() const { return m_floating; }

    /// Returns a toggle-visibility action for menus.
    QAction *toggleViewAction() const;

signals:
    /// Emitted when the user starts dragging this toolbar.
    void dragStarted(DockToolBar *bar, const QPoint &globalPos);

    /// Emitted when the toolbar has been dropped at a new position.
    void dropped(DockToolBar *bar);

    /// Toolbar visibility changed.
    void visibilityChanged(DockToolBar *bar, bool visible);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    friend class DockToolBarManager;
    friend class DockToolBarArea;

    void setEdge(ToolBarEdge e)   { m_edge = e; }
    void setBand(int b)           { m_band = b; }
    void setBandPosition(int p)   { m_bandPos = p; }
    void setToolBarFloating(bool f) { m_floating = f; }

    QString      m_id;
    QString      m_title;
    ToolBarEdge  m_edge     = ToolBarEdge::Top;
    int          m_band     = 0;
    int          m_bandPos  = 0;
    bool         m_locked   = false;
    bool         m_floating = false;

    // Drag state
    QPoint       m_dragStart;
    bool         m_dragging = false;
};

} // namespace exdock
