#include "debuglaunchcontroller.h"
#include "cmakeintegration.h"
#include "toolchainmanager.h"
#include "debug/gdbmiadapter.h"
#include "debug/idebugadapter.h"

#include <QFileInfo>
#include <QProcess>
#include <memory>

DebugLaunchController::DebugLaunchController(QObject *parent)
    : QObject(parent)
{
}

// ── Start debugging (F5) ─────────────────────────────────────────────────────

void DebugLaunchController::startDebugging(const DebugLaunchConfig &config)
{
    if (!config.isValid()) {
        emit launchError(tr("No executable specified"));
        return;
    }

    if (!m_adapter) {
        emit launchError(tr("No debug adapter configured"));
        return;
    }

    if (!QFileInfo::exists(config.executable) && m_buildBeforeRun && m_cmake) {
        // Need to build first
        m_pendingConfig = config;
        m_pendingAction = PendingAction::Debug;
        emit preBuildStarted();

        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = connect(m_cmake, &CMakeIntegration::buildFinished,
                this, [this, conn](bool s, int c) {
            disconnect(*conn);
            onBuildFinished(s, c);
        });
        m_cmake->build();
        return;
    }

    if (m_buildBeforeRun && m_cmake && m_cmake->hasCMakeProject()) {
        // Build before debug even if executable exists (ensure up-to-date)
        m_pendingConfig = config;
        m_pendingAction = PendingAction::Debug;
        emit preBuildStarted();

        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = connect(m_cmake, &CMakeIntegration::buildFinished,
                this, [this, conn](bool s, int c) {
            disconnect(*conn);
            onBuildFinished(s, c);
        });
        m_cmake->build();
        return;
    }

    doLaunchDebug(config);
}

// ── Start without debugging (Ctrl+F5) ─────────────────────────────────────────

void DebugLaunchController::startWithoutDebugging(const DebugLaunchConfig &config)
{
    if (!config.isValid()) {
        emit launchError(tr("No executable specified"));
        return;
    }

    if (m_buildBeforeRun && m_cmake && m_cmake->hasCMakeProject()) {
        m_pendingConfig = config;
        m_pendingAction = PendingAction::Run;
        emit preBuildStarted();

        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = connect(m_cmake, &CMakeIntegration::buildFinished,
                this, [this, conn](bool s, int c) {
            disconnect(*conn);
            onBuildFinished(s, c);
        });
        m_cmake->build();
        return;
    }

    doLaunchRun(config);
}

// ── Stop ──────────────────────────────────────────────────────────────────────

void DebugLaunchController::stopDebugging()
{
    if (m_adapter && m_adapter->isRunning())
        m_adapter->terminate();

    if (m_runProcess && m_runProcess->state() != QProcess::NotRunning) {
        m_runProcess->terminate();
        if (!m_runProcess->waitForFinished(2000))
            m_runProcess->kill();
    }

    if (m_cmake && m_cmake->isBuilding())
        m_cmake->cancelBuild();
}

// ── Build callback ────────────────────────────────────────────────────────────

void DebugLaunchController::onBuildFinished(bool success, int /*exitCode*/)
{
    if (!success) {
        emit launchError(tr("Build failed — cannot launch"));
        m_pendingAction = PendingAction::None;
        return;
    }

    switch (m_pendingAction) {
    case PendingAction::Debug:
        doLaunchDebug(m_pendingConfig);
        break;
    case PendingAction::Run:
        doLaunchRun(m_pendingConfig);
        break;
    case PendingAction::None:
        break;
    }
    m_pendingAction = PendingAction::None;
}

// ── Internal launch ───────────────────────────────────────────────────────────

void DebugLaunchController::doLaunchDebug(const DebugLaunchConfig &config)
{
    if (!m_adapter) {
        emit launchError(tr("No debug adapter available"));
        return;
    }

    if (!QFileInfo::exists(config.executable)) {
        emit launchError(tr("Executable not found: %1").arg(config.executable));
        return;
    }

    // Set the debugger path on GdbMiAdapter if available
    const QString debuggerPath = config.debuggerPath.isEmpty()
        ? resolveDebugger()
        : config.debuggerPath;

    // Apply debugger path to GdbMiAdapter
    if (auto *gdb = qobject_cast<GdbMiAdapter *>(m_adapter))
        gdb->setDebuggerPath(debuggerPath);

    m_adapter->launch(config.executable, config.args,
                      config.workingDir, config.env);
}

void DebugLaunchController::doLaunchRun(const DebugLaunchConfig &config)
{
    if (!QFileInfo::exists(config.executable)) {
        emit launchError(tr("Executable not found: %1").arg(config.executable));
        return;
    }

    if (!m_runProcess) {
        m_runProcess = new QProcess(this);
        connect(m_runProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int exitCode, QProcess::ExitStatus) {
            emit processFinished(exitCode);
        });
    }

    if (!config.workingDir.isEmpty())
        m_runProcess->setWorkingDirectory(config.workingDir);

    // Set environment
    if (!config.env.isEmpty()) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        for (auto it = config.env.begin(); it != config.env.end(); ++it)
            env.insert(it.key(), it.value().toString());
        m_runProcess->setProcessEnvironment(env);
    }

    emit processStarted(config.executable);
    m_runProcess->start(config.executable, config.args);
}

QString DebugLaunchController::resolveDebugger() const
{
    if (!m_toolchainMgr) return QStringLiteral("gdb");

    const Toolchain tc = m_toolchainMgr->defaultToolchain();
    if (tc.debugger.isValid())
        return tc.debugger.path;

    return QStringLiteral("gdb");
}
