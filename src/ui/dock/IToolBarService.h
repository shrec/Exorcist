#pragma once

/// IToolBarService — Plugin interface for creating and managing
/// dockable toolbars in the Exorcist IDE.
///
/// Plugins obtain this service from ServiceRegistry under the key
/// "toolBarService" and use it to create, populate, and control
/// toolbars without depending on the concrete dock implementation.

#include <QObject>
#include <QString>

class QAction;
class QWidget;

class IToolBarService
{
public:
    virtual ~IToolBarService() = default;

    /// Create a new toolbar. Returns nullptr if the ID already exists.
    virtual QWidget *createToolBar(const QString &id,
                                    const QString &title) = 0;

    /// Get a toolbar by ID. Returns nullptr if not found.
    virtual QWidget *toolBar(const QString &id) const = 0;

    /// Remove (and delete) a toolbar by ID.
    virtual bool removeToolBar(const QString &id) = 0;

    /// Add an action to an existing toolbar.
    virtual void addAction(const QString &toolBarId, QAction *action) = 0;

    /// Add a widget to an existing toolbar.
    virtual void addWidget(const QString &toolBarId, QWidget *widget) = 0;

    /// Add a separator to an existing toolbar.
    virtual void addSeparator(const QString &toolBarId) = 0;

    /// Show or hide a toolbar.
    virtual void setToolBarVisible(const QString &toolBarId, bool visible) = 0;

    /// All toolbar IDs.
    virtual QStringList toolBarIds() const = 0;
};

#define EXORCIST_ITOOLBARSERVICE_IID "org.exorcist.IToolBarService/1.0"

Q_DECLARE_INTERFACE(IToolBarService, EXORCIST_ITOOLBARSERVICE_IID)
