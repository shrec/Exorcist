#pragma once

#include "DockTypes.h"

#include <QWidget>
#include <QString>

namespace exdock {

class DockArea;
class DockManager;

/// A single dockable panel in the Exorcist docking system.
///
/// Replaces QDockWidget with a richer API: auto-hide, floating,
/// grouping, and theme integration.
class ExDockWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString dockId READ dockId CONSTANT)

public:
    explicit ExDockWidget(const QString &title, QWidget *parent = nullptr);

    /// Set the actual content widget.
    void setContentWidget(QWidget *widget);
    QWidget *contentWidget() const { return m_content; }

    /// Unique dock ID (for state save/restore).
    QString dockId() const { return m_id; }
    void setDockId(const QString &id) { m_id = id; }

    /// Title (displayed in tab and title bar).
    QString title() const { return m_title; }
    void setTitle(const QString &title);

    /// Current state.
    DockState state() const { return m_state; }

    /// The area this widget belongs to (null if floating alone).
    DockArea *dockArea() const { return m_area; }

    /// Close (hide) this dock widget.
    void closeDock();

    /// Toggle visibility. Returns an action suitable for a View menu.
    QAction *toggleViewAction() const { return m_toggleAction; }

    // ── Size policy ─────────────────────────────────────────────────

    /// Preferred size hint for this dock (used by splitter allocation).
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    /// Set the preferred/initial size (used by DockArea / DockSplitter).
    void setPreferredSize(const QSize &size) { m_preferredSize = size; }
    QSize preferredSize() const { return m_preferredSize; }

    /// Set minimum size constraint.
    void setMinimumDockSize(const QSize &size) { m_minimumDockSize = size; }
    QSize minimumDockSize() const { return m_minimumDockSize; }

signals:
    void titleChanged(const QString &newTitle);
    void stateChanged(DockState newState);
    void closed();

private:
    friend class DockArea;
    friend class DockManager;

    void setDockArea(DockArea *area) { m_area = area; }
    void setState(DockState s);

    QString    m_id;
    QString    m_title;
    DockState  m_state = DockState::Closed;
    QWidget   *m_content = nullptr;
    DockArea  *m_area = nullptr;
    QAction   *m_toggleAction = nullptr;
    QSize      m_preferredSize  = {250, 200};
    QSize      m_minimumDockSize = {100, 60};
};

} // namespace exdock
