#pragma once

#include <QObject>

#include "plugininterface.h"
#include "plugin/iviewcontributor.h"

class ToolchainManager;
class CMakeIntegration;
class BuildToolbar;
class OutputPanel;
class RunLaunchPanel;
class DebugLaunchController;
class BuildSystemService;
class KitManager;
class IDebugAdapter;

class BuildPlugin : public QObject, public IPlugin, public IViewContributor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID)
    Q_INTERFACES(IPlugin IViewContributor)

public:
    PluginInfo info() const override;
    bool initialize(IHostServices *host) override;
    void shutdown() override;

    // IViewContributor
    QWidget *createView(const QString &viewId, QWidget *parent) override;

    // Accessors for service wiring
    ToolchainManager      *toolchainManager() const { return m_toolchainMgr; }
    CMakeIntegration      *cmakeIntegration() const { return m_cmake; }
    BuildToolbar          *buildToolbar() const     { return m_toolbar; }
    OutputPanel           *outputPanel() const      { return m_output; }
    RunLaunchPanel        *runPanel() const         { return m_runPanel; }
    DebugLaunchController *debugLauncher() const    { return m_launcher; }

    void setWorkingDir(const QString &dir);

private:
    void wireConnections();
    void wireDebugAdapter();

    IHostServices *m_host = nullptr;

    ToolchainManager      *m_toolchainMgr = nullptr;
    KitManager            *m_kitMgr       = nullptr;
    CMakeIntegration      *m_cmake        = nullptr;
    BuildToolbar          *m_toolbar      = nullptr;
    OutputPanel           *m_output       = nullptr;
    RunLaunchPanel        *m_runPanel     = nullptr;
    DebugLaunchController *m_launcher     = nullptr;
    BuildSystemService    *m_buildSvc     = nullptr;

    IDebugAdapter         *m_debugAdapter = nullptr;
    QString                m_workingDir;
};
