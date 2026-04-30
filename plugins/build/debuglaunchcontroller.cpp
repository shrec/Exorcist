#include "debuglaunchcontroller.h"
#include "cmakeintegration.h"
#include "toolchainmanager.h"
#include "sdk/idebugadapter.h"

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
    emit debugLog(QStringLiteral("[DEBUG] startDebugging: exe=%1 adapter=%2 cmake=%3")
                  .arg(config.executable.isEmpty() ? QStringLiteral("(empty)") : config.executable,
                       m_adapter ? QStringLiteral("yes") : QStringLiteral("NULL"),
                       (m_cmake && m_cmake->hasCMakeProject()) ? QStringLiteral("yes") : QStringLiteral("no")));

    // Allow empty exe when cmake project exists — we'll build first and auto-discover.
    if (!config.isValid() && (!m_cmake || !m_cmake->hasCMakeProject())) {
        emit launchError(tr("No executable specified"));
        return;
    }

    if (!m_adapter) {
        emit launchError(tr("No debug adapter configured"));
        return;
    }

    // Build before debug whenever we have a cmake project (ensures exe is up-to-date).
    if (m_cmake && m_cmake->hasCMakeProject()) {
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
    // Allow empty exe when cmake project exists — we'll build first and auto-discover.
    if (!config.isValid() && (!m_cmake || !m_cmake->hasCMakeProject())) {
        emit launchError(tr("No executable specified"));
        return;
    }

    // Build before run whenever we have a cmake project (ensures exe is up-to-date).
    if (m_cmake && m_cmake->hasCMakeProject()) {
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
    emit debugLog(QStringLiteral("[DEBUG] onBuildFinished: success=%1 pendingAction=%2")
                  .arg(success).arg(int(m_pendingAction)));

    if (!success) {
        emit launchError(tr("Build failed — cannot launch"));
        m_pendingAction = PendingAction::None;
        return;
    }

    switch (m_pendingAction) {
    case PendingAction::Debug:
        emit debugLog(QStringLiteral("[DEBUG] → doLaunchDebug"));
        doLaunchDebug(m_pendingConfig);
        break;
    case PendingAction::Run:
        doLaunchRun(m_pendingConfig);
        break;
    case PendingAction::None:
        emit debugLog(QStringLiteral("[DEBUG] pendingAction=None — skipped"));
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

    QString exePath = config.executable;

    // Auto-discover first executable when none was specified (build-before-debug flow).
    if (exePath.isEmpty() && m_cmake) {
        const QStringList targets = m_cmake->discoverTargets();
        if (!targets.isEmpty())
            exePath = targets.first();
        emit debugLog(QStringLiteral("[DEBUG] auto-discovered exe=%1").arg(exePath));
    }

    emit debugLog(QStringLiteral("[DEBUG] doLaunchDebug: exe=%1").arg(exePath));

    if (!QFileInfo::exists(exePath)) {
        emit launchError(exePath.isEmpty()
            ? tr("No executable found in build directory")
            : tr("Executable not found: %1").arg(exePath));
        return;
    }

    // Set the debugger path on GdbMiAdapter if available
    const QString debuggerPath = config.debuggerPath.isEmpty()
        ? resolveDebugger()
        : config.debuggerPath;

    emit debugLog(QStringLiteral("[DEBUG] LAUNCHING: debugger=%1 exe=%2").arg(debuggerPath, exePath));

    // Apply debugger path to adapter
    if (m_adapter)
        m_adapter->setDebuggerPath(debuggerPath);

    m_adapter->launch(exePath, config.args,
                      config.workingDir, config.env);

    emit debugLog(QStringLiteral("[DEBUG] launch() returned — adapter.isRunning=%1")
                  .arg(m_adapter->isRunning() ? QStringLiteral("YES") : QStringLiteral("NO")));
}

void DebugLaunchController::doLaunchRun(const DebugLaunchConfig &config)
{
    QString exePath = config.executable;

    // Auto-discover first executable when none was specified (build-before-run flow).
    if (exePath.isEmpty() && m_cmake) {
        const QStringList targets = m_cmake->discoverTargets();
        if (!targets.isEmpty())
            exePath = targets.first();
    }

    if (!QFileInfo::exists(exePath)) {
        emit launchError(exePath.isEmpty()
            ? tr("No executable found in build directory")
            : tr("Executable not found: %1").arg(exePath));
        return;
    }

    if (!m_runProcess) {
        m_runProcess = new QProcess(this);
        connect(m_runProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int exitCode, QProcess::ExitStatus) {
            emit processFinished(exitCode);
        });
        connect(m_runProcess, &QProcess::readyReadStandardOutput, this, [this]() {
            const QString text = QString::fromUtf8(m_runProcess->readAllStandardOutput());
            emit processOutput(text, false);
        });
        connect(m_runProcess, &QProcess::readyReadStandardError, this, [this]() {
            const QString text = QString::fromUtf8(m_runProcess->readAllStandardError());
            emit processOutput(text, true);
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

    emit processStarted(exePath);
    m_runProcess->start(exePath, config.args);
}

QString DebugLaunchController::resolveDebugger() const
{
    // 1. Toolchain-detected debugger (best — respects user's kit selection)
    if (m_toolchainMgr) {
        const Toolchain tc = m_toolchainMgr->defaultToolchain();
        if (tc.debugger.isValid())
            return tc.debugger.path;
    }

    // 2. Probe well-known Windows GDB locations (detection may still be running)
    static const char *const kProbe[] = {
        "C:/Qt/Tools/mingw1310_64/bin/gdb.exe",
        "C:/Qt/Tools/mingw1120_64/bin/gdb.exe",
        "C:/msys64/ucrt64/bin/gdb.exe",
        "C:/msys64/mingw64/bin/gdb.exe",
        "C:/mingw64/bin/gdb.exe",
    };
    for (const char *p : kProbe) {
        if (QFile::exists(QString::fromLatin1(p)))
            return QString::fromLatin1(p);
    }

    // 3. Bare name — relies on PATH
    return QStringLiteral("gdb");
}
