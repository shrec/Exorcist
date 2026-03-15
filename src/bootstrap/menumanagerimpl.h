#pragma once

#include "core/imenumanager.h"

#include <QHash>
#include <QList>
#include <QObject>

class QMenuBar;
class QMainWindow;

/// Concrete implementation of IMenuManager that manages the actual
/// QMenuBar on the main window. Creates standard menus on demand
/// and allows plugins to contribute actions and custom menus.
class MenuManagerImpl : public QObject, public IMenuManager
{
    Q_OBJECT

public:
    explicit MenuManagerImpl(QMainWindow *window, QObject *parent = nullptr);

    // ── IMenuManager ────────────────────────────────────────────────

    QMenu *menu(MenuLocation location) override;
    QMenu *createMenu(const QString &id, const QString &title) override;
    bool removeMenu(const QString &id) override;

    void addAction(MenuLocation location, QAction *action,
                   const QString &group = {}) override;
    void addAction(const QString &menuId, QAction *action,
                   const QString &group = {}) override;
    QMenu *addSubmenu(MenuLocation location, const QString &title,
                      const QString &group = {}) override;
    void addSeparator(MenuLocation location) override;
    bool removeAction(QAction *action) override;

    void addEditorContextAction(QAction *action, const QString &group = {},
                                 const QString &when = {}) override;
    void addExplorerContextAction(QAction *action, const QString &group = {},
                                   const QString &when = {}) override;

    QStringList customMenuIds() const override;
    QMenu *customMenu(const QString &id) const override;

    // ── Context menu building (called by MainWindow) ────────────────

    /// Build an editor context menu from registered actions.
    QMenu *buildEditorContextMenu(QWidget *parent = nullptr) const;

    /// Build an explorer context menu from registered actions.
    QMenu *buildExplorerContextMenu(QWidget *parent = nullptr) const;

private:
    QMenu *ensureMenu(MenuLocation location);
    QString menuTitle(MenuLocation location) const;

    struct ContextAction {
        QAction *action = nullptr;
        QString  group;
        QString  when;
    };

    QMainWindow *m_window;
    QMenuBar    *m_menuBar;

    QHash<MenuLocation, QMenu *>  m_standardMenus;
    QHash<QString, QMenu *>       m_customMenus;

    QList<ContextAction> m_editorContextActions;
    QList<ContextAction> m_explorerContextActions;
};
