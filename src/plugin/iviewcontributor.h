#pragma once

#include <QString>

class QWidget;

// ── IViewContributor ──────────────────────────────────────────────────────────
//
// Plugins implement this to create custom dock panel widgets.
// Views are declared in PluginManifest::views. When the IDE activates a view,
// it calls createView() to obtain the widget.
//
// The IDE owns the returned widget (parented to `parent`).
//
// For non-C++ layers (C ABI, LuaJIT, DSL), the widget catalog provides
// pre-built widgets (ExTreeView, ExListView, etc.) that can be composed
// without raw QWidget access.

class IViewContributor
{
public:
    virtual ~IViewContributor() = default;

    /// Create the widget for the given viewId.
    /// `viewId` matches ViewContribution::id from the manifest.
    /// Called lazily — only when the view is first shown.
    virtual QWidget *createView(const QString &viewId,
                                QWidget *parent) = 0;

    /// Called when the view becomes visible/hidden.
    /// Plugins can use this to start/stop background work.
    virtual void viewVisibilityChanged(const QString &viewId, bool visible)
    {
        Q_UNUSED(viewId);
        Q_UNUSED(visible);
    }

    /// Called when the view is about to be destroyed.
    virtual void viewClosed(const QString &viewId)
    {
        Q_UNUSED(viewId);
    }
};

#define EXORCIST_VIEW_CONTRIBUTOR_IID "org.exorcist.IViewContributor/1.0"
Q_DECLARE_INTERFACE(IViewContributor, EXORCIST_VIEW_CONTRIBUTOR_IID)
