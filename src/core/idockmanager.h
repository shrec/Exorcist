#pragma once

/// IDockManager — Abstract interface for dock panel management.
///
/// Plugins and subsystems use this interface to create, show, hide,
/// pin, and unpin dock panels at runtime. The core IDE provides the
/// concrete implementation (DockManager). Plugins never depend on
/// DockManager directly — they go through this interface, obtained
/// from ServiceRegistry under key "dockManager".
///
/// This is the foundation of the plugin-first UI model: plugins
/// contribute their own panels by calling addPanel(), and the core
/// shell handles layout, persistence, and lifecycle.

#include <QString>
#include <QStringList>

class QWidget;
class QAction;

class IDockManager
{
public:
    virtual ~IDockManager() = default;

    /// Where a panel can be placed.
    enum Area {
        Left,
        Right,
        Bottom,
        Center
    };

    // ── Panel lifecycle ─────────────────────────────────────────────

    /// Add a new dock panel. The manager takes ownership of the widget.
    /// Returns false if a panel with this ID already exists.
    virtual bool addPanel(const QString &id,
                          const QString &title,
                          QWidget *widget,
                          Area area = Left,
                          bool startVisible = false) = 0;

    /// Remove a dock panel by ID. Returns false if not found.
    virtual bool removePanel(const QString &id) = 0;

    // ── Visibility ──────────────────────────────────────────────────

    /// Show a panel (pin it into the layout).
    virtual void showPanel(const QString &id) = 0;

    /// Hide a panel (close it from layout, keep registered).
    virtual void hidePanel(const QString &id) = 0;

    /// Toggle panel visibility.
    virtual void togglePanel(const QString &id) = 0;

    /// Check if a panel is currently visible (pinned).
    virtual bool isPanelVisible(const QString &id) const = 0;

    // ── Pin / Auto-hide ─────────────────────────────────────────────

    /// Pin a panel (visible in layout).
    virtual void pinPanel(const QString &id) = 0;

    /// Unpin a panel (collapse to sidebar, auto-hide).
    virtual void unpinPanel(const QString &id) = 0;

    /// Check if a panel is pinned (vs auto-hidden or closed).
    virtual bool isPanelPinned(const QString &id) const = 0;

    // ── Query ───────────────────────────────────────────────────────

    /// All registered panel IDs.
    virtual QStringList panelIds() const = 0;

    /// Get the toggle-view action for a panel (for View menu).
    virtual QAction *panelToggleAction(const QString &id) const = 0;

    /// Get the content widget for a panel. Returns nullptr if not found.
    virtual QWidget *panelWidget(const QString &id) const = 0;

    // ── Layout state ────────────────────────────────────────────────

    /// Save the entire panel layout (for workspace persistence).
    virtual QByteArray saveLayout() const = 0;

    /// Restore a previously saved layout.
    virtual bool restoreLayout(const QByteArray &data) = 0;
};
