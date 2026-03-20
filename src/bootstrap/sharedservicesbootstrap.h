#pragma once

#include <QObject>

class ServiceRegistry;
class HostServices;
class PluginManager;
class WorkspaceSettings;
class ProfileManager;

/// Registers shared non-visual services used across multiple plugins
/// and workbench workflows.
class SharedServicesBootstrap : public QObject
{
    Q_OBJECT

public:
    explicit SharedServicesBootstrap(QObject *parent = nullptr);

    void initialize(ServiceRegistry *services,
                    HostServices *hostServices,
                    PluginManager *pluginManager);

    WorkspaceSettings *workspaceSettings() const { return m_workspaceSettings; }
    ProfileManager *profileManager() const { return m_profileManager; }

private:
    WorkspaceSettings *m_workspaceSettings = nullptr;
    ProfileManager *m_profileManager = nullptr;
};
