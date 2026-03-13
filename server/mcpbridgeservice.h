#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QProcess>

#include <functional>
#include <map>
#include <memory>

// ── McpBridgeService ──────────────────────────────────────────────────────────
//
// Manages MCP (Model Context Protocol) server processes inside ExoBridge.
// Multiple IDE instances share the same MCP server processes via IPC.
//
// Methods:
//   "start"    — Start an MCP server { name, command, args, env }
//   "stop"     — Stop an MCP server  { name }
//   "list"     — List running servers + tools
//   "callTool" — Call a tool on a server { tool, arguments, requestId }

class McpBridgeService : public QObject
{
    Q_OBJECT

public:
    explicit McpBridgeService(QObject *parent = nullptr);
    ~McpBridgeService() override;

    /// Handle a service/call dispatch from ExoBridgeCore.
    void handleCall(const QString &method,
                    const QJsonObject &args,
                    std::function<void(bool ok, const QJsonObject &result)> respond);

    /// Stop all managed MCP servers.
    void shutdown();

signals:
    /// Emitted when a server completes initialization + tool discovery.
    void serverReady(const QString &name);

    /// Emitted when a server disconnects unexpectedly.
    void serverLost(const QString &name);

private:
    struct ToolInfo {
        QString name;
        QString description;
        QJsonObject inputSchema;
        QString serverName;
    };

    struct ServerState {
        QString   name;
        QString   command;
        QStringList args;
        QHash<QString, QString> env;
        std::unique_ptr<QProcess> process;
        bool      initialized = false;
        int       nextMcpId   = 1;
        QByteArray buffer;
        QList<ToolInfo> tools;
        // Maps MCP request id → external callback
        QHash<int, std::function<void(bool, QJsonObject)>> pendingRequests;
    };

    void doStart(const QJsonObject &args,
                 std::function<void(bool, QJsonObject)> respond);
    void doStop(const QJsonObject &args,
                std::function<void(bool, QJsonObject)> respond);
    void doList(std::function<void(bool, QJsonObject)> respond);
    void doCallTool(const QJsonObject &args,
                    std::function<void(bool, QJsonObject)> respond);

    void onReadyRead(const QString &name);
    void onFinished(const QString &name, int exitCode);
    void handleMcpMessage(const QString &name, const QJsonObject &msg);
    void sendMcpRequest(const QString &name, const QString &method,
                        const QJsonObject &params,
                        std::function<void(bool, QJsonObject)> cb = {});
    void sendMcpMessage(const QString &name, const QJsonObject &msg);

    std::map<QString, std::unique_ptr<ServerState>> m_servers;
};
