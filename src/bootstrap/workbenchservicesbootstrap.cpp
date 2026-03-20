#include "workbenchservicesbootstrap.h"

#include "../thememanager.h"
#include "../ui/keymapmanager.h"
#include "../editor/filewatchservice.h"
#include "../serviceregistry.h"

WorkbenchServicesBootstrap::WorkbenchServicesBootstrap(QObject *parent)
    : QObject(parent)
{
}

void WorkbenchServicesBootstrap::initialize(ServiceRegistry *services)
{
    m_themeManager  = new ThemeManager(this);
    m_keymapManager = new KeymapManager(this);
    m_fileWatcher   = new FileWatchService(this);

    if (services) {
        services->registerService(QStringLiteral("themeManager"),  m_themeManager);
        services->registerService(QStringLiteral("keymapManager"), m_keymapManager);
        services->registerService(QStringLiteral("fileWatcher"),   m_fileWatcher);
    }
}
