#include "mcpbridgeservice.h"

#include <QJsonDocument>
#include <QProcessEnvironment>

McpBridgeService::McpBridgeService(QObject *parent)
    : QObject(parent)
{
}

McpBridgeService::~McpBridgeService()
{
    shutdown();
}

void McpBridgeService::handleCall(
    const QString &method,
    const QJsonObject &args,
    std::function<void(bool ok, const QJsonObject &result)> respond)
{
    if (method == QLatin1String("start"))
        doStart(args, std::move(respond));
    else if (method == QLatin1String("stop"))
        doStop(args, std::move(respond));
    else if (method == QLatin1String("list"))
        doList(std::move(respond));
    else if (method == QLatin1String("callTool"))
        doCallTool(args, std::move(respond));
    else {
        QJsonObject err;
        err[QLatin1String("message")] =
            QStringLiteral("Unknown mcp method: %1").arg(method);
        respond(false, err);
    }
}

void McpBridgeService::shutdown()
{
    for (auto &[name, state] : m_servers) {
        if (state->process) {
            state->process->kill();
            state->process->waitForFinished(3000);
        }
    }
    m_servers.clear();
}

// ── Method implementations ───────────────────────────────────────────────────

void McpBridgeService::doStart(
    const QJsonObject &args,
    std::function<void(bool, QJsonObject)> respond)
{
    const QString name    = args.value(QLatin1String("name")).toString();
    const QString command = args.value(QLatin1String("command")).toString();
    const QJsonArray argsArr = args.value(QLatin1String("args")).toArray();
    const QJsonObject envObj = args.value(QLatin1String("env")).toObject();

    if (name.isEmpty() || command.isEmpty()) {
        QJsonObject err;
        err[QLatin1String("message")] = QStringLiteral("Missing 'name' or 'command'");
        respond(false, err);
        return;
    }

    // Already running?
    if (m_servers.count(name) > 0) {
        auto &state = m_servers[name];
        if (state->initialized) {
            QJsonObject result;
            result[QLatin1String("name")]   = name;
            result[QLatin1String("status")] = QStringLiteral("already_running");
            QJsonArray toolArr;
            for (const auto &t : state->tools) {
                QJsonObject to;
                to[QLatin1String("name")]        = t.name;
                to[QLatin1String("description")] = t.description;
                toolArr.append(to);
            }
            result[QLatin1String("tools")] = toolArr;
            respond(true, result);
            return;
        }
    }

    auto state = std::make_unique<ServerState>();
    state->name    = name;
    state->command = command;

    for (const auto &a : argsArr)
        state->args << a.toString();

    for (auto it = envObj.begin(); it != envObj.end(); ++it)
        state->env.insert(it.key(), it.value().toString());

    state->process = std::make_unique<QProcess>();
    state->process->setProgram(command);
    state->process->setArguments(state->args);

    if (!state->env.isEmpty()) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        for (auto it = state->env.begin(); it != state->env.end(); ++it)
            env.insert(it.key(), it.value());
        state->process->setProcessEnvironment(env);
    }

    auto *raw = state->process.get();
    connect(raw, &QProcess::readyReadStandardOutput, this,
            [this, name]() { onReadyRead(name); });
    connect(raw, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, name](int exitCode, QProcess::ExitStatus) {
                onFinished(name, exitCode);
            });

    raw->start(QIODevice::ReadWrite);
    if (!raw->waitForStarted(10000)) {
        QJsonObject err;
        err[QLatin1String("message")] =
            QStringLiteral("Failed to start MCP server '%1': %2")
                .arg(name, raw->errorString());
        respond(false, err);
        return;
    }

    // Store the respond callback — it will be called when initialization
    // completes (after tool discovery) or fails.
    state->pendingRequests.insert(
        0, // use id=0 as the "start" completion marker
        [respond](bool ok, const QJsonObject &result) { respond(ok, result); });

    m_servers[name] = std::move(state);

    // Send MCP initialize request
    QJsonObject initParams;
    initParams[QLatin1String("protocolVersion")] = QLatin1String("2024-11-05");
    QJsonObject clientInfo;
    clientInfo[QLatin1String("name")]    = QLatin1String("exobridge");
    clientInfo[QLatin1String("version")] = QLatin1String("1.0.0");
    initParams[QLatin1String("clientInfo")]   = clientInfo;
    initParams[QLatin1String("capabilities")] = QJsonObject{};

    sendMcpRequest(name, QStringLiteral("initialize"), initParams);
}

void McpBridgeService::doStop(
    const QJsonObject &args,
    std::function<void(bool, QJsonObject)> respond)
{
    const QString name = args.value(QLatin1String("name")).toString();
    auto it = m_servers.find(name);
    if (it == m_servers.end()) {
        QJsonObject err;
        err[QLatin1String("message")] =
            QStringLiteral("MCP server '%1' not found").arg(name);
        respond(false, err);
        return;
    }

    if (it->second->process) {
        it->second->process->kill();
        it->second->process->waitForFinished(3000);
    }
    m_servers.erase(it);

    QJsonObject result;
    result[QLatin1String("name")]   = name;
    result[QLatin1String("status")] = QStringLiteral("stopped");
    respond(true, result);
}

void McpBridgeService::doList(
    std::function<void(bool, QJsonObject)> respond)
{
    QJsonArray servers;
    for (auto it = m_servers.begin(); it != m_servers.end(); ++it) {
        QJsonObject s;
        s[QLatin1String("name")]        = it->first;
        s[QLatin1String("initialized")] = it->second->initialized;
        QJsonArray toolArr;
        for (const auto &t : it->second->tools) {
            QJsonObject to;
            to[QLatin1String("name")]        = t.name;
            to[QLatin1String("description")] = t.description;
            to[QLatin1String("inputSchema")] = t.inputSchema;
            to[QLatin1String("serverName")]  = t.serverName;
            toolArr.append(to);
        }
        s[QLatin1String("tools")] = toolArr;
        servers.append(s);
    }

    QJsonObject result;
    result[QLatin1String("servers")] = servers;
    respond(true, result);
}

void McpBridgeService::doCallTool(
    const QJsonObject &args,
    std::function<void(bool, QJsonObject)> respond)
{
    const QString toolName = args.value(QLatin1String("tool")).toString();
    const QJsonObject toolArgs = args.value(QLatin1String("arguments")).toObject();

    // Find which server owns this tool
    for (auto it = m_servers.begin(); it != m_servers.end(); ++it) {
        if (!it->second->initialized) continue;
        for (const auto &tool : it->second->tools) {
            if (tool.name == toolName) {
                QJsonObject mcpParams;
                mcpParams[QLatin1String("name")]      = toolName;
                mcpParams[QLatin1String("arguments")] = toolArgs;
                sendMcpRequest(it->first, QStringLiteral("tools/call"),
                               mcpParams, std::move(respond));
                return;
            }
        }
    }

    QJsonObject err;
    err[QLatin1String("message")] =
        QStringLiteral("Tool '%1' not found on any MCP server").arg(toolName);
    respond(false, err);
}

// ── MCP protocol handling ────────────────────────────────────────────────────

void McpBridgeService::onReadyRead(const QString &name)
{
    auto it = m_servers.find(name);
    if (it == m_servers.end()) return;
    auto &state = *it->second;

    state.buffer.append(state.process->readAllStandardOutput());

    // MCP uses newline-delimited JSON-RPC
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
        handleMcpMessage(name, doc.object());
    }
    if (consumed > 0)
        state.buffer.remove(0, consumed);
}

void McpBridgeService::onFinished(const QString &name, int exitCode)
{
    Q_UNUSED(exitCode)
    auto it = m_servers.find(name);
    if (it == m_servers.end()) return;

    // Fail any pending requests
    for (auto &cb : it->second->pendingRequests)
        if (cb) {
            QJsonObject err;
            err[QLatin1String("message")] =
                QStringLiteral("MCP server '%1' exited").arg(name);
            cb(false, err);
        }

    m_servers.erase(it);
    emit serverLost(name);
}

void McpBridgeService::handleMcpMessage(const QString &name,
                                         const QJsonObject &msg)
{
    auto it = m_servers.find(name);
    if (it == m_servers.end()) return;
    auto &state = *it->second;

    // Response (has "id")
    if (msg.contains(QLatin1String("id"))) {
        const int msgId = msg[QLatin1String("id")].toInt();
        const QJsonObject result = msg[QLatin1String("result")].toObject();
        const QJsonObject error  = msg[QLatin1String("error")].toObject();

        // Initialize response?
        if (!state.initialized
            && result.contains(QLatin1String("protocolVersion"))) {
            state.initialized = true;

            // Send initialized notification
            QJsonObject notif;
            notif[QLatin1String("jsonrpc")] = QLatin1String("2.0");
            notif[QLatin1String("method")]  = QLatin1String("notifications/initialized");
            sendMcpMessage(name, notif);

            // Request tool list
            sendMcpRequest(name, QStringLiteral("tools/list"), QJsonObject{},
                           [this, name](bool ok, const QJsonObject &toolResult) {
                if (!ok) return;
                auto sit = m_servers.find(name);
                if (sit == m_servers.end()) return;
                auto &st = *sit->second;

                const QJsonArray toolsArr =
                    toolResult[QLatin1String("tools")].toArray();
                st.tools.clear();
                for (const auto &v : toolsArr) {
                    const QJsonObject tObj = v.toObject();
                    ToolInfo info;
                    info.name        = tObj[QLatin1String("name")].toString();
                    info.description = tObj[QLatin1String("description")].toString();
                    info.inputSchema = tObj[QLatin1String("inputSchema")].toObject();
                    info.serverName  = name;
                    st.tools.append(info);
                }

                emit serverReady(name);

                // Complete the "start" callback (id 0)
                auto startCb = st.pendingRequests.take(0);
                if (startCb) {
                    QJsonObject res;
                    res[QLatin1String("name")]   = name;
                    res[QLatin1String("status")] = QStringLiteral("ready");
                    QJsonArray arr;
                    for (const auto &t : st.tools) {
                        QJsonObject to;
                        to[QLatin1String("name")]        = t.name;
                        to[QLatin1String("description")] = t.description;
                        arr.append(to);
                    }
                    res[QLatin1String("tools")] = arr;
                    startCb(true, res);
                }
            });
            return;
        }

        // Tool-call or tools/list response
        auto cbIt = state.pendingRequests.find(msgId);
        if (cbIt != state.pendingRequests.end()) {
            auto cb = std::move(cbIt.value());
            state.pendingRequests.erase(cbIt);
            if (!error.isEmpty()) {
                QJsonObject errResult;
                errResult[QLatin1String("message")] =
                    error[QLatin1String("message")].toString();
                cb(false, errResult);
            } else {
                cb(true, result);
            }
        }
    }
    // Server-initiated notifications — ignored for now
}

void McpBridgeService::sendMcpRequest(
    const QString &name,
    const QString &method,
    const QJsonObject &params,
    std::function<void(bool, QJsonObject)> cb)
{
    auto it = m_servers.find(name);
    if (it == m_servers.end()) return;
    auto &state = *it->second;

    const int id = state.nextMcpId++;
    if (cb)
        state.pendingRequests.insert(id, std::move(cb));

    QJsonObject msg;
    msg[QLatin1String("jsonrpc")] = QLatin1String("2.0");
    msg[QLatin1String("id")]      = id;
    msg[QLatin1String("method")]  = method;
    if (!params.isEmpty())
        msg[QLatin1String("params")] = params;

    sendMcpMessage(name, msg);
}

void McpBridgeService::sendMcpMessage(const QString &name,
                                       const QJsonObject &msg)
{
    auto it = m_servers.find(name);
    if (it == m_servers.end()) return;
    auto &state = *it->second;
    if (!state.process) return;

    const QByteArray data =
        QJsonDocument(msg).toJson(QJsonDocument::Compact) + '\n';
    state.process->write(data);
}
