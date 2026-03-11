#pragma once

#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QUuid>

#include <memory>

// ── TerminalSession ───────────────────────────────────────────────────────────
// Represents a single terminal command execution, either foreground or
// background.  Background sessions buffer output for later retrieval.

struct TerminalSession
{
    QString     id;
    QString     command;
    QString     explanation;
    int         exitCode   = -1;
    bool        running    = false;
    bool        timedOut   = false;
    QString     stdOut;
    QString     stdErr;
    qint64      startTimeMs = 0;
};

// Internal thread-safe wrapper around TerminalSession
struct TerminalSessionInternal
{
    mutable QMutex mutex;
    TerminalSession data;

    TerminalSession snapshot() const
    {
        QMutexLocker lock(&mutex);
        return data;
    }
};

// ── TerminalSessionManager ────────────────────────────────────────────────────
// Manages background and foreground terminal processes.
// Background sessions are tracked by UUID and can be queried, awaited, or
// killed after launch.
//
// Thread-safety: methods are guarded by a mutex so callers from the agent
// controller thread and QProcess signal threads are safe.

class TerminalSessionManager : public QObject
{
    Q_OBJECT

public:
    explicit TerminalSessionManager(const QString &workDir, QObject *parent = nullptr)
        : QObject(parent), m_workDir(workDir) {}

    void setWorkingDirectory(const QString &dir) { m_workDir = dir; }

    // ── Run foreground (blocking) ─────────────────────────────────────────
    TerminalSession runForeground(const QString &command, int timeoutMs)
    {
        TerminalSession s;
        s.id      = QUuid::createUuid().toString(QUuid::WithoutBraces);
        s.command = command;

        auto proc = std::make_unique<QProcess>();
        setupProcess(proc.get(), command);

        proc->start();
        if (!proc->waitForStarted(5000)) {
            s.stdErr = tr("Failed to start process.");
            return s;
        }

        s.running = true;
        const bool finished = proc->waitForFinished(timeoutMs);
        s.running  = false;
        s.timedOut = !finished;

        if (s.timedOut) {
            proc->kill();
            proc->waitForFinished(2000);
        }

        s.exitCode = proc->exitCode();
        s.stdOut   = QString::fromUtf8(proc->readAllStandardOutput());
        s.stdErr   = QString::fromUtf8(proc->readAllStandardError());
        return s;
    }

    // ── Run background (non-blocking) ─────────────────────────────────────
    QString runBackground(const QString &command, const QString &explanation = {})
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

    // ── Query session ─────────────────────────────────────────────────────
    TerminalSession session(const QString &id) const
    {
        QMutexLocker lock(&m_mutex);
        auto it = m_sessions.find(id);
        if (it == m_sessions.end())
            return {};
        return (*it)->snapshot();
    }

    bool hasSession(const QString &id) const
    {
        QMutexLocker lock(&m_mutex);
        return m_sessions.contains(id);
    }

    // ── Kill session ──────────────────────────────────────────────────────
    bool killSession(const QString &id)
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

    // ── Await session completion ──────────────────────────────────────────
    TerminalSession awaitSession(const QString &id, int timeoutMs)
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
            proc->waitForFinished(timeoutMs);
        }

        return sess->snapshot();
    }

    // ── List sessions ─────────────────────────────────────────────────────
    QStringList sessionIds() const
    {
        QMutexLocker lock(&m_mutex);
        return m_sessions.keys();
    }

signals:
    void sessionFinished(const QString &id, int exitCode);

private:
    void setupProcess(QProcess *proc, const QString &command) const
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

    QString m_workDir;
    mutable QMutex m_mutex;

    QHash<QString, std::shared_ptr<TerminalSessionInternal>> m_sessions;
    QHash<QString, QProcess *> m_processes;   // parent-owned, not unique_ptr
};
