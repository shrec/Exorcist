#include "processlanguageserver.h"

#include "lspclient.h"
#include "processlsptransport.h"
#include "sdk/ilspservice.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QUrl>

class ProcessLanguageServerService : public ILspService
{
    Q_OBJECT

public:
    explicit ProcessLanguageServerService(ProcessLanguageServer *server,
                                          QObject *parent = nullptr)
        : ILspService(parent)
        , m_server(server)
    {
    }

    void startServer(const QString &workspaceRoot,
                     const QStringList &args = {}) override
    {
        if (m_server)
            m_server->start(workspaceRoot, args);
    }

    void stopServer() override
    {
        if (m_server)
            m_server->stop();
    }

private:
    ProcessLanguageServer *m_server = nullptr;
};

namespace {

void connectLspSignals(LspClient *client, ILspService *service)
{
    QObject::connect(client, &LspClient::definitionResult, service,
                     [service](const QString &, const QJsonArray &locations) {
        if (locations.isEmpty()) {
            emit service->statusMessage(QObject::tr("No definition found"), 3000);
            return;
        }

        const QJsonObject loc = locations.first().toObject();
        const QString targetUri = loc.contains(QStringLiteral("targetUri"))
            ? loc.value(QStringLiteral("targetUri")).toString()
            : loc.value(QStringLiteral("uri")).toString();
        const QJsonObject range = loc.contains(QStringLiteral("targetSelectionRange"))
            ? loc.value(QStringLiteral("targetSelectionRange")).toObject()
            : loc.value(QStringLiteral("range")).toObject();
        const QJsonObject start = range.value(QStringLiteral("start")).toObject();

        QString path = QUrl(targetUri).toLocalFile();
        if (path.isEmpty())
            path = targetUri;
        emit service->navigateToLocation(path,
                                         start.value(QStringLiteral("line")).toInt(),
                                         start.value(QStringLiteral("character")).toInt());
    });

    QObject::connect(client, &LspClient::declarationResult, service,
                     [service](const QString &, const QJsonArray &locations) {
        if (locations.isEmpty()) {
            emit service->statusMessage(QObject::tr("No declaration found"), 3000);
            return;
        }

        const QJsonObject loc = locations.first().toObject();
        const QString targetUri = loc.contains(QStringLiteral("targetUri"))
            ? loc.value(QStringLiteral("targetUri")).toString()
            : loc.value(QStringLiteral("uri")).toString();
        const QJsonObject range = loc.contains(QStringLiteral("targetSelectionRange"))
            ? loc.value(QStringLiteral("targetSelectionRange")).toObject()
            : loc.value(QStringLiteral("range")).toObject();
        const QJsonObject start = range.value(QStringLiteral("start")).toObject();

        QString path = QUrl(targetUri).toLocalFile();
        if (path.isEmpty())
            path = targetUri;
        emit service->navigateToLocation(path,
                                         start.value(QStringLiteral("line")).toInt(),
                                         start.value(QStringLiteral("character")).toInt());
    });

    QObject::connect(client, &LspClient::referencesResult, service,
                     [service](const QString &, const QJsonArray &locations) {
        if (locations.isEmpty()) {
            emit service->statusMessage(QObject::tr("No references found"), 3000);
            return;
        }
        emit service->referencesReady(locations);
    });

    QObject::connect(client, &LspClient::renameResult, service,
                     [service](const QString &, const QJsonObject &workspaceEdit) {
        emit service->workspaceEditRequested(workspaceEdit);
    });
}

}

ProcessLanguageServer::ProcessLanguageServer(const Config &config, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_process(std::make_unique<QProcess>())
    , m_transport(std::make_unique<ProcessLspTransport>(m_process.get()))
    , m_client(std::make_unique<LspClient>(m_transport.get()))
    , m_service(std::make_unique<ProcessLanguageServerService>(this))
{
    connectSignals();
    connectLspSignals(m_client.get(), m_service.get());
}

ProcessLanguageServer::~ProcessLanguageServer()
{
    stop();
}

void ProcessLanguageServer::start(const QString &workspaceRoot,
                                  const QStringList &extraArgs)
{
    m_workspaceRoot = workspaceRoot;
    if (m_process->state() != QProcess::NotRunning)
        return;

    QStringList args = m_config.baseArguments;
    args += extraArgs;

    m_process->setProgram(m_config.program);
    m_process->setArguments(args);
    if (!workspaceRoot.isEmpty())
        m_process->setWorkingDirectory(workspaceRoot);
    m_process->start();
    m_transport->start();
}

void ProcessLanguageServer::stop()
{
    if (m_process->state() == QProcess::NotRunning)
        return;

    m_client->shutdown();
    m_process->terminate();
    if (!m_process->waitForFinished(2000)) {
        m_process->kill();
        m_process->waitForFinished();
    }
}

LspClient *ProcessLanguageServer::client() const
{
    return m_client.get();
}

ILspService *ProcessLanguageServer::service() const
{
    return m_service.get();
}

void ProcessLanguageServer::connectSignals()
{
    connect(m_process.get(), &QProcess::started, this, [this]() {
        emit statusMessage(tr("%1 ready").arg(m_config.displayName), 3000);
        emit serverReady();
    });

    connect(m_process.get(), &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
                if (error == QProcess::FailedToStart) {
                    emit statusMessage(tr("%1 is not installed or not on PATH")
                                           .arg(m_config.displayName),
                                       5000);
                    emit serverStopped();
                    return;
                }

                emit statusMessage(tr("%1 crashed or disconnected")
                                       .arg(m_config.displayName),
                                   5000);
                emit serverCrashed();
            });

    connect(m_process.get(),
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            [this](int exitCode, QProcess::ExitStatus status) {
                if (status == QProcess::CrashExit || exitCode != 0) {
                    emit statusMessage(tr("%1 exited unexpectedly")
                                           .arg(m_config.displayName),
                                       5000);
                    emit serverCrashed();
                } else {
                    emit serverStopped();
                }
            });

    connect(m_client.get(), &LspClient::serverError, this,
            [this](const QString &message) {
                emit statusMessage(tr("%1: %2").arg(m_config.displayName, message),
                                   5000);
            });

    connect(this, &ProcessLanguageServer::serverReady,
            m_service.get(), &ILspService::serverReady);
    connect(this, &ProcessLanguageServer::statusMessage,
            m_service.get(), &ILspService::statusMessage);
}

#include "processlanguageserver.moc"