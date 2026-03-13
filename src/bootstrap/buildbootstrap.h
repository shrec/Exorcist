#pragma once

#include <QObject>
#include <QString>

class ToolchainManager;
class CMakeIntegration;
class BuildToolbar;
class OutputPanel;

class BuildBootstrap : public QObject
{
    Q_OBJECT

public:
    explicit BuildBootstrap(QObject *parent = nullptr);

    void initialize();
    void wireConnections();

    void setWorkingDir(const QString &dir) { m_workingDir = dir; }

    ToolchainManager *toolchainManager() const { return m_toolchainMgr; }
    CMakeIntegration *cmakeIntegration() const { return m_cmakeIntegration; }
    BuildToolbar     *buildToolbar() const     { return m_buildToolbar; }
    OutputPanel      *outputPanel() const      { return m_outputPanel; }

signals:
    void showOutputDock();
    void statusMessage(const QString &msg, int timeout);
    void restartClangd(const QString &compileCommandsDir);
    void buildDiagnosticsReady();

private:
    ToolchainManager *m_toolchainMgr    = nullptr;
    CMakeIntegration *m_cmakeIntegration = nullptr;
    BuildToolbar     *m_buildToolbar     = nullptr;
    OutputPanel      *m_outputPanel      = nullptr;
    QString           m_workingDir;
};
