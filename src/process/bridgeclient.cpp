#include "bridgeclient.h"

#include <QCoreApplication>
#include <QFile>
#include <QLockFile>
#include <QProcess>
#include <QStandardPaths>
#include <QUuid>

BridgeClient::BridgeClient(QObject *parent)
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

    m_reconnectTimer.setInterval(3000);
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout,
            this, &BridgeClient::tryConnect);
}

BridgeClient::~BridgeClient()
{
    disconnect();
}

void BridgeClient::connectToBridge()
{
    if (m_state != State::Disconnected)
        return;
    tryConnect();
}

void BridgeClient::connectToServer(const QString &pipeName)
{
    if (m_state != State::Disconnected)
        return;
    m_state = State::Connecting;
    m_socket->connectToServer(pipeName);
}

void BridgeClient::disconnect()
{
    m_reconnectTimer.stop();
    m_autoReconnect = false;
    if (m_socket->state() != QLocalSocket::UnconnectedState)
        m_socket->disconnectFromServer();
    m_state = State::Disconnected;
}

void BridgeClient::callService(const QString &service, const QString &method,
                               const QJsonObject &args, ResponseCallback cb)
{
    QJsonObject params;
    params[QLatin1String("service")] = service;
    params[QLatin1String("method")]  = method;
    params[QLatin1String("args")]    = args;
    sendRequest(QLatin1String(Ipc::Method::CallService), params, std::move(cb));
}

void BridgeClient::listServices(ResponseCallback cb)
{
    sendRequest(QLatin1String(Ipc::Method::ListServices), {}, std::move(cb));
}

void BridgeClient::registerService(const QString &name, const QJsonObject &metadata,
                                   ResponseCallback cb)
{
    QJsonObject params;
    params[QLatin1String("name")] = name;
    if (!metadata.isEmpty())
        params[QLatin1String("metadata")] = metadata;
    sendRequest(QLatin1String(Ipc::Method::RegisterService),
                params, std::move(cb));
}

void BridgeClient::unregisterService(const QString &name, ResponseCallback cb)
{
    QJsonObject params;
    params[QLatin1String("name")] = name;
    sendRequest(QLatin1String(Ipc::Method::UnregisterService),
                params, std::move(cb));
}

void BridgeClient::requestShutdown()
{
    sendRequest(QLatin1String(Ipc::Method::Shutdown), {});
}

void BridgeClient::notify(const QString &method, const QJsonObject &params)
{
    sendRaw(Ipc::Message::notification(method, params));
}

QString BridgeClient::serverLockPath()
{
    const QString dir = QStandardPaths::writableLocation(
        QStandardPaths::TempLocation);
    const QString user = qEnvironmentVariable("USERNAME",
                             qEnvironmentVariable("USER",
                                 QStringLiteral("default")));
    return dir + QStringLiteral("/exobridge-%1.lock").arg(user);
}

bool BridgeClient::isServerProcessRunning() const
{
    QLockFile probe(serverLockPath());
    if (probe.tryLock(0)) {
        probe.unlock();
        return false;
    }
    return true;
}

void BridgeClient::tryConnect()
{
    m_state = State::Connecting;
    m_socket->connectToServer(Ipc::bridgePipeName());

    QTimer::singleShot(500, this, [this]() {
        if (m_state != State::Connecting
            || m_socket->state() != QLocalSocket::UnconnectedState)
            return;

        if (isServerProcessRunning()) {
            QTimer::singleShot(1000, this, [this]() {
                if (m_state == State::Connecting)
                    m_socket->connectToServer(Ipc::bridgePipeName());
            });
        } else {
            launchBridge();
        }
    });
}

void BridgeClient::launchBridge()
{
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

    // Auto-quit after 5 minutes with no IDE clients.
    // When Exorcist closes, its socket disconnects; the idle timer then starts
    // and ExoBridge exits cleanly — no orphan process left in Task Manager.
    const QStringList bridgeArgs{
        QStringLiteral("--idle-timeout"),
        QStringLiteral("300"),
    };

    if (!QProcess::startDetached(bridgePath, bridgeArgs)) {
        qWarning("[BridgeClient] Failed to launch ExoBridge");
        m_state = State::Disconnected;
        emit connectionFailed(tr("Failed to launch ExoBridge"));
        return;
    }

    qInfo("[BridgeClient] Launched ExoBridge, retrying connect…");

    QTimer::singleShot(500, this, [this]() {
        m_socket->connectToServer(Ipc::bridgePipeName());
    });
}

void BridgeClient::onConnected()
{
    m_state = State::Connected;
    m_buffer.clear();
    m_gracefulShutdown = false;

    QJsonObject params;
    params[QLatin1String("instanceId")] = m_instanceId;
    params[QLatin1String("pid")]        = static_cast<qint64>(
        QCoreApplication::applicationPid());
    sendRequest(QLatin1String(Ipc::Method::Handshake), params);

    qInfo("[BridgeClient] Connected to ExoBridge");
    emit connected();
}

void BridgeClient::onDisconnected()
{
    // Drain any remaining data — the server may have sent a shutdown
    // notification just before closing the connection.
    if (m_socket->bytesAvailable() > 0)
        onReadyRead();

    const bool wasCrash = (m_state == State::Connected && !m_gracefulShutdown);
    m_state = State::Disconnected;
    m_pendingCalls.clear();
    m_gracefulShutdown = false;

    if (wasCrash) {
        qWarning("[BridgeClient] ExoBridge crashed — will restart");
        emit bridgeCrashed();
    } else {
        qInfo("[BridgeClient] Disconnected from ExoBridge");
    }
    emit disconnected();

    if (m_autoReconnect)
        m_reconnectTimer.start();
}

void BridgeClient::onError(QLocalSocket::LocalSocketError err)
{
    if (m_state == State::Connecting
        && err == QLocalSocket::ServerNotFoundError) {
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

void BridgeClient::onReadyRead()
{
    m_buffer.append(m_socket->readAll());

    // Guard: cap read buffer at 64 MB to prevent OOM.
    static constexpr int MaxReadBuffer = 64 * 1024 * 1024;
    if (m_buffer.size() > MaxReadBuffer) {
        qWarning("[BridgeClient] Read buffer exceeded %d MB — "
                 "clearing", MaxReadBuffer / (1024 * 1024));
        m_buffer.clear();
        return;
    }

    QByteArray payload;
    while (Ipc::tryUnframe(m_buffer, payload)) {
        const auto msg = Ipc::Message::deserialize(payload);
        handleMessage(msg);
    }

    // Reclaim memory when buffer is fully consumed.
    if (m_buffer.isEmpty())
        m_buffer.squeeze();
}

void BridgeClient::handleMessage(const Ipc::Message &msg)
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
            m_gracefulShutdown = true;
            emit serverShuttingDown();
        }
    }
}

void BridgeClient::sendRequest(const QString &method, const QJsonObject &params,
                               ResponseCallback cb)
{
    const int id = m_nextId++;
    if (cb)
        m_pendingCalls.insert(id, std::move(cb));
    sendRaw(Ipc::Message::request(id, method, params));
}

void BridgeClient::sendRaw(const Ipc::Message &msg)
{
    if (m_socket->state() != QLocalSocket::ConnectedState)
        return;
    const QByteArray payload = msg.serialize();
    m_socket->write(Ipc::frame(payload));
    m_socket->flush();
}
