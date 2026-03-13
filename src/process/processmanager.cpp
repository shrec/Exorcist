#include "processmanager.h"

ProcessManager::ProcessManager(QObject *parent)
    : QObject(parent)
{
}

QString ProcessManager::startLocal(const QString &program,
                                    const QStringList &args,
                                    const QString &workDir)
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

void ProcessManager::killLocal(const QString &id)
{
    auto it = m_localProcesses.find(id);
    if (it == m_localProcesses.end())
        return;
    it->second->kill();
}

void ProcessManager::terminateLocal(const QString &id)
{
    auto it = m_localProcesses.find(id);
    if (it == m_localProcesses.end())
        return;
    it->second->terminate();
}

ProcessManager::ProcessInfo ProcessManager::processInfo(const QString &id) const
{
    auto it = m_localInfos.find(id);
    if (it != m_localInfos.end())
        return it.value();
    return {};
}

QList<ProcessManager::ProcessInfo> ProcessManager::localProcesses() const
{
    return m_localInfos.values();
}

QProcess *ProcessManager::localProcess(const QString &id) const
{
    auto it = m_localProcesses.find(id);
    return (it != m_localProcesses.end()) ? it->second.get() : nullptr;
}

void ProcessManager::startShared(const QString &name,
                                  const QString &program,
                                  const QStringList &args,
                                  const QString &workDir)
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

void ProcessManager::killShared(const QString &name)
{
    if (!m_bridgeClient) return;

    QJsonObject params;
    params[QLatin1String("name")] = name;
    m_bridgeClient->callService(
        QStringLiteral("process"),
        QStringLiteral("kill"),
        params, {});
}

void ProcessManager::shutdownLocal()
{
    for (auto &[key, proc] : m_localProcesses) {
        if (proc && proc->state() != QProcess::NotRunning)
            proc->kill();
    }
    m_localProcesses.clear();
    m_localInfos.clear();
}
