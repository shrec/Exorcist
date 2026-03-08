#pragma once

#include "DockTypes.h"

#include <QWidget>

namespace exdock {

/// Visual Studio–style drop indicator overlay.
///
/// Shows semi-transparent drop zone rectangles over the main window
/// during dock widget drag operations. The user drags a title bar
/// or tab, and this overlay highlights where the widget will land
/// (left, right, top, bottom edge).
class DockDropOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit DockDropOverlay(QWidget *parent);

    /// Begin showing the overlay (call when drag starts).
    void showOverlay();

    /// Hide the overlay (call when drag ends or is cancelled).
    void hideOverlay();

    /// Update the highlighted drop zone based on cursor position.
    /// Returns the zone the cursor is in (None if not over a valid zone).
    SideBarArea updateDropZone(const QPoint &globalPos);

    /// Get the currently highlighted zone.
    SideBarArea currentZone() const { return m_currentZone; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    /// Compute drop zone rectangles based on current size.
    void computeZones();

    SideBarArea m_currentZone = SideBarArea::None;

    QRect m_leftZone;
    QRect m_rightZone;
    QRect m_topZone;
    QRect m_bottomZone;
};

} // namespace exdock
