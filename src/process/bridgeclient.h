#pragma once

#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>
#include <QCoreApplication>
#include <QFile>
#include <QLockFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>

#include <functional>

#include "ipcprotocol.h"

// ── BridgeClient ─────────────────────────────────────────────────────────────
//
// IDE-side connection to the ExoBridge daemon.
//
// Usage:
//   auto *client = new BridgeClient(this);
//   connect(client, &BridgeClient::connected, ...);
//   client->connectToBridge();          // auto-launches daemon if needed
//   client->callService("search", "query", params, [](QJsonObject result){ });

class BridgeClient : public QObject
{
    Q_OBJECT

public:
    explicit BridgeClient(QObject *parent = nullptr)
        : QObject(parent)
        , m_socket(new QLocalSocket(this))
    {
        connect(m_socket, &QLocalSocket::connected,
                this, &BridgeClient::onConnected);
        connect(m_socket, &QLocalSocket::disconnected,
                this, &BridgeClient::onDisconnected);
        connect(m_socket, &QLocalSocket::readyRead,
                this, &BridgeClient::onReadyRead);
        connect(m_socket, &QLocalSocket::errorOccurred,
                this, &BridgeClient::onError);

        // Reconnect timer
        m_reconnectTimer.setInterval(3000);
        m_reconnectTimer.setSingleShot(true);
        connect(&m_reconnectTimer, &QTimer::timeout,
                this, &BridgeClient::tryConnect);
    }

    ~BridgeClient() override
    {
        disconnect();
    }

    enum class State { Disconnected, Connecting, Connected };

    State state() const { return m_state; }
    bool isConnected() const { return m_state == State::Connected; }

    /// Connect to ExoBridge.  If the daemon isn't running, launches it.
    void connectToBridge()
    {
        if (m_state != State::Disconnected)
            return;
        tryConnect();
    }

    /// Gracefully disconnect.
    void disconnect()
    {
        m_reconnectTimer.stop();
        m_autoReconnect = false;
        if (m_socket->state() != QLocalSocket::UnconnectedState)
            m_socket->disconnectFromServer();
        m_state = State::Disconnected;
    }

    /// Enable/disable auto-reconnect on disconnect (default: true).
    void setAutoReconnect(bool on) { m_autoReconnect = on; }

    // ── Service calls ─────────────────────────────────────────────────

    using ResponseCallback = std::function<void(bool ok, const QJsonObject &result)>;

    /// Call a shared service method.
    void callService(const QString &service, const QString &method,
                     const QJsonObject &args, ResponseCallback cb)
    {
        QJsonObject params;
        params[QLatin1String("service")] = service;
        params[QLatin1String("method")]  = method;
        params[QLatin1String("args")]    = args;
        sendRequest(QLatin1String(Ipc::Method::CallService), params, std::move(cb));
    }

    /// List available shared services.
    void listServices(ResponseCallback cb)
    {
        sendRequest(QLatin1String(Ipc::Method::ListServices), {}, std::move(cb));
    }

    /// Register a service on ExoBridge.
    void registerService(const QString &name, const QJsonObject &metadata,
                         ResponseCallback cb = {})
    {
        QJsonObject params;
        params[QLatin1String("name")] = name;
        if (!metadata.isEmpty())
            params[QLatin1String("metadata")] = metadata;
        sendRequest(QLatin1String(Ipc::Method::RegisterService),
                    params, std::move(cb));
    }

    /// Unregister a service.
    void unregisterService(const QString &name, ResponseCallback cb = {})
    {
        QJsonObject params;
        params[QLatin1String("name")] = name;
        sendRequest(QLatin1String(Ipc::Method::UnregisterService),
                    params, std::move(cb));
    }

    /// Request bridge shutdown (all clients will be notified).
    void requestShutdown()
    {
        sendRequest(QLatin1String(Ipc::Method::Shutdown), {});
    }

    /// Send a fire-and-forget notification to the bridge.
    void notify(const QString &method, const QJsonObject &params = {})
    {
        sendRaw(Ipc::Message::notification(method, params));
    }

signals:
    void connected();
    void disconnected();
    void connectionFailed(const QString &error);
    void serviceEvent(const QString &service, const QString &event);
    void serverShuttingDown();

private:
    static QString serverLockPath()
    {
        const QString dir = QStandardPaths::writableLocation(
            QStandardPaths::TempLocation);
        const QString user = qEnvironmentVariable("USERNAME",
                                 qEnvironmentVariable("USER",
                                     QStringLiteral("default")));
        return dir + QStringLiteral("/exobridge-%1.lock").arg(user);
    }

    bool isServerProcessRunning() const
    {
        QLockFile probe(serverLockPath());
        if (probe.tryLock(0)) {
            // We acquired it → no server holds the lock
            probe.unlock();
            return false;
        }
        return true; // Lock held → server is alive
    }

    void tryConnect()
    {
        m_state = State::Connecting;
        m_socket->connectToServer(Ipc::bridgePipeName());

        // If the connection doesn't succeed quickly, check if the bridge is alive
        QTimer::singleShot(500, this, [this]() {
            if (m_state != State::Connecting
                || m_socket->state() != QLocalSocket::UnconnectedState)
                return;

            if (isServerProcessRunning()) {
                // Bridge is alive but pipe not ready yet — retry shortly
                QTimer::singleShot(1000, this, [this]() {
                    if (m_state == State::Connecting)
                        m_socket->connectToServer(Ipc::bridgePipeName());
                });
            } else {
                // Bridge is not running — launch it
                launchBridge();
            }
        });
    }

    void launchBridge()
    {
        // Find the bridge executable next to our own binary
        const QString dir = QCoreApplication::applicationDirPath();
        const QString bridgePath =
            dir + QStringLiteral("/exobridge")
#ifdef Q_OS_WIN
            + QStringLiteral(".exe")
#endif
            ;

        if (!QFile::exists(bridgePath)) {
            qWarning("[BridgeClient] ExoBridge binary not found: %s",
                     qPrintable(bridgePath));
            m_state = State::Disconnected;
            emit connectionFailed(tr("ExoBridge binary not found"));
            return;
        }

        // Start detached — the bridge runs independently of this IDE instance
        if (!QProcess::startDetached(bridgePath, {})) {
            qWarning("[BridgeClient] Failed to launch ExoBridge");
            m_state = State::Disconnected;
            emit connectionFailed(tr("Failed to launch ExoBridge"));
            return;
        }

        qInfo("[BridgeClient] Launched ExoBridge, retrying connect…");

        // Wait a moment for the bridge to start listening, then retry
        QTimer::singleShot(500, this, [this]() {
            m_socket->connectToServer(Ipc::bridgePipeName());
        });
    }

    void onConnected()
    {
        m_state = State::Connected;
        m_buffer.clear();

        // Handshake
        QJsonObject params;
        params[QLatin1String("instanceId")] = m_instanceId;
        params[QLatin1String("pid")]        = static_cast<qint64>(
            QCoreApplication::applicationPid());
        sendRequest(QLatin1String(Ipc::Method::Handshake), params);

        qInfo("[BridgeClient] Connected to ExoBridge");
        emit connected();
    }

    void onDisconnected()
    {
        m_state = State::Disconnected;
        m_pendingCalls.clear();
        qInfo("[BridgeClient] Disconnected from ExoBridge");
        emit disconnected();

        if (m_autoReconnect)
            m_reconnectTimer.start();
    }

    void onError(QLocalSocket::LocalSocketError err)
    {
        if (m_state == State::Connecting
            && err == QLocalSocket::ServerNotFoundError) {
            // Bridge not running yet — try launching
            launchBridge();
            return;
        }

        if (m_state == State::Connecting) {
            m_state = State::Disconnected;
            emit connectionFailed(m_socket->errorString());

            if (m_autoReconnect)
                m_reconnectTimer.start();
        }
    }

    void onReadyRead()
    {
        m_buffer.append(m_socket->readAll());

        QByteArray payload;
        while (Ipc::tryUnframe(m_buffer, payload)) {
            const auto msg = Ipc::Message::deserialize(payload);
            handleMessage(msg);
        }
    }

    void handleMessage(const Ipc::Message &msg)
    {
        if (msg.kind == Ipc::MessageKind::Response) {
            auto it = m_pendingCalls.find(msg.id);
            if (it != m_pendingCalls.end()) {
                auto cb = std::move(it.value());
                m_pendingCalls.erase(it);
                if (cb)
                    cb(!msg.isError, msg.isError ? msg.error : msg.result);
            }
        } else if (msg.kind == Ipc::MessageKind::Notification) {
            if (msg.method == QLatin1String(Ipc::Method::ServiceEvent)) {
                const QString svc   = msg.params.value(
                    QLatin1String("service")).toString();
                const QString event = msg.params.value(
                    QLatin1String("event")).toString();
                emit serviceEvent(svc, event);
            } else if (msg.method
                       == QLatin1String(Ipc::Method::ServerShutdown)) {
                emit serverShuttingDown();
            }
        }
    }

    void sendRequest(const QString &method, const QJsonObject &params,
                     ResponseCallback cb = {})
    {
        const int id = m_nextId++;
        if (cb)
            m_pendingCalls.insert(id, std::move(cb));
        sendRaw(Ipc::Message::request(id, method, params));
    }

    void sendRaw(const Ipc::Message &msg)
    {
        if (m_socket->state() != QLocalSocket::ConnectedState)
            return;
        const QByteArray payload = msg.serialize();
        m_socket->write(Ipc::frame(payload));
        m_socket->flush();
    }

    QLocalSocket *m_socket = nullptr;
    State         m_state  = State::Disconnected;
    QByteArray    m_buffer;
    QTimer        m_reconnectTimer;
    bool          m_autoReconnect = true;
    int           m_nextId        = 1;
    QString       m_instanceId    = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QHash<int, ResponseCallback> m_pendingCalls;
};
