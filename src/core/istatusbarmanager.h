#pragma once

/// IStatusBarManager — Abstract interface for status bar management.
///
/// Plugins and subsystems use this interface to contribute items to
/// the IDE status bar at runtime. The core shell provides the concrete
/// implementation. Plugins obtain this via ServiceRegistry under key
/// "statusBarManager".
///
/// Status bar items have an alignment (left or right) and a priority
/// that controls their position relative to other items on the same side.

#include <QString>

class QWidget;

class IStatusBarManager
{
public:
    virtual ~IStatusBarManager() = default;

    /// Which side of the status bar to place the item.
    enum Alignment {
        Left,
        Right
    };

    // ── Item management ─────────────────────────────────────────────

    /// Add a widget to the status bar. The manager does NOT take ownership
    /// of the widget (Qt parent hierarchy does). Higher priority values
    /// place the item closer to the edge.
    virtual void addWidget(const QString &id,
                           QWidget *widget,
                           Alignment alignment = Left,
                           int priority = 0) = 0;

    /// Remove a status bar item by ID. Returns false if not found.
    virtual bool removeWidget(const QString &id) = 0;

    /// Update the text of a simple text-based status item.
    /// This is a convenience — if the item is a QLabel, sets its text.
    virtual void setText(const QString &id, const QString &text) = 0;

    /// Set the tooltip for a status bar item.
    virtual void setTooltip(const QString &id, const QString &tooltip) = 0;

    /// Show or hide a status bar item.
    virtual void setVisible(const QString &id, bool visible) = 0;

    // ── Temporary messages ──────────────────────────────────────────

    /// Show a temporary message in the status bar that auto-clears.
    virtual void showMessage(const QString &text, int timeoutMs = 3000) = 0;

    // ── Query ───────────────────────────────────────────────────────

    /// All registered item IDs.
    virtual QStringList itemIds() const = 0;

    /// Get the widget for an item. Returns nullptr if not found.
    virtual QWidget *widget(const QString &id) const = 0;
};
