#pragma once

#include "DockTypes.h"

#include <QWidget>
#include <QList>
#include <QHash>

class QVBoxLayout;
class QHBoxLayout;
class QBoxLayout;

namespace exdock {

class DockToolBar;

/// A "band" is a single row or column of toolbars.
struct ToolBarBand {
    QWidget            *container = nullptr;
    QBoxLayout         *layout    = nullptr;
    QList<DockToolBar *> bars;
};

/// DockToolBarArea manages all toolbar bands at one window edge.
///
/// Visual Studio allows multiple rows of toolbars at each edge.
/// This class handles:
/// - Multiple bands (rows/columns) of toolbars
/// - Adding/removing/reordering toolbars
/// - Drag repositioning between bands
/// - Band auto-removal when empty
/// - Saving/restoring layout state
class DockToolBarArea : public QWidget
{
    Q_OBJECT

public:
    explicit DockToolBarArea(ToolBarEdge edge, QWidget *parent = nullptr);

    /// The edge this area sits on.
    ToolBarEdge edge() const { return m_edge; }

    /// Add a toolbar to a specific band (creates band if needed).
    void addToolBar(DockToolBar *bar, int band = 0, int position = -1);

    /// Remove a toolbar from this area. Returns true if found.
    bool removeToolBar(DockToolBar *bar);

    /// Move a toolbar to a different band/position within this area.
    void moveToolBar(DockToolBar *bar, int targetBand, int position = -1);

    /// Number of bands currently active.
    int bandCount() const { return m_bands.size(); }

    /// All toolbars in this area, across all bands.
    QList<DockToolBar *> toolBars() const;

    /// All toolbars in a specific band.
    QList<DockToolBar *> toolBarsInBand(int band) const;

    /// Check if empty (no toolbars at all).
    bool isEmpty() const;

    /// Updates visibility: hides the area if no toolbars are visible.
    void updateVisibility();

signals:
    /// A toolbar was added to this area.
    void toolBarAdded(DockToolBar *bar);

    /// A toolbar was removed from this area.
    void toolBarRemoved(DockToolBar *bar);

private:
    void ensureBand(int band);
    void cleanupEmptyBands();
    Qt::Orientation bandOrientation() const;

    ToolBarEdge              m_edge;
    QBoxLayout              *m_rootLayout;
    QList<ToolBarBand>       m_bands;
};

} // namespace exdock
