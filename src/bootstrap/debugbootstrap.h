#pragma once

#include <QObject>
#include <QString>
#include <QList>

class DebugLaunchController;
class GdbMiAdapter;
class DebugPanel;
class CMakeIntegration;
class ToolchainManager;
class BuildToolbar;

struct DebugFrame;
enum class DebugStopReason;

class DebugBootstrap : public QObject
{
    Q_OBJECT

public:
    explicit DebugBootstrap(QObject *parent = nullptr);

    void initialize();

    /// Wire connections. Must be called after build bootstrap is wired so
    /// that debug can connect to the build toolbar for run/debug actions.
    void wireConnections(CMakeIntegration *cmake, ToolchainManager *toolchain,
                         BuildToolbar *toolbar);

    void setWorkingDir(const QString &dir) { m_workingDir = dir; }

    DebugLaunchController *debugLauncher() const { return m_debugLauncher; }
    GdbMiAdapter          *debugAdapter() const  { return m_debugAdapter; }
    DebugPanel            *debugPanel() const     { return m_debugPanel; }

signals:
    void navigateToSource(const QString &filePath, int line);
    void showRunDock();
    void showDebugDock();
    void statusMessage(const QString &msg, int timeout);
    void debugStopped(const QList<DebugFrame> &frames);
    void debugTerminated();

private:
    DebugLaunchController *m_debugLauncher = nullptr;
    GdbMiAdapter          *m_debugAdapter  = nullptr;
    DebugPanel            *m_debugPanel    = nullptr;
    QString                m_workingDir;
};
