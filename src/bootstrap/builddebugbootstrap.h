#pragma once

#include "buildbootstrap.h"
#include "debugbootstrap.h"

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

// ── BuildDebugBootstrap ──────────────────────────────────────────────────────
//
// Compatibility wrapper that delegates to BuildBootstrap + DebugBootstrap.
// Will be removed once MainWindow references the split bootstraps directly.

class BuildDebugBootstrap : public QObject
{
    Q_OBJECT

public:
    explicit BuildDebugBootstrap(QObject *parent = nullptr);

    void initialize();
    void wireConnections();

    void setWorkingDir(const QString &dir);

    ToolchainManager      *toolchainManager() const { return m_build.toolchainManager(); }
    CMakeIntegration      *cmakeIntegration() const { return m_build.cmakeIntegration(); }
    DebugLaunchController *debugLauncher() const    { return m_debug.debugLauncher(); }
    GdbMiAdapter          *debugAdapter() const     { return m_debug.debugAdapter(); }
    BuildToolbar          *buildToolbar() const      { return m_build.buildToolbar(); }
    OutputPanel           *outputPanel() const       { return m_build.outputPanel(); }
    DebugPanel            *debugPanel() const        { return m_debug.debugPanel(); }

signals:
    void navigateToSource(const QString &filePath, int line);
    void showOutputDock();
    void showRunDock();
    void showDebugDock();
    void statusMessage(const QString &msg, int timeout);
    void restartClangd(const QString &compileCommandsDir);
    void buildDiagnosticsReady();
    void debugStopped(const QList<DebugFrame> &frames);
    void debugTerminated();

private:
    BuildBootstrap m_build;
    DebugBootstrap m_debug;
};
