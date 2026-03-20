#pragma once

#include <QObject>

class ProfileManager;
class WorkspaceSettings;

class WorkbenchStateBootstrap : public QObject
{
    Q_OBJECT

public:
    struct Callbacks
    {
        std::function<void(WorkspaceSettings *)> applyWorkspaceSettings;
        std::function<void(const QString &, bool)> setDockVisible;
    };

    explicit WorkbenchStateBootstrap(QObject *parent = nullptr);

    void initialize(WorkspaceSettings *workspaceSettings,
                    ProfileManager *profileManager,
                    const Callbacks &callbacks);
};
