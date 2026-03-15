#include "menumanagerimpl.h"

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>

MenuManagerImpl::MenuManagerImpl(QMainWindow *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_menuBar(window->menuBar())
{
}

QMenu *MenuManagerImpl::menu(MenuLocation location)
{
    return ensureMenu(location);
}

QMenu *MenuManagerImpl::createMenu(const QString &id, const QString &title)
{
    auto it = m_customMenus.find(id);
    if (it != m_customMenus.end())
        return it.value();

    auto *m = m_menuBar->addMenu(title);
    m_customMenus.insert(id, m);
    return m;
}

bool MenuManagerImpl::removeMenu(const QString &id)
{
    auto it = m_customMenus.find(id);
    if (it == m_customMenus.end())
        return false;

    m_menuBar->removeAction(it.value()->menuAction());
    it.value()->deleteLater();
    m_customMenus.erase(it);
    return true;
}

void MenuManagerImpl::addAction(MenuLocation location, QAction *action,
                                 const QString & /*group*/)
{
    ensureMenu(location)->addAction(action);
}

void MenuManagerImpl::addAction(const QString &menuId, QAction *action,
                                 const QString & /*group*/)
{
    auto it = m_customMenus.find(menuId);
    if (it != m_customMenus.end())
        it.value()->addAction(action);
}

QMenu *MenuManagerImpl::addSubmenu(MenuLocation location, const QString &title,
                                    const QString & /*group*/)
{
    return ensureMenu(location)->addMenu(title);
}

void MenuManagerImpl::addSeparator(MenuLocation location)
{
    ensureMenu(location)->addSeparator();
}

bool MenuManagerImpl::removeAction(QAction *action)
{
    // Search in all menus
    for (auto *menu : m_standardMenus) {
        if (menu->actions().contains(action)) {
            menu->removeAction(action);
            return true;
        }
    }
    for (auto *menu : m_customMenus) {
        if (menu->actions().contains(action)) {
            menu->removeAction(action);
            return true;
        }
    }
    return false;
}

void MenuManagerImpl::addEditorContextAction(QAction *action,
                                              const QString &group,
                                              const QString &when)
{
    m_editorContextActions.append({action, group, when});
}

void MenuManagerImpl::addExplorerContextAction(QAction *action,
                                                const QString &group,
                                                const QString &when)
{
    m_explorerContextActions.append({action, group, when});
}

QStringList MenuManagerImpl::customMenuIds() const
{
    return m_customMenus.keys();
}

QMenu *MenuManagerImpl::customMenu(const QString &id) const
{
    return m_customMenus.value(id, nullptr);
}

QMenu *MenuManagerImpl::buildEditorContextMenu(QWidget *parent) const
{
    auto *menu = new QMenu(parent);
    for (const auto &ca : m_editorContextActions)
        menu->addAction(ca.action);
    return menu;
}

QMenu *MenuManagerImpl::buildExplorerContextMenu(QWidget *parent) const
{
    auto *menu = new QMenu(parent);
    for (const auto &ca : m_explorerContextActions)
        menu->addAction(ca.action);
    return menu;
}

QMenu *MenuManagerImpl::ensureMenu(MenuLocation location)
{
    auto it = m_standardMenus.find(location);
    if (it != m_standardMenus.end())
        return it.value();

    auto *m = m_menuBar->addMenu(menuTitle(location));
    m_standardMenus.insert(location, m);
    return m;
}

QString MenuManagerImpl::menuTitle(MenuLocation location) const
{
    switch (location) {
    case File:      return tr("&File");
    case Edit:      return tr("&Edit");
    case View:      return tr("&View");
    case Selection: return tr("&Selection");
    case Run:       return tr("&Run");
    case Terminal:  return tr("&Terminal");
    case Tools:     return tr("&Tools");
    case Help:      return tr("&Help");
    case Custom:    return tr("Custom");
    }
    return {};
}
