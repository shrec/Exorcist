#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QJsonObject>

class IDebugAdapter;
class CMakeIntegration;
class ToolchainManager;

/// Debug launch configuration for a single target.
struct DebugLaunchConfig
{
    QString name;           // display name
    QString executable;     // path to target binary
    QStringList args;       // command-line arguments
    QString workingDir;     // working directory
    QJsonObject env;        // environment variables
    QString debuggerPath;   // explicit debugger path (empty = auto)

    bool isValid() const { return !executable.isEmpty(); }
};

/// Coordinates build-before-debug and debug adapter launch.
/// F5 triggers: build (if needed) → launch debugger.
/// Ctrl+F5 triggers: build (if needed) → run without debugger.
class DebugLaunchController : public QObject
{
    Q_OBJECT

public:
    explicit DebugLaunchController(QObject *parent = nullptr);

    void setDebugAdapter(IDebugAdapter *adapter) { m_adapter = adapter; }
    void setCMakeIntegration(CMakeIntegration *cmake) { m_cmake = cmake; }
    void setToolchainManager(ToolchainManager *mgr) { m_toolchainMgr = mgr; }

    /// Start debugging: build first (if configured), then launch debugger.
    void startDebugging(const DebugLaunchConfig &config);

    /// Run without debugger: build first (if configured), then execute.
    void startWithoutDebugging(const DebugLaunchConfig &config);

    /// Stop the current debug session.
    void stopDebugging();

    /// Whether to build before debug/run.
    void setBuildBeforeRun(bool enabled) { m_buildBeforeRun = enabled; }
    bool buildBeforeRun() const { return m_buildBeforeRun; }

signals:
    /// Process started (without debugger).
    void processStarted(const QString &executable);

    /// Process finished (without debugger).
    void processFinished(int exitCode);

    /// Build started (pre-debug/run build).
    void preBuildStarted();

    /// Error occurred during launch.
    void launchError(const QString &message);

private slots:
    void onBuildFinished(bool success, int exitCode);

private:
    void doLaunchDebug(const DebugLaunchConfig &config);
    void doLaunchRun(const DebugLaunchConfig &config);
    QString resolveDebugger() const;

    IDebugAdapter    *m_adapter = nullptr;
    CMakeIntegration *m_cmake = nullptr;
    ToolchainManager *m_toolchainMgr = nullptr;

    bool m_buildBeforeRun = true;
    DebugLaunchConfig m_pendingConfig;
    enum class PendingAction { None, Debug, Run };
    PendingAction m_pendingAction = PendingAction::None;
    QProcess *m_runProcess = nullptr;
};
