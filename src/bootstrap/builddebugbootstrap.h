#pragma once

#include <QObject>
#include <QString>

class ToolchainManager;
class CMakeIntegration;
class DebugLaunchController;
class GdbMiAdapter;
class BuildToolbar;
class OutputPanel;
class DebugPanel;

struct DebugFrame;
enum class DebugStopReason;

class BuildDebugBootstrap : public QObject
{
    Q_OBJECT

public:
    explicit BuildDebugBootstrap(QObject *parent = nullptr);

    /// Create all build/debug subsystem objects.
    void initialize();

    /// Wire signal/slot connections between subsystems. Call after
    /// the dock widgets have been created.
    void wireConnections();

    /// Set the working directory used for Run/Debug launch configs.
    void setWorkingDir(const QString &dir) { m_workingDir = dir; }

    // Accessors
    ToolchainManager      *toolchainManager() const { return m_toolchainMgr; }
    CMakeIntegration      *cmakeIntegration() const { return m_cmakeIntegration; }
    DebugLaunchController *debugLauncher() const    { return m_debugLauncher; }
    GdbMiAdapter          *debugAdapter() const     { return m_debugAdapter; }
    BuildToolbar          *buildToolbar() const      { return m_buildToolbar; }
    OutputPanel           *outputPanel() const       { return m_outputPanel; }
    DebugPanel            *debugPanel() const        { return m_debugPanel; }

signals:
    /// Request to navigate editor to a source location.
    void navigateToSource(const QString &filePath, int line);

    /// Request to show a dock panel.
    void showOutputDock();
    void showRunDock();
    void showDebugDock();

    /// Status bar message.
    void statusMessage(const QString &msg, int timeout);

    /// Clangd should be restarted with new compile_commands.json.
    void restartClangd(const QString &compileCommandsDir);

    /// Build diagnostics need pushing to editors.
    void buildDiagnosticsReady();

    /// Debug stopped — need to highlight line in editor.
    void debugStopped(const QList<DebugFrame> &frames);

    /// Debug session ended — clear debug highlights.
    void debugTerminated();

private:
    ToolchainManager      *m_toolchainMgr    = nullptr;
    CMakeIntegration      *m_cmakeIntegration = nullptr;
    DebugLaunchController *m_debugLauncher    = nullptr;
    GdbMiAdapter          *m_debugAdapter     = nullptr;
    BuildToolbar          *m_buildToolbar     = nullptr;
    OutputPanel           *m_outputPanel      = nullptr;
    DebugPanel            *m_debugPanel       = nullptr;
    QString                m_workingDir;
};
