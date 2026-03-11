#include "mcpclient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

McpClient::McpClient(QObject *parent)
    : QObject(parent)
{
}

McpClient::~McpClient()
{
    disconnectAll();
}

void McpClient::addServer(const McpServerConfig &config)
{
    if (m_servers.contains(config.name))
        return;
    ServerState state;
    state.config = config;
    m_servers.insert(config.name, state);
}

void McpClient::connectAll()
{
    for (auto it = m_servers.begin(); it != m_servers.end(); ++it)
        connectServer(it.key());
}

bool McpClient::connectServer(const QString &name)
{
    if (!m_servers.contains(name))
        return false;

    auto &state = m_servers[name];
    if (state.process)
        return state.initialized;

    state.process = new QProcess(this);
    state.process->setProgram(state.config.command);
    state.process->setArguments(state.config.args);

    // Set environment variables
    if (!state.config.env.isEmpty()) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        for (auto it = state.config.env.begin(); it != state.config.env.end(); ++it)
            env.insert(it.key(), it.value());
        state.process->setProcessEnvironment(env);
    }

    connect(state.process, &QProcess::readyReadStandardOutput, this,
            [this, name]() { onProcessReadyRead(name); });
    connect(state.process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, name](int code, QProcess::ExitStatus) {
                onProcessFinished(name, code);
            });
    connect(state.process, &QProcess::errorOccurred, this,
            [this, name](QProcess::ProcessError err) {
                Q_UNUSED(err)
                if (m_servers.contains(name))
                    emit serverError(name, m_servers[name].process->errorString());
            });

    state.process->start(QIODevice::ReadWrite);
    if (!state.process->waitForStarted(10000)) {
        emit serverError(name, state.process->errorString());
        state.process->deleteLater();
        state.process = nullptr;
        return false;
    }

    // Send initialize request
    QJsonObject params;
    params[QLatin1String("protocolVersion")] = QLatin1String("2024-11-05");
    QJsonObject clientInfo;
    clientInfo[QLatin1String("name")] = QLatin1String("exorcist");
    clientInfo[QLatin1String("version")] = QLatin1String("1.0.0");
    params[QLatin1String("clientInfo")] = clientInfo;
    QJsonObject capabilities;
    params[QLatin1String("capabilities")] = capabilities;

    sendRequest(name, QStringLiteral("initialize"), params);
    return true;
}

void McpClient::disconnectServer(const QString &name)
{
    if (!m_servers.contains(name))
        return;
    auto &state = m_servers[name];
    if (state.process) {
        state.process->kill();
        state.process->waitForFinished(3000);
        state.process->deleteLater();
        state.process = nullptr;
    }
    state.initialized = false;
    state.tools.clear();
    state.buffer.clear();
    state.pendingRequests.clear();
    emit serverDisconnected(name);
}

void McpClient::disconnectAll()
{
    const QStringList names = m_servers.keys();
    for (const QString &name : names)
        disconnectServer(name);
}

QList<McpToolInfo> McpClient::allTools() const
{
    QList<McpToolInfo> result;
    for (const auto &state : m_servers) {
        if (state.initialized)
            result.append(state.tools);
    }
    return result;
}

void McpClient::callTool(const QString &toolName, const QJsonObject &arguments,
                         const QString &requestId)
{
    // Find which server owns this tool
    for (auto it = m_servers.begin(); it != m_servers.end(); ++it) {
        if (!it->initialized) continue;
        for (const auto &tool : it->tools) {
            if (tool.name == toolName) {
                QJsonObject params;
                params[QLatin1String("name")] = toolName;
                params[QLatin1String("arguments")] = arguments;
                sendRequest(it.key(), QStringLiteral("tools/call"), params, requestId);
                return;
            }
        }
    }
    // Tool not found
    McpToolResult result;
    result.ok = false;
    result.error = QStringLiteral("Tool '%1' not found on any connected MCP server").arg(toolName);
    emit toolCallFinished(requestId, result);
}

QStringList McpClient::connectedServers() const
{
    QStringList result;
    for (auto it = m_servers.begin(); it != m_servers.end(); ++it) {
        if (it->initialized)
            result.append(it.key());
    }
    return result;
}

bool McpClient::isConnected(const QString &name) const
{
    return m_servers.contains(name) && m_servers[name].initialized;
}

// ── Private ──────────────────────────────────────────────────────────────────

void McpClient::onProcessReadyRead(const QString &name)
{
    if (!m_servers.contains(name)) return;
    auto &state = m_servers[name];
    state.buffer.append(state.process->readAllStandardOutput());

    // MCP uses newline-delimited JSON-RPC messages
    int consumed = 0;
    while (true) {
        const int nlPos = state.buffer.indexOf('\n', consumed);
        if (nlPos < 0) break;
        const QByteArray line = state.buffer.mid(consumed, nlPos - consumed).trimmed();
        consumed = nlPos + 1;

        if (line.isEmpty()) continue;
        QJsonParseError parseErr;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &parseErr);
        if (doc.isNull()) continue;
        handleMessage(name, doc.object());
    }
    if (consumed > 0)
        state.buffer.remove(0, consumed);
}

void McpClient::onProcessFinished(const QString &name, int exitCode)
{
    Q_UNUSED(exitCode)
    if (!m_servers.contains(name)) return;
    auto &state = m_servers[name];
    state.initialized = false;
    state.process->deleteLater();
    state.process = nullptr;
    emit serverDisconnected(name);
}

void McpClient::handleMessage(const QString &name, const QJsonObject &msg)
{
    if (!m_servers.contains(name)) return;
    auto &state = m_servers[name];

    // Check if it's a response (has "id")
    if (msg.contains(QLatin1String("id"))) {
        const int msgId = msg[QLatin1String("id")].toInt();
        const QJsonObject result = msg[QLatin1String("result")].toObject();
        const QJsonObject error  = msg[QLatin1String("error")].toObject();

        // Check if this was the initialize response
        if (!state.initialized && result.contains(QLatin1String("protocolVersion"))) {
            state.initialized = true;

            // Send initialized notification
            QJsonObject notif;
            notif[QLatin1String("jsonrpc")] = QLatin1String("2.0");
            notif[QLatin1String("method")]  = QLatin1String("notifications/initialized");
            sendMessage(name, notif);

            emit serverConnected(name);

            // Request tool list
            sendRequest(name, QStringLiteral("tools/list"), QJsonObject{});
            return;
        }

        // Check if this is a tools/list response
        if (result.contains(QLatin1String("tools"))) {
            state.tools.clear();
            const QJsonArray toolsArr = result[QLatin1String("tools")].toArray();
            for (const QJsonValue &v : toolsArr) {
                const QJsonObject tObj = v.toObject();
                McpToolInfo info;
                info.name = tObj[QLatin1String("name")].toString();
                info.description = tObj[QLatin1String("description")].toString();
                info.inputSchema = tObj[QLatin1String("inputSchema")].toObject();
                info.serverName = name;
                state.tools.append(info);
            }
            emit toolsDiscovered(name, state.tools);
            return;
        }

        // Check if this is a tool call response
        if (state.pendingRequests.contains(msgId)) {
            const QString externalId = state.pendingRequests.take(msgId);
            McpToolResult res;
            if (!error.isEmpty()) {
                res.ok = false;
                res.error = error[QLatin1String("message")].toString();
            } else {
                res.ok = true;
                res.content = result[QLatin1String("content")].toArray();
                // Extract text content
                for (const QJsonValue &v : res.content) {
                    const QJsonObject piece = v.toObject();
                    if (piece[QLatin1String("type")].toString() == QLatin1String("text"))
                        res.text += piece[QLatin1String("text")].toString();
                }
                if (result.contains(QLatin1String("isError")) && result[QLatin1String("isError")].toBool()) {
                    res.ok = false;
                    res.error = res.text;
                }
            }
            emit toolCallFinished(externalId, res);
        }
    }
    // Notifications from server — currently ignored (could log them)
}

void McpClient::sendMessage(const QString &name, const QJsonObject &msg)
{
    if (!m_servers.contains(name)) return;
    auto &state = m_servers[name];
    if (!state.process) return;

    const QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact) + '\n';
    state.process->write(data);
}

int McpClient::sendRequest(const QString &name, const QString &method,
                           const QJsonObject &params, const QString &externalId)
{
    if (!m_servers.contains(name)) return -1;
    auto &state = m_servers[name];

    const int id = state.nextId++;
    QJsonObject msg;
    msg[QLatin1String("jsonrpc")] = QLatin1String("2.0");
    msg[QLatin1String("id")] = id;
    msg[QLatin1String("method")] = method;
    msg[QLatin1String("params")] = params;

    if (!externalId.isEmpty())
        state.pendingRequests.insert(id, externalId);

    sendMessage(name, msg);
    return id;
}
