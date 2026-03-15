#include "toolbarmanageradapter.h"

#include "ui/dock/DockToolBar.h"
#include "ui/dock/DockToolBarManager.h"
#include "ui/dock/DockTypes.h"

#include <QAction>
#include <QJsonDocument>
#include <QJsonObject>

using namespace exdock;

namespace {
ToolBarEdge toEdge(IToolBarManager::Edge e)
{
    switch (e) {
    case IToolBarManager::Top:    return ToolBarEdge::Top;
    case IToolBarManager::Bottom: return ToolBarEdge::Bottom;
    case IToolBarManager::Left:   return ToolBarEdge::Left;
    case IToolBarManager::Right:  return ToolBarEdge::Right;
    }
    return ToolBarEdge::Top;
}
} // namespace

ToolBarManagerAdapter::ToolBarManagerAdapter(exdock::DockToolBarManager *impl,
                                             QObject *parent)
    : QObject(parent)
    , m_impl(impl)
{
}

bool ToolBarManagerAdapter::createToolBar(const QString &id, const QString &title,
                                           Edge edge)
{
    if (m_impl->toolBar(id))
        return false;
    return m_impl->createToolBar(id, title, toEdge(edge)) != nullptr;
}

bool ToolBarManagerAdapter::removeToolBar(const QString &id)
{
    return m_impl->removeToolBar(id);
}

void ToolBarManagerAdapter::addAction(const QString &toolBarId, QAction *action)
{
    auto *bar = m_impl->toolBar(toolBarId);
    if (bar)
        bar->addAction(action);
}

void ToolBarManagerAdapter::addWidget(const QString &toolBarId, QWidget *widget)
{
    auto *bar = m_impl->toolBar(toolBarId);
    if (bar)
        bar->addWidget(widget);
}

void ToolBarManagerAdapter::addSeparator(const QString &toolBarId)
{
    auto *bar = m_impl->toolBar(toolBarId);
    if (bar)
        bar->addSeparator();
}

void ToolBarManagerAdapter::setVisible(const QString &toolBarId, bool visible)
{
    m_impl->setToolBarVisible(toolBarId, visible);
}

bool ToolBarManagerAdapter::isVisible(const QString &toolBarId) const
{
    auto *bar = m_impl->toolBar(toolBarId);
    return bar && bar->isVisible();
}

void ToolBarManagerAdapter::setEdge(const QString &toolBarId, Edge edge)
{
    m_impl->moveToolBar(toolBarId, toEdge(edge));
}

void ToolBarManagerAdapter::setAllLocked(bool locked)
{
    m_impl->setAllLocked(locked);
}

bool ToolBarManagerAdapter::allLocked() const
{
    return m_impl->allLocked();
}

QStringList ToolBarManagerAdapter::toolBarIds() const
{
    return m_impl->toolBarIds();
}

QByteArray ToolBarManagerAdapter::saveLayout() const
{
    QJsonObject state = m_impl->saveState();
    return QJsonDocument(state).toJson(QJsonDocument::Compact);
}

bool ToolBarManagerAdapter::restoreLayout(const QByteArray &data)
{
    QJsonObject state = QJsonDocument::fromJson(data).object();
    if (state.isEmpty())
        return false;
    m_impl->restoreState(state);
    return true;
}
