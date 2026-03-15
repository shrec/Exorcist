#include "dockmanageradapter.h"

#include "ui/dock/DockManager.h"
#include "ui/dock/DockTypes.h"
#include "ui/dock/ExDockWidget.h"

#include <QAction>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>

using namespace exdock;

namespace {
SideBarArea toSideBar(IDockManager::Area area)
{
    switch (area) {
    case IDockManager::Left:    return SideBarArea::Left;
    case IDockManager::Right:   return SideBarArea::Right;
    case IDockManager::Bottom:  return SideBarArea::Bottom;
    case IDockManager::Center:  return SideBarArea::Left; // center has no sidebar
    }
    return SideBarArea::Left;
}
} // namespace

DockManagerAdapter::DockManagerAdapter(exdock::DockManager *impl, QObject *parent)
    : QObject(parent)
    , m_impl(impl)
{
}

bool DockManagerAdapter::addPanel(const QString &id, const QString &title,
                                   QWidget *widget, Area area, bool startVisible)
{
    if (m_impl->dockWidget(id))
        return false; // already exists

    auto *dock = new ExDockWidget(title, m_impl->mainWindow());
    dock->setDockId(id);
    dock->setContentWidget(widget);
    m_impl->addDockWidget(dock, toSideBar(area), startVisible);
    return true;
}

bool DockManagerAdapter::removePanel(const QString &id)
{
    auto *dock = m_impl->dockWidget(id);
    if (!dock)
        return false;
    m_impl->removeDockWidget(dock);
    return true;
}

void DockManagerAdapter::showPanel(const QString &id)
{
    auto *dock = m_impl->dockWidget(id);
    if (dock)
        m_impl->showDock(dock);
}

void DockManagerAdapter::hidePanel(const QString &id)
{
    auto *dock = m_impl->dockWidget(id);
    if (dock)
        m_impl->closeDock(dock);
}

void DockManagerAdapter::togglePanel(const QString &id)
{
    auto *dock = m_impl->dockWidget(id);
    if (dock)
        m_impl->toggleDock(dock);
}

bool DockManagerAdapter::isPanelVisible(const QString &id) const
{
    auto *dock = m_impl->dockWidget(id);
    return dock && m_impl->isPinned(dock);
}

void DockManagerAdapter::pinPanel(const QString &id)
{
    auto *dock = m_impl->dockWidget(id);
    if (dock)
        m_impl->pinDock(dock);
}

void DockManagerAdapter::unpinPanel(const QString &id)
{
    auto *dock = m_impl->dockWidget(id);
    if (dock)
        m_impl->unpinDock(dock);
}

bool DockManagerAdapter::isPanelPinned(const QString &id) const
{
    auto *dock = m_impl->dockWidget(id);
    return dock && m_impl->isPinned(dock);
}

QStringList DockManagerAdapter::panelIds() const
{
    return m_impl->dockWidgetIds();
}

QAction *DockManagerAdapter::panelToggleAction(const QString &id) const
{
    auto *dock = m_impl->dockWidget(id);
    return dock ? dock->toggleViewAction() : nullptr;
}

QWidget *DockManagerAdapter::panelWidget(const QString &id) const
{
    auto *dock = m_impl->dockWidget(id);
    return dock ? dock->contentWidget() : nullptr;
}

QByteArray DockManagerAdapter::saveLayout() const
{
    QJsonObject state = m_impl->saveState();
    return QJsonDocument(state).toJson(QJsonDocument::Compact);
}

bool DockManagerAdapter::restoreLayout(const QByteArray &data)
{
    QJsonObject state = QJsonDocument::fromJson(data).object();
    if (state.isEmpty())
        return false;
    m_impl->restoreState(state);
    return true;
}
