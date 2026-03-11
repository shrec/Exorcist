#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <memory>
#include <unordered_map>

// ── ProcessManager ────────────────────────────────────────────────────────────
//
// Centralized process lifecycle management for an Exorcist IDE instance.
//
// Two categories of processes:
//
// • **Local** — processes owned by *this* IDE instance (terminal shells,
//   per-window builds, debug adapters).  Lifecycle is tied to the window.
//
// • **Shared** — processes owned by ExoBridge daemon
//   (MCP tool servers, auth token refreshers).
//   Multiple IDE windows pointing at the same workspace share these.
//
// ProcessManager provides a uniform API to start, stop, and query both
// kinds.  Shared processes are delegated via BridgeClient IPC; local
// processes run as direct QProcess children.

#include "bridgeclient.h"

class ProcessManager : public QObject
{
    Q_OBJECT

public:
    explicit ProcessManager(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    /// Set the BridgeClient for shared-process delegation.
    /// Must be called before using shared process features.
    void setBridgeClient(BridgeClient *client) { m_bridgeClient = client; }

    // ── Local processes ───────────────────────────────────────────────

    struct ProcessInfo {
        QString   id;
        QString   program;
        QStringList args;
        QString   workDir;
        qint64    pid       = -1;
        bool      running   = false;
        bool      shared    = false;
    };

    /// Start a local process.  Returns an ID used to track it.
    QString startLocal(const QString &program,
                       const QStringList &args = {},
                       const QString &workDir = {})
    {
        const QString id = nextId();
        auto proc = std::make_unique<QProcess>();
        proc->setProgram(program);
        proc->setArguments(args);
        if (!workDir.isEmpty())
            proc->setWorkingDirectory(workDir);

        auto *raw = proc.get();
        connect(raw, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, id](int exitCode, QProcess::ExitStatus status) {
            emit processFinished(id, exitCode, status == QProcess::CrashExit);
            m_localProcesses.erase(id);
            m_localInfos.remove(id);
        });
        connect(raw, &QProcess::errorOccurred,
                this, [this, id](QProcess::ProcessError err) {
            Q_UNUSED(err)
            auto pit = m_localProcesses.find(id);
            emit processError(id, (pit != m_localProcesses.end())
                                      ? pit->second->errorString()
                                      : QString());
        });

        proc->start();

        ProcessInfo info;
        info.id      = id;
        info.program = program;
        info.args    = args;
        info.workDir = workDir;
        info.pid     = raw->processId();
        info.running = true;
        info.shared  = false;

        m_localProcesses[id] = std::move(proc);
        m_localInfos.insert(id, info);

        emit processStarted(id);
        return id;
    }

    /// Kill a local process by ID.
    void killLocal(const QString &id)
    {
        auto it = m_localProcesses.find(id);
        if (it == m_localProcesses.end())
            return;
        it->second->kill();
    }

    /// Gracefully terminate a local process.
    void terminateLocal(const QString &id)
    {
        auto it = m_localProcesses.find(id);
        if (it == m_localProcesses.end())
            return;
        it->second->terminate();
    }

    /// Get info about a tracked process (local or shared).
    ProcessInfo processInfo(const QString &id) const
    {
        auto it = m_localInfos.find(id);
        if (it != m_localInfos.end())
            return it.value();
        return {};
    }

    /// List all running local processes.
    QList<ProcessInfo> localProcesses() const
    {
        return m_localInfos.values();
    }

    /// Get raw QProcess pointer (local only — use for stdout/stderr hooking).
    QProcess *localProcess(const QString &id) const
    {
        auto it = m_localProcesses.find(id);
        return (it != m_localProcesses.end()) ? it->second.get() : nullptr;
    }

    // ── Shared processes (via ServerClient) ───────────────────────────

    /// Request the shared server to start a process.
    void startShared(const QString &name,
                     const QString &program,
                     const QStringList &args = {},
                     const QString &workDir = {})
    {
        if (!m_bridgeClient) {
            qWarning("[ProcessManager] No BridgeClient — cannot start shared"
                     " process '%s'", qPrintable(name));
            return;
        }

        QJsonObject params;
        params[QLatin1String("name")]    = name;
        params[QLatin1String("program")] = program;
        QJsonArray argsArr;
        for (const auto &a : args)
            argsArr.append(a);
        params[QLatin1String("args")]    = argsArr;
        if (!workDir.isEmpty())
            params[QLatin1String("workDir")] = workDir;

        m_bridgeClient->callService(
            QStringLiteral("process"),
            QStringLiteral("start"),
            params,
            [this, name](bool ok, const QJsonObject &result) {
                if (ok)
                    emit sharedProcessStarted(name);
                else
                    qWarning("[ProcessManager] Failed to start shared process"
                             " '%s': %s",
                             qPrintable(name),
                             qPrintable(result.value(
                                 QLatin1String("message")).toString()));
            });
    }

    /// Request the shared server to kill a process.
    void killShared(const QString &name)
    {
        if (!m_bridgeClient) return;

        QJsonObject params;
        params[QLatin1String("name")] = name;
        m_bridgeClient->callService(
            QStringLiteral("process"),
            QStringLiteral("kill"),
            params, {});
    }

    // ── Shutdown ──────────────────────────────────────────────────────

    /// Kill all local processes.
    void shutdownLocal()
    {
        for (auto &[key, proc] : m_localProcesses) {
            if (proc && proc->state() != QProcess::NotRunning)
                proc->kill();
        }
        m_localProcesses.clear();
        m_localInfos.clear();
    }

signals:
    void processStarted(const QString &id);
    void processFinished(const QString &id, int exitCode, bool crashed);
    void processError(const QString &id, const QString &error);
    void sharedProcessStarted(const QString &name);

private:
    QString nextId()
    {
        return QStringLiteral("proc-%1").arg(m_nextLocalId++);
    }

    BridgeClient *m_bridgeClient = nullptr;
    int           m_nextLocalId  = 1;

    struct StdStringHash {
        std::size_t operator()(const QString &s) const { return qHash(s); }
    };
    std::unordered_map<QString, std::unique_ptr<QProcess>, StdStringHash> m_localProcesses;
    QHash<QString, ProcessInfo>               m_localInfos;
};
