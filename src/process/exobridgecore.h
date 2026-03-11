#pragma once

#include <QHash>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QTimer>

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
    explicit ExoBridgeCore(QObject *parent = nullptr)
        : QObject(parent)
    {
        // Idle shutdown: if no clients remain for N seconds, stop.
        // Disabled when m_persistent is true (default).
        m_idleTimer.setSingleShot(true);
        connect(&m_idleTimer, &QTimer::timeout, this, [this]() {
            if (m_clients.isEmpty()) {
                qInfo("[ExoBridge] No clients for %d s — shutting down",
                      m_idleTimeoutSec);
                emit shutdownRequested();
            }
        });
    }

    /// When persistent, the server never auto-shuts down due to idle timeout.
    /// Default: true (persistent).
    void setPersistent(bool on) { m_persistent = on; }
    bool isPersistent() const  { return m_persistent; }

    /// Set the idle timeout in seconds (only applies when not persistent).
    void setIdleTimeout(int seconds)
    {
        m_idleTimeoutSec = seconds;
        m_idleTimer.setInterval(seconds * 1000);
    }

    bool start()
    {
        m_server = new QLocalServer(this);
        m_server->setSocketOptions(QLocalServer::UserAccessOption);

        // Remove stale socket file (Unix) before listening
        QLocalServer::removeServer(Ipc::bridgePipeName());

        if (!m_server->listen(Ipc::bridgePipeName())) {
            qWarning("[ExoBridge] Failed to listen: %s",
                     qPrintable(m_server->errorString()));
            return false;
        }

        connect(m_server, &QLocalServer::newConnection,
                this, &ExoBridgeCore::onNewConnection);

        qInfo("[ExoBridge] Listening on \"%s\"",
              qPrintable(Ipc::bridgePipeName()));
        return true;
    }

    void stop()
    {
        // Notify all clients
        const auto msg = Ipc::Message::notification(
            QLatin1String(Ipc::Method::ServerShutdown));
        for (auto *sock : m_clients.keys())
            sendMessage(sock, msg);

        if (m_server) {
            m_server->close();
            m_server->deleteLater();
            m_server = nullptr;
        }
    }

    int clientCount() const { return m_clients.size(); }

signals:
    void shutdownRequested();

private:
    struct ClientInfo {
        QString instanceId;
        int nextResponseId = 1;
    };

    void onNewConnection()
    {
        while (auto *socket = m_server->nextPendingConnection()) {
            m_idleTimer.stop();

            ClientInfo info;
            m_clients.insert(socket, info);
            m_readBuffers.insert(socket, {});

            connect(socket, &QLocalSocket::readyRead,
                    this, [this, socket]() { onReadyRead(socket); });
            connect(socket, &QLocalSocket::disconnected,
                    this, [this, socket]() { onClientDisconnected(socket); });

            qInfo("[ExoBridge] Client connected (%lld total)",
                  static_cast<long long>(m_clients.size()));
        }
    }

    void onClientDisconnected(QLocalSocket *socket)
    {
        m_clients.remove(socket);
        m_readBuffers.remove(socket);
        socket->deleteLater();

        qInfo("[ExoBridge] Client disconnected (%lld remaining)",
              static_cast<long long>(m_clients.size()));

        if (m_clients.isEmpty() && !m_persistent)
            m_idleTimer.start();
    }

    void onReadyRead(QLocalSocket *socket)
    {
        auto &buf = m_readBuffers[socket];
        buf.append(socket->readAll());

        QByteArray payload;
        while (Ipc::tryUnframe(buf, payload)) {
            const auto msg = Ipc::Message::deserialize(payload);
            handleMessage(socket, msg);
        }
    }

    void handleMessage(QLocalSocket *socket, const Ipc::Message &msg)
    {
        if (msg.kind == Ipc::MessageKind::Request)
            handleRequest(socket, msg);
        else if (msg.kind == Ipc::MessageKind::Notification)
            handleNotification(socket, msg);
    }

    void handleRequest(QLocalSocket *socket, const Ipc::Message &req)
    {
        const auto method = req.method;

        if (method == QLatin1String(Ipc::Method::Handshake)) {
            auto &info = m_clients[socket];
            info.instanceId = req.params.value(QLatin1String("instanceId"))
                                  .toString();
            QJsonObject result;
            result[QLatin1String("serverVersion")] = QStringLiteral("1.0");
            result[QLatin1String("clients")]       = m_clients.size();
            sendMessage(socket, Ipc::Message::response(req.id, result));

        } else if (method == QLatin1String(Ipc::Method::ListServices)) {
            QJsonObject result;
            QJsonArray arr;
            for (const auto &name : m_sharedServices.keys())
                arr.append(name);
            result[QLatin1String("services")] = arr;
            sendMessage(socket, Ipc::Message::response(req.id, result));

        } else if (method == QLatin1String(Ipc::Method::RegisterService)) {
            const QString name = req.params.value(QLatin1String("name")).toString();
            if (name.isEmpty()) {
                sendMessage(socket, Ipc::Message::errorResponse(
                    req.id, Ipc::ErrorCode::InvalidParams,
                    QStringLiteral("Missing 'name'")));
                return;
            }
            m_sharedServices.insert(name, req.params);
            qInfo("[ExoBridge] Service registered: %s", qPrintable(name));
            sendMessage(socket, Ipc::Message::response(req.id, {}));

            // Notify all other clients
            QJsonObject event;
            event[QLatin1String("event")]   = QStringLiteral("registered");
            event[QLatin1String("service")] = name;
            broadcastNotification(Ipc::Method::ServiceEvent, event, socket);

        } else if (method == QLatin1String(Ipc::Method::UnregisterService)) {
            const QString name = req.params.value(QLatin1String("name")).toString();
            m_sharedServices.remove(name);
            sendMessage(socket, Ipc::Message::response(req.id, {}));

            QJsonObject event;
            event[QLatin1String("event")]   = QStringLiteral("unregistered");
            event[QLatin1String("service")] = name;
            broadcastNotification(Ipc::Method::ServiceEvent, event, socket);

        } else if (method == QLatin1String(Ipc::Method::CallService)) {
            const QString name = req.params.value(QLatin1String("service"))
                                     .toString();
            if (!m_sharedServices.contains(name)) {
                sendMessage(socket, Ipc::Message::errorResponse(
                    req.id, Ipc::ErrorCode::ServiceNotFound,
                    QStringLiteral("Service '%1' not found").arg(name)));
                return;
            }
            // For now: echo back the service metadata.
            // Real implementation will route to service handlers.
            QJsonObject result;
            result[QLatin1String("service")] = name;
            result[QLatin1String("status")]  = QStringLiteral("ok");
            sendMessage(socket, Ipc::Message::response(req.id, result));

        } else if (method == QLatin1String(Ipc::Method::ProcessList)) {
            QJsonObject result;
            QJsonArray arr;
            for (auto it = m_managedProcesses.begin();
                 it != m_managedProcesses.end(); ++it) {
                QJsonObject p;
                p[QLatin1String("pid")]  = static_cast<qint64>(it.key());
                p[QLatin1String("name")] = it.value();
                arr.append(p);
            }
            result[QLatin1String("processes")] = arr;
            sendMessage(socket, Ipc::Message::response(req.id, result));

        } else if (method == QLatin1String(Ipc::Method::Shutdown)) {
            sendMessage(socket, Ipc::Message::response(req.id, {}));
            QTimer::singleShot(100, this, [this]() {
                emit shutdownRequested();
            });

        } else {
            sendMessage(socket, Ipc::Message::errorResponse(
                req.id, Ipc::ErrorCode::MethodNotFound,
                QStringLiteral("Unknown method: %1").arg(method)));
        }
    }

    void handleNotification(QLocalSocket * /*socket*/,
                            const Ipc::Message &msg)
    {
        // Notifications are fire-and-forget.  Extend as needed.
        Q_UNUSED(msg)
    }

    void sendMessage(QLocalSocket *socket, const Ipc::Message &msg)
    {
        if (!socket || socket->state() != QLocalSocket::ConnectedState)
            return;
        const QByteArray payload = msg.serialize();
        socket->write(Ipc::frame(payload));
        socket->flush();
    }

    void broadcastNotification(const char *method,
                               const QJsonObject &params,
                               QLocalSocket *exclude = nullptr)
    {
        const auto msg = Ipc::Message::notification(
            QLatin1String(method), params);
        for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
            if (it.key() != exclude)
                sendMessage(it.key(), msg);
        }
    }

    QLocalServer                       *m_server = nullptr;
    QHash<QLocalSocket *, ClientInfo>   m_clients;
    QHash<QLocalSocket *, QByteArray>   m_readBuffers;
    QHash<QString, QJsonObject>         m_sharedServices;
    QHash<qint64, QString>              m_managedProcesses;
    QTimer                              m_idleTimer;
    bool                                m_persistent = true;
    int                                 m_idleTimeoutSec = 60;
};
