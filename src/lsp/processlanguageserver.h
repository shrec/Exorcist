#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

class ILspService;
class LspClient;
class ProcessLspTransport;
class QProcess;

class ProcessLanguageServerService;

class ProcessLanguageServer : public QObject
{
    Q_OBJECT

public:
    struct Config {
        QString program;
        QString displayName;
        QStringList baseArguments;
    };

    explicit ProcessLanguageServer(const Config &config, QObject *parent = nullptr);
    ~ProcessLanguageServer() override;

    void start(const QString &workspaceRoot, const QStringList &extraArgs = {});
    void stop();

    QString workspaceRoot() const { return m_workspaceRoot; }
    LspClient *client() const;
    ILspService *service() const;

signals:
    void serverReady();
    void serverStopped();
    void serverCrashed();
    void statusMessage(const QString &message, int timeout);

private:
    void connectSignals();

    Config m_config;
    QString m_workspaceRoot;
    std::unique_ptr<QProcess> m_process;
    std::unique_ptr<ProcessLspTransport> m_transport;
    std::unique_ptr<LspClient> m_client;
    std::unique_ptr<ProcessLanguageServerService> m_service;
};
