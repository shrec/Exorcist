#include "terminalsessionmanager.h"

#include <QCoreApplication>

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
        s.stdErr = tr("Failed to start process: %1").arg(proc->errorString());
        s.exitCode = -1;
        return s;
    }

    s.running = true;

    // Tools run on worker threads (QtConcurrent). Use blocking
    // waitForFinished instead of QEventLoop — a fast command (e.g. dir)
    // can finish before a QEventLoop is created on the worker thread,
    // causing the finished() signal to be lost and the tool to hang
    // until the 30s timeout.
    const bool finished = proc->waitForFinished(timeoutMs);

    s.running  = false;
    s.timedOut = !finished;

    if (!finished) {
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
        // Use blocking wait — same race-condition reasoning as runForeground:
        // on a worker thread, QEventLoop may miss the finished() signal.
        proc->waitForFinished(timeoutMs);
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
