#include "clangdmanager.h"

#include <QProcess>

#include "processlsptransport.h"

ClangdManager::ClangdManager(QObject *parent)
    : QObject(parent),
      m_process(new QProcess(this)),
      m_transport(new ProcessLspTransport(m_process, this))
{
    connect(m_process, &QProcess::started, this, &ClangdManager::serverReady);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        emit serverCrashed();
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
                emit serverStopped();
            });
    connect(m_transport, &LspTransport::messageReceived, this, &ClangdManager::messageReceived);
}

ClangdManager::~ClangdManager()
{
    stop();
}

void ClangdManager::start(const QString &workspaceRoot, const QStringList &args)
{
    m_workspaceRoot = workspaceRoot;
    if (m_process->state() != QProcess::NotRunning) {
        return;
    }

    QStringList finalArgs = args;
    m_process->setProgram("clangd");
    m_process->setArguments(finalArgs);
    m_process->setWorkingDirectory(workspaceRoot);
    m_process->start();
    m_transport->start();
}

void ClangdManager::stop()
{
    if (m_process->state() == QProcess::NotRunning) {
        emit serverStopped();
        return;
    }

    m_process->terminate();
    if (!m_process->waitForFinished(2000)) {
        m_process->kill();
        m_process->waitForFinished();
    }
    emit serverStopped();
}
