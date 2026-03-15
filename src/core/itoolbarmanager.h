#pragma once

/// IToolBarManager — Abstract interface for toolbar management.
///
/// Plugins and subsystems use this interface to create, populate,
/// and control toolbars at runtime. The core shell provides the
/// concrete implementation (DockToolBarManager). Plugins obtain this
/// via ServiceRegistry under key "toolBarManager".
///
/// Note: IToolBarService (in src/ui/dock/) is the legacy version.
/// This interface supersedes it with a richer API that supports
/// edge placement, bands, and state persistence.

#include <QString>
#include <QStringList>

class QAction;
class QWidget;

class IToolBarManager
{
public:
    virtual ~IToolBarManager() = default;

    /// Which edge of the window the toolbar docks at.
    enum Edge {
        Top,
        Bottom,
        Left,
        Right
    };

    // ── Toolbar lifecycle ───────────────────────────────────────────

    /// Create a new toolbar. Returns false if the ID already exists.
    virtual bool createToolBar(const QString &id,
                               const QString &title,
                               Edge edge = Top) = 0;

    /// Remove a toolbar by ID. Returns false if not found.
    virtual bool removeToolBar(const QString &id) = 0;

    // ── Action contribution ─────────────────────────────────────────

    /// Add an action to a toolbar.
    virtual void addAction(const QString &toolBarId, QAction *action) = 0;

    /// Add a widget to a toolbar.
    virtual void addWidget(const QString &toolBarId, QWidget *widget) = 0;

    /// Add a separator to a toolbar.
    virtual void addSeparator(const QString &toolBarId) = 0;

    // ── Visibility ──────────────────────────────────────────────────

    /// Show or hide a toolbar.
    virtual void setVisible(const QString &toolBarId, bool visible) = 0;

    /// Check if a toolbar is visible.
    virtual bool isVisible(const QString &toolBarId) const = 0;

    // ── Position ────────────────────────────────────────────────────

    /// Move a toolbar to a different edge.
    virtual void setEdge(const QString &toolBarId, Edge edge) = 0;

    // ── Lock ────────────────────────────────────────────────────────

    /// Lock or unlock all toolbars (prevent/allow drag-and-drop).
    virtual void setAllLocked(bool locked) = 0;

    /// Check if toolbars are locked.
    virtual bool allLocked() const = 0;

    // ── Query ───────────────────────────────────────────────────────

    /// All toolbar IDs.
    virtual QStringList toolBarIds() const = 0;

    // ── State ───────────────────────────────────────────────────────

    /// Save toolbar layout.
    virtual QByteArray saveLayout() const = 0;

    /// Restore toolbar layout.
    virtual bool restoreLayout(const QByteArray &data) = 0;
};
