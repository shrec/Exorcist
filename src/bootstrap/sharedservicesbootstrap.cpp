#include "sharedservicesbootstrap.h"

#include <memory>

#include "pluginmanager.h"
#include "profile/profilemanager.h"
#include "sdk/hostservices.h"
#include "serviceregistry.h"
#include "settings/workspacesettings.h"

SharedServicesBootstrap::SharedServicesBootstrap(QObject *parent)
    : QObject(parent)
{
}

void SharedServicesBootstrap::initialize(ServiceRegistry *services,
                                         HostServices *hostServices,
                                         PluginManager *pluginManager)
{
    if (!services || !hostServices || !pluginManager)
        return;

    if (!m_workspaceSettings) {
        auto workspaceSettings = std::make_unique<WorkspaceSettings>(this);
        m_workspaceSettings = workspaceSettings.release();
        services->registerService(QStringLiteral("workspaceSettings"), m_workspaceSettings);
    }

    if (!m_profileManager) {
        auto profileManager = std::make_unique<ProfileManager>(pluginManager, this);
        m_profileManager = profileManager.release();
        m_profileManager->loadProfilesFromDirectory(QStringLiteral("profiles"));
        hostServices->setProfileManager(m_profileManager);
        services->registerService(QStringLiteral("profileManager"), m_profileManager);
    }
}
