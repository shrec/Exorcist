#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QProcess>

class BridgeClient;

/// MCP Tool descriptor discovered from a server.
struct McpToolInfo {
    QString name;
    QString description;
    QJsonObject inputSchema;   // JSON Schema for the tool's parameters
    QString serverName;        // which MCP server exposes this tool
};

/// MCP Server configuration.
struct McpServerConfig {
    QString name;           // user-friendly name
    QString command;        // executable (e.g., "npx", "python")
    QStringList args;       // arguments (e.g., ["-y", "@mcp/server"])
    QHash<QString, QString> env; // optional environment variables
};

/// Result of calling an MCP tool.
struct McpToolResult {
    bool ok = false;
    QString text;           // text content from the tool
    QJsonArray content;     // raw content array from MCP response
    QString error;
};

/// Client for the Model Context Protocol (MCP).
/// Supports stdio-based MCP servers using JSON-RPC 2.0.
class McpClient : public QObject
{
    Q_OBJECT

public:
    explicit McpClient(QObject *parent = nullptr);
    ~McpClient() override;

    /// Add a server configuration without connecting.
    void addServer(const McpServerConfig &config);

    /// Connect to all configured servers.
    void connectAll();

    /// Connect to a specific server by name.
    bool connectServer(const QString &name);

    /// Disconnect a specific server.
    void disconnectServer(const QString &name);

    /// Disconnect all servers.
    void disconnectAll();

    /// All discovered tools across all connected servers.
    QList<McpToolInfo> allTools() const;

    /// Call a tool on its originating server.
    void callTool(const QString &toolName, const QJsonObject &arguments,
                  const QString &requestId);

    /// List connected server names.
    QStringList connectedServers() const;

    /// Check if a server is connected and initialized.
    bool isConnected(const QString &name) const;

    /// Set a BridgeClient for shared MCP server delegation.
    /// When set, connectServer() delegates to ExoBridge instead of
    /// spawning local processes.
    void setBridgeClient(BridgeClient *client);

    /// Whether bridge delegation is active.
    bool useBridge() const { return m_bridgeClient != nullptr; }

signals:
    void serverConnected(const QString &name);
    void serverDisconnected(const QString &name);
    void serverError(const QString &name, const QString &error);
    void toolsDiscovered(const QString &serverName, const QList<McpToolInfo> &tools);
    void toolCallFinished(const QString &requestId, const McpToolResult &result);

private:
    struct ServerState {
        McpServerConfig config;
        QProcess *process = nullptr;
        bool initialized = false;
        int nextId = 1;
        QByteArray buffer;
        QList<McpToolInfo> tools;
        QHash<int, QString> pendingRequests; // msgId → external requestId
    };

    void onProcessReadyRead(const QString &name);
    void onProcessFinished(const QString &name, int exitCode);
    void handleMessage(const QString &name, const QJsonObject &msg);
    void sendMessage(const QString &name, const QJsonObject &msg);
    int sendRequest(const QString &name, const QString &method,
                    const QJsonObject &params, const QString &externalId = {});

    // Bridge delegation helpers
    bool connectServerViaBridge(const QString &name);
    void callToolViaBridge(const QString &toolName, const QJsonObject &arguments,
                           const QString &requestId);

    QHash<QString, ServerState> m_servers;
    BridgeClient *m_bridgeClient = nullptr;
};
