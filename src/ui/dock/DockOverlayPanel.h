#pragma once

#include "DockTypes.h"

#include <QFrame>
#include <QTimer>

namespace exdock {

class ExDockWidget;
class DockSideBar;

/// A slide-out overlay panel that appears when an auto-hidden
/// dock's sidebar tab is clicked.
///
/// Behaviour mirrors Visual Studio:
/// - Slides out from the sidebar edge
/// - Contains the dock widget's content
/// - Auto-hides when the user clicks outside or focus leaves
/// - Has a title bar with pin (restore to docked) and close buttons
///
/// The panel is a top-level frameless window positioned relative
/// to the main window, overlapping the central area.
class DockOverlayPanel : public QFrame
{
    Q_OBJECT

public:
    explicit DockOverlayPanel(QWidget *mainWindow, QWidget *parent = nullptr);
    ~DockOverlayPanel() override;

    /// Show the overlay for a dock widget, sliding from the given side.
    void showForDock(ExDockWidget *dock, SideBarArea side,
                     const QRect &sideBarGeometry);

    /// Hide the overlay and detach the dock content.
    void hideOverlay();

    /// Force-hide the overlay immediately, bypassing mouse/focus checks.
    /// Used when the user explicitly clicks pin or close.
    void forceHideOverlay();

    /// The dock currently being displayed (nullptr if hidden).
    ExDockWidget *activeDock() const { return m_activeDock; }

    /// Whether the overlay is currently visible.
    bool isOverlayVisible() const { return m_activeDock != nullptr && isVisible(); }

    /// Set the auto-hide delay in ms (default 350).
    void setAutoHideDelay(int ms) { m_hideTimer.setInterval(ms); }

signals:
    /// User clicked the pin button — dock should be restored to docked state.
    void pinRequested(ExDockWidget *dock);

    /// User clicked close — dock should be hidden entirely.
    void closeRequested(ExDockWidget *dock);

    /// Overlay was hidden (by auto-hide or explicit close).
    void overlayHidden();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void scheduleHide();
    void cancelHide();
    QRect computeGeometry(SideBarArea side, const QRect &sideBarGeo) const;

    QWidget      *m_mainWindow;
    ExDockWidget *m_activeDock = nullptr;
    SideBarArea   m_activeSide = SideBarArea::None;
    QWidget      *m_overlayTitleBar = nullptr;
    QTimer        m_hideTimer;
};

} // namespace exdock
