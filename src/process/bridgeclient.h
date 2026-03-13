#pragma once

#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>
#include <QTimer>
#include <QHash>

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
    explicit BridgeClient(QObject *parent = nullptr);
    ~BridgeClient() override;

    enum class State { Disconnected, Connecting, Connected };

    State state() const { return m_state; }
    bool isConnected() const { return m_state == State::Connected; }

    void connectToBridge();
    void disconnect();

    void setAutoReconnect(bool on) { m_autoReconnect = on; }

    // ── Service calls ─────────────────────────────────────────────────

    using ResponseCallback = std::function<void(bool ok, const QJsonObject &result)>;

    void callService(const QString &service, const QString &method,
                     const QJsonObject &args, ResponseCallback cb);
    void listServices(ResponseCallback cb);
    void registerService(const QString &name, const QJsonObject &metadata,
                         ResponseCallback cb = {});
    void unregisterService(const QString &name, ResponseCallback cb = {});
    void requestShutdown();
    void notify(const QString &method, const QJsonObject &params = {});

signals:
    void connected();
    void disconnected();
    void connectionFailed(const QString &error);
    void serviceEvent(const QString &service, const QString &event);
    void serverShuttingDown();

private:
    static QString serverLockPath();
    bool isServerProcessRunning() const;
    void tryConnect();
    void launchBridge();
    void onConnected();
    void onDisconnected();
    void onError(QLocalSocket::LocalSocketError err);
    void onReadyRead();
    void handleMessage(const Ipc::Message &msg);
    void sendRequest(const QString &method, const QJsonObject &params,
                     ResponseCallback cb = {});
    void sendRaw(const Ipc::Message &msg);

    QLocalSocket *m_socket = nullptr;
    State         m_state  = State::Disconnected;
    QByteArray    m_buffer;
    QTimer        m_reconnectTimer;
    bool          m_autoReconnect = true;
    int           m_nextId        = 1;
    QString       m_instanceId;

    QHash<int, ResponseCallback> m_pendingCalls;
};
