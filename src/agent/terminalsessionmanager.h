#pragma once

#include <QAtomicPointer>
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
// Foreground execution uses a local QEventLoop so the main thread remains
// responsive — this allows watchdog timers and cancel() to work properly.
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

    // ── Run foreground (event-loop, non-blocking main thread) ─────────────
    TerminalSession runForeground(const QString &command, int timeoutMs);

    // ── Run background (non-blocking) ─────────────────────────────────────
    QString runBackground(const QString &command, const QString &explanation = {});

    // ── Query session ─────────────────────────────────────────────────────
    TerminalSession session(const QString &id) const;
    bool hasSession(const QString &id) const;

    // ── Kill session ──────────────────────────────────────────────────────
    bool killSession(const QString &id);

    // ── Cancel the currently running foreground process ────────────────────
    void cancelForeground();

    // ── Await session completion ──────────────────────────────────────────
    TerminalSession awaitSession(const QString &id, int timeoutMs);

    // ── List sessions ─────────────────────────────────────────────────────
    QStringList sessionIds() const;

signals:
    void sessionFinished(const QString &id, int exitCode);

private:
    void setupProcess(QProcess *proc, const QString &command) const;

    QString m_workDir;
    mutable QMutex m_mutex;

    QHash<QString, std::shared_ptr<TerminalSessionInternal>> m_sessions;
    QHash<QString, QProcess *> m_processes;   // parent-owned, not unique_ptr

    QAtomicPointer<QProcess> m_foregroundProcess{nullptr};
};
