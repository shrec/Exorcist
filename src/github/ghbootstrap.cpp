#include "ghbootstrap.h"
#include "ghservice.h"
#include "ghpanel.h"

#include "ui/dock/DockManager.h"
#include "ui/dock/ExDockWidget.h"

GhBootstrap::GhBootstrap(QObject *parent)
    : QObject(parent)
{
}

void GhBootstrap::initialize(exdock::DockManager *dockManager, QWidget *dockParent)
{
    using namespace exdock;

    m_service = new GhService(this);
    m_panel   = new GhPanel(m_service, dockParent);

    m_dock = new ExDockWidget(tr("GitHub"), dockParent);
    m_dock->setDockId(QStringLiteral("GitHubDock"));
    m_dock->setContentWidget(m_panel);
    dockManager->addDockWidget(m_dock, SideBarArea::Left, /*startPinned=*/false);
}

void GhBootstrap::setWorkingDirectory(const QString &path)
{
    if (m_service)
        m_service->setWorkingDirectory(path);
}

void GhBootstrap::refresh()
{
    if (m_panel)
        m_panel->refresh();
}

bool GhBootstrap::isAvailable() const
{
    return m_service && m_service->isAvailable();
}
