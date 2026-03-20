#include "workbenchstatebootstrap.h"

#include "profile/profilemanager.h"
#include "settings/workspacesettings.h"

#include <QTimer>

WorkbenchStateBootstrap::WorkbenchStateBootstrap(QObject *parent)
    : QObject(parent)
{
}

void WorkbenchStateBootstrap::initialize(WorkspaceSettings *workspaceSettings,
                                         ProfileManager *profileManager,
                                         const Callbacks &callbacks)
{
    if (workspaceSettings && callbacks.applyWorkspaceSettings) {
        connect(workspaceSettings, &WorkspaceSettings::settingsChanged, this,
                [workspaceSettings, callbacks]() {
            callbacks.applyWorkspaceSettings(workspaceSettings);
        });
    }

    if (workspaceSettings && profileManager) {
        connect(profileManager, &ProfileManager::profileChanged, this,
                [this, workspaceSettings, profileManager, callbacks](const QString &newProfileId, const QString &) {
            const ProfileManifest *manifest = profileManager->manifest(newProfileId);
            if (!manifest)
                return;

            for (const auto &setting : manifest->settings) {
                const int dot = setting.key.indexOf(QLatin1Char('.'));
                if (dot <= 0 || dot >= setting.key.size() - 1)
                    continue;

                const QString group = setting.key.left(dot);
                const QString key = setting.key.mid(dot + 1);
                if (!workspaceSettings->hasWorkspaceOverride(group, key))
                    workspaceSettings->setWorkspaceValue(group, key, setting.value);
            }

            if (callbacks.setDockVisible) {
                QTimer::singleShot(0, this, [manifest, callbacks]() {
                    for (const auto &dockDefault : manifest->dockDefaults)
                        callbacks.setDockVisible(dockDefault.dockId, dockDefault.visible);
                });
            }
        });
    }
}
