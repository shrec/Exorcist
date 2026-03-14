#include "terminalsessionmanager.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

// ── Run foreground (non-blocking local event loop) ────────────────────────────
TerminalSession TerminalSessionManager::runForeground(const QString &command, int timeoutMs)
{
    TerminalSession s;
    s.id      = QUuid::createUuid().toString(QUuid::WithoutBraces);
    s.command = command;

    auto proc = std::make_unique<QProcess>();
    setupProcess(proc.get(), command);

    // Store raw pointer so cancel() can kill it while the loop spins
    m_foregroundProcess.storeRelaxed(proc.get());

    proc->start();
    if (!proc->waitForStarted(5000)) {
        m_foregroundProcess.storeRelaxed(nullptr);
        s.stdErr = tr("Failed to start process.");
        return s;
    }

    s.running = true;

    // Use a local event loop so the main thread keeps processing events
    // (timers, signals) while waiting for the process to finish.
    QEventLoop loop;
    bool timedOut = false;

    QObject::connect(proc.get(),
                     QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     &loop, &QEventLoop::quit);

    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        loop.quit();
    });
    timer.start(timeoutMs);

    loop.exec(QEventLoop::ExcludeSocketNotifiers);

    timer.stop();

    s.running  = false;
    s.timedOut = timedOut;

    if (timedOut) {
        proc->kill();
        proc->waitForFinished(2000);
    }

    m_foregroundProcess.storeRelaxed(nullptr);

    s.exitCode = proc->exitCode();
    s.stdOut   = QString::fromUtf8(proc->readAllStandardOutput());
    s.stdErr   = QString::fromUtf8(proc->readAllStandardError());
    return s;
}

// ── Run background (non-blocking) ────────────────────────────────────────────
QString TerminalSessionManager::runBackground(const QString &command, const QString &explanation)
{
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    auto session = std::make_shared<TerminalSessionInternal>();
    {
        QMutexLocker lock(&session->mutex);
        session->data.id          = id;
        session->data.command     = command;
        session->data.explanation = explanation;
        session->data.running     = true;
        session->data.startTimeMs = QDateTime::currentMSecsSinceEpoch();
    }

    // QProcess must be on the heap with signal connections
    auto *proc = new QProcess(this);      // parent-owned
    setupProcess(proc, command);

    connect(proc, &QProcess::readyReadStandardOutput, this,
            [session, proc]() {
                QMutexLocker lock(&session->mutex);
                session->data.stdOut += QString::fromUtf8(proc->readAllStandardOutput());
            });
    connect(proc, &QProcess::readyReadStandardError, this,
            [session, proc]() {
                QMutexLocker lock(&session->mutex);
                session->data.stdErr += QString::fromUtf8(proc->readAllStandardError());
            });
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, session, proc, id](int exitCode, QProcess::ExitStatus) {
                {
                    QMutexLocker lock(&session->mutex);
                    session->data.exitCode = exitCode;
                    session->data.running  = false;
                    session->data.stdOut += QString::fromUtf8(proc->readAllStandardOutput());
                    session->data.stdErr += QString::fromUtf8(proc->readAllStandardError());
                }
                proc->deleteLater();
                emit sessionFinished(id, exitCode);
            });

    proc->start();

    {
        QMutexLocker lock(&m_mutex);
        m_sessions[id]  = session;
        m_processes[id] = proc;
    }

    return id;
}

// ── Query session ─────────────────────────────────────────────────────────────
TerminalSession TerminalSessionManager::session(const QString &id) const
{
    QMutexLocker lock(&m_mutex);
    auto it = m_sessions.find(id);
    if (it == m_sessions.end())
        return {};
    return (*it)->snapshot();
}

bool TerminalSessionManager::hasSession(const QString &id) const
{
    QMutexLocker lock(&m_mutex);
    return m_sessions.contains(id);
}

// ── Kill session ──────────────────────────────────────────────────────────────
bool TerminalSessionManager::killSession(const QString &id)
{
    QMutexLocker lock(&m_mutex);
    auto pit = m_processes.find(id);
    if (pit == m_processes.end())
        return false;

    QProcess *proc = *pit;
    if (proc && proc->state() != QProcess::NotRunning) {
        proc->kill();
        return true;
    }
    return false;
}

// ── Await session completion ──────────────────────────────────────────────────
TerminalSession TerminalSessionManager::awaitSession(const QString &id, int timeoutMs)
{
    std::shared_ptr<TerminalSessionInternal> sess;
    QProcess *proc = nullptr;
    {
        QMutexLocker lock(&m_mutex);
        auto it = m_sessions.find(id);
        if (it == m_sessions.end())
            return {};
        sess = *it;
        auto pit = m_processes.find(id);
        if (pit != m_processes.end())
            proc = *pit;
    }

    if (proc && proc->state() != QProcess::NotRunning) {
        QEventLoop loop;

        QObject::connect(proc,
                         QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                         &loop, &QEventLoop::quit);

        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(timeoutMs);

        loop.exec(QEventLoop::ExcludeSocketNotifiers);
        timer.stop();
    }

    return sess->snapshot();
}

// ── Cancel foreground ─────────────────────────────────────────────────────────
void TerminalSessionManager::cancelForeground()
{
    QProcess *proc = m_foregroundProcess.loadRelaxed();
    if (proc)
        proc->kill();
}

// ── List sessions ─────────────────────────────────────────────────────────────
QStringList TerminalSessionManager::sessionIds() const
{
    QMutexLocker lock(&m_mutex);
    return m_sessions.keys();
}

// ── Setup process ─────────────────────────────────────────────────────────────
void TerminalSessionManager::setupProcess(QProcess *proc, const QString &command) const
{
    if (!m_workDir.isEmpty())
        proc->setWorkingDirectory(m_workDir);

#ifdef Q_OS_WIN
    proc->setProgram(QStringLiteral("cmd"));
    proc->setArguments({QStringLiteral("/c"), command});
#else
    proc->setProgram(QStringLiteral("sh"));
    proc->setArguments({QStringLiteral("-c"), command});
#endif
}
