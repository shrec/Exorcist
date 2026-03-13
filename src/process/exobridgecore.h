#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QProcess>
#include <QTimer>

#include <functional>
#include <memory>
#include <unordered_map>

#include "ipcprotocol.h"

// ── ExoBridgeCore ─────────────────────────────────────────────────────────────
//
// The heart of ExoBridge — the shared daemon process.  Listens on a
// QLocalServer (named pipe / Unix domain socket) and services requests
// from any number of connected Exorcist IDE instances.
//
// Shared resources managed here (not per-IDE-window):
//   • Workspace file index (WorkspaceIndexer)
//   • MCP tool server processes
//   • Git file watching & status cache
//   • Auth token cache & refresh cycles
//
// Per-instance resources (LSP, terminal, editor) stay in the IDE process.

class ExoBridgeCore : public QObject
{
    Q_OBJECT

public:
    explicit ExoBridgeCore(QObject *parent = nullptr);

    void setPersistent(bool on) { m_persistent = on; }
    bool isPersistent() const  { return m_persistent; }

    void setIdleTimeout(int seconds);

    bool start();
    bool start(const QString &pipeName);
    void stop();

    int clientCount() const { return m_clients.size(); }

    // ── Service handler dispatch ─────────────────────────────────────

    using ServiceCallHandler = std::function<void(
        const QString &method,
        const QJsonObject &args,
        std::function<void(bool ok, const QJsonObject &result)> respond)>;

    void installServiceHandler(const QString &serviceName,
                               ServiceCallHandler handler);

    void broadcastServiceEvent(const QString &serviceName,
                               const QJsonObject &eventData);

signals:
    void shutdownRequested();

private:
    struct ClientInfo {
        QString instanceId;
        int nextResponseId = 1;
    };

    void onNewConnection();
    void onClientDisconnected(QLocalSocket *socket);
    void onReadyRead(QLocalSocket *socket);
    void handleMessage(QLocalSocket *socket, const Ipc::Message &msg);
    void handleRequest(QLocalSocket *socket, const Ipc::Message &req);
    void handleNotification(QLocalSocket *socket, const Ipc::Message &msg);
    void sendMessage(QLocalSocket *socket, const Ipc::Message &msg);
    void broadcastNotification(const char *method,
                               const QJsonObject &params,
                               QLocalSocket *exclude = nullptr);

    std::unique_ptr<QLocalServer>        m_server;
    QHash<QLocalSocket *, ClientInfo>   m_clients;
    QHash<QLocalSocket *, QByteArray>   m_readBuffers;
    QHash<QString, QJsonObject>         m_sharedServices;
    QHash<qint64, QString>              m_managedProcesses;
    QHash<QString, ServiceCallHandler>  m_serviceHandlers;
    QTimer                              m_idleTimer;
    bool                                m_persistent = true;
    int                                 m_idleTimeoutSec = 60;

    // Process lifecycle — owned by ExoBridge
    struct StdStringHash {
        std::size_t operator()(const QString &s) const { return qHash(s); }
    };
    std::unordered_map<QString, std::unique_ptr<QProcess>, StdStringHash>
        m_ownedProcesses;
};
