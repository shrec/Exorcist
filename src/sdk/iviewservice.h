#pragma once

#include <QString>

class QWidget;

// ── View Service ─────────────────────────────────────────────────────────────
//
// Stable SDK interface for registering and managing plugin-contributed views.
// Plugins use this to add panels, sidebars, and tree views.

class ITreeDataProvider;  // forward — plugins implement this

class IViewService
{
public:
    virtual ~IViewService() = default;

    /// Register a panel widget contributed by a plugin.
    /// The IDE takes ownership of the widget and places it according to
    /// the ViewContribution in the plugin manifest.
    virtual void registerPanel(const QString &viewId,
                               const QString &title,
                               QWidget *widget) = 0;

    /// Remove a previously registered panel.
    virtual void unregisterPanel(const QString &viewId) = 0;

    /// Show a registered view (bring it to front / make visible).
    virtual void showView(const QString &viewId) = 0;

    /// Hide a registered view without destroying it.
    virtual void hideView(const QString &viewId) = 0;

    /// Check whether a view is currently visible.
    virtual bool isViewVisible(const QString &viewId) const = 0;
};
