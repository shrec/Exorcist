#include "exobridgecore.h"

ExoBridgeCore::ExoBridgeCore(QObject *parent)
    : QObject(parent)
{
    m_idleTimer.setSingleShot(true);
    connect(&m_idleTimer, &QTimer::timeout, this, [this]() {
        if (m_clients.isEmpty()) {
            qInfo("[ExoBridge] No clients for %d s — shutting down",
                  m_idleTimeoutSec);
            emit shutdownRequested();
        }
    });
}

void ExoBridgeCore::setIdleTimeout(int seconds)
{
    m_idleTimeoutSec = seconds;
    m_idleTimer.setInterval(seconds * 1000);
}

bool ExoBridgeCore::start()
{
    return start(Ipc::bridgePipeName());
}

bool ExoBridgeCore::start(const QString &pipeName)
{
    m_server = std::make_unique<QLocalServer>();
    m_server->setSocketOptions(QLocalServer::UserAccessOption);

    QLocalServer::removeServer(pipeName);

    if (!m_server->listen(pipeName)) {
        qWarning("[ExoBridge] Failed to listen: %s",
                 qPrintable(m_server->errorString()));
        return false;
    }

    connect(m_server.get(), &QLocalServer::newConnection,
            this, &ExoBridgeCore::onNewConnection);

    qInfo("[ExoBridge] Listening on \"%s\"",
          qPrintable(pipeName));
    return true;
}

void ExoBridgeCore::stop()
{
    const auto msg = Ipc::Message::notification(
        QLatin1String(Ipc::Method::ServerShutdown));
    for (auto *sock : m_clients.keys())
        sendMessage(sock, msg);

    for (auto *sock : m_clients.keys()) {
        sock->disconnectFromServer();
        sock->setParent(nullptr);
    }
    m_clients.clear();
    m_readBuffers.clear();

    if (m_server) {
        m_server->close();
        m_server.reset();
    }
}

void ExoBridgeCore::installServiceHandler(const QString &serviceName,
                                          ServiceCallHandler handler)
{
    m_serviceHandlers.insert(serviceName, std::move(handler));
    if (!m_sharedServices.contains(serviceName)) {
        QJsonObject meta;
        meta[QLatin1String("name")] = serviceName;
        meta[QLatin1String("type")] = QStringLiteral("builtin");
        m_sharedServices.insert(serviceName, meta);
    }
}

void ExoBridgeCore::broadcastServiceEvent(const QString &serviceName,
                                          const QJsonObject &eventData)
{
    QJsonObject params;
    params[QLatin1String("service")] = serviceName;
    params[QLatin1String("event")]   = QStringLiteral("data");
    params[QLatin1String("data")]    = eventData;
    broadcastNotification(Ipc::Method::ServiceEvent, params);
}

void ExoBridgeCore::onNewConnection()
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

void ExoBridgeCore::onClientDisconnected(QLocalSocket *socket)
{
    m_clients.remove(socket);
    m_readBuffers.remove(socket);
    socket->deleteLater();

    qInfo("[ExoBridge] Client disconnected (%lld remaining)",
          static_cast<long long>(m_clients.size()));

    if (m_clients.isEmpty() && !m_persistent)
        m_idleTimer.start();
}

void ExoBridgeCore::onReadyRead(QLocalSocket *socket)
{
    auto &buf = m_readBuffers[socket];
    buf.append(socket->readAll());

    QByteArray payload;
    while (Ipc::tryUnframe(buf, payload)) {
        const auto msg = Ipc::Message::deserialize(payload);
        handleMessage(socket, msg);
    }
}

void ExoBridgeCore::handleMessage(QLocalSocket *socket, const Ipc::Message &msg)
{
    if (msg.kind == Ipc::MessageKind::Request)
        handleRequest(socket, msg);
    else if (msg.kind == Ipc::MessageKind::Notification)
        handleNotification(socket, msg);
}

void ExoBridgeCore::handleRequest(QLocalSocket *socket, const Ipc::Message &req)
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
        const QString svcMethod = req.params.value(QLatin1String("method"))
                                      .toString();
        const QJsonObject args = req.params.value(QLatin1String("args"))
                                     .toObject();

        auto handlerIt = m_serviceHandlers.find(name);
        if (handlerIt != m_serviceHandlers.end()) {
            const int reqId = req.id;
            handlerIt.value()(
                svcMethod, args,
                [this, socket, reqId](bool ok, const QJsonObject &result) {
                    if (ok)
                        sendMessage(socket,
                                    Ipc::Message::response(reqId, result));
                    else
                        sendMessage(socket,
                                    Ipc::Message::errorResponse(
                                        reqId, Ipc::ErrorCode::InternalError,
                                        result.value(QLatin1String("message"))
                                            .toString()));
                });
            return;
        }

        if (!m_sharedServices.contains(name)) {
            sendMessage(socket, Ipc::Message::errorResponse(
                req.id, Ipc::ErrorCode::ServiceNotFound,
                QStringLiteral("Service '%1' not found").arg(name)));
            return;
        }
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

    } else if (method == QLatin1String(Ipc::Method::ProcessStart)) {
        const QString name    = req.params.value(QLatin1String("name")).toString();
        const QString program = req.params.value(QLatin1String("program")).toString();
        const QJsonArray argsArr = req.params.value(QLatin1String("args")).toArray();
        const QString workDir = req.params.value(QLatin1String("workDir")).toString();

        if (name.isEmpty() || program.isEmpty()) {
            sendMessage(socket, Ipc::Message::errorResponse(
                req.id, Ipc::ErrorCode::InvalidParams,
                QStringLiteral("Missing 'name' or 'program'")));
            return;
        }

        for (auto &kv : m_managedProcesses) {
            if (kv == name) {
                QJsonObject result;
                result[QLatin1String("name")]   = name;
                result[QLatin1String("status")]  = QStringLiteral("already_running");
                sendMessage(socket, Ipc::Message::response(req.id, result));
                return;
            }
        }

        QStringList args;
        for (const auto &a : argsArr)
            args << a.toString();

        auto proc = std::make_unique<QProcess>();
        proc->setProgram(program);
        proc->setArguments(args);
        if (!workDir.isEmpty())
            proc->setWorkingDirectory(workDir);

        auto *raw = proc.get();
        connect(raw, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, name](int exitCode, QProcess::ExitStatus status) {
            for (auto it = m_managedProcesses.begin(); it != m_managedProcesses.end(); ++it) {
                if (it.value() == name) {
                    m_managedProcesses.erase(it);
                    break;
                }
            }
            m_ownedProcesses.erase(name);

            QJsonObject event;
            event[QLatin1String("name")]     = name;
            event[QLatin1String("exitCode")] = exitCode;
            event[QLatin1String("crashed")]  = (status == QProcess::CrashExit);
            broadcastNotification(Ipc::Method::ProcessExited, event);
        });

        proc->start(QIODevice::ReadWrite);
        if (!raw->waitForStarted(5000)) {
            sendMessage(socket, Ipc::Message::errorResponse(
                req.id, Ipc::ErrorCode::InternalError,
                QStringLiteral("Failed to start process '%1': %2")
                    .arg(name, raw->errorString())));
            return;
        }

        const qint64 pid = raw->processId();
        m_managedProcesses.insert(pid, name);
        m_ownedProcesses[name] = std::move(proc);

        QJsonObject result;
        result[QLatin1String("name")] = name;
        result[QLatin1String("pid")]  = pid;
        sendMessage(socket, Ipc::Message::response(req.id, result));

    } else if (method == QLatin1String(Ipc::Method::ProcessKill)) {
        const QString name = req.params.value(QLatin1String("name")).toString();
        auto it = m_ownedProcesses.find(name);
        if (it == m_ownedProcesses.end()) {
            sendMessage(socket, Ipc::Message::errorResponse(
                req.id, Ipc::ErrorCode::ProcessNotFound,
                QStringLiteral("Process '%1' not found").arg(name)));
            return;
        }
        it->second->kill();
        QJsonObject result;
        result[QLatin1String("name")]   = name;
        result[QLatin1String("status")] = QStringLiteral("killed");
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

void ExoBridgeCore::handleNotification(QLocalSocket * /*socket*/,
                                       const Ipc::Message &msg)
{
    Q_UNUSED(msg)
}

void ExoBridgeCore::sendMessage(QLocalSocket *socket, const Ipc::Message &msg)
{
    if (!socket || socket->state() != QLocalSocket::ConnectedState)
        return;
    const QByteArray payload = msg.serialize();
    socket->write(Ipc::frame(payload));
    socket->flush();
}

void ExoBridgeCore::broadcastNotification(const char *method,
                                          const QJsonObject &params,
                                          QLocalSocket *exclude)
{
    const auto msg = Ipc::Message::notification(
        QLatin1String(method), params);
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it.key() != exclude)
            sendMessage(it.key(), msg);
    }
}
