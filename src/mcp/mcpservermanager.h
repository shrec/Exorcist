#pragma once

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QString>
#include <QStringList>

// ─────────────────────────────────────────────────────────────────────────────
// MCPServerManager — manages connections to MCP servers via stdio or HTTP.
//
// Reads .mcp.json configuration, starts stdio servers as child processes,
// discovers tools via initialize → tools/list protocol, and routes
// tool calls to the appropriate server.
//
// Security: workspace-level .mcp.json configs require explicit user trust
// before servers are started. Trust decisions are persisted per workspace.
// ─────────────────────────────────────────────────────────────────────────────

struct MCPServerConfig {
    QString name;
    QString command;
    QStringList args;
    QString url;
    enum Transport { Stdio, Http } transport = Stdio;
    bool enabled = true;
    QJsonObject env;
};

struct MCPDiscoveredTool {
    QString serverName;
    QString name;
    QString description;
    QJsonObject inputSchema;
};

class MCPServerManager : public QObject
{
    Q_OBJECT

public:
    explicit MCPServerManager(QObject *parent = nullptr);

    QList<MCPServerConfig> loadConfig(const QString &workspaceRoot) const;

    /// Start a server only if the workspace is trusted. Emits
    /// trustRequired() if the workspace has not been approved yet.
    bool startServer(const MCPServerConfig &config);

    void stopServer(const QString &name);
    void sendToServer(const QString &name, const QString &method,
                      const QJsonObject &params = {});
    void callTool(const QString &serverName, const QString &toolName,
                  const QJsonObject &args);
    void requestToolList(const QString &name);

    QStringList runningServers() const { return m_processes.keys(); }
    QList<MCPDiscoveredTool> discoveredTools() const { return m_tools; }

    // ── Workspace trust ──────────────────────────────────────────────────

    /// Set the current workspace root (required for trust checks).
    void setWorkspaceRoot(const QString &root);

    /// Check whether the current workspace is trusted for MCP servers.
    bool isWorkspaceTrusted() const;

    /// Mark the current workspace as trusted (persisted in QSettings).
    void trustWorkspace();

    /// Revoke trust for the current workspace.
    void untrustWorkspace();

signals:
    void serverStarted(const QString &name);
    void serverStopped(const QString &name, int exitCode);
    void serverError(const QString &name, const QString &error);
    void toolsDiscovered(const QString &serverName, const QList<MCPDiscoveredTool> &tools);
    void toolResult(const QString &serverName, const QString &toolName,
                    const QJsonObject &result);

    /// Emitted when a server start is blocked because the workspace
    /// is not trusted. The UI should prompt the user and call
    /// trustWorkspace() if approved, then retry starting the server.
    void trustRequired(const QString &workspaceRoot,
                       const QList<MCPServerConfig> &pendingServers);

private:
    void processServerOutput(const QString &name, const QByteArray &data);
    void handleMessage(const QString &name, const QJsonObject &msg);
    bool startServerInternal(const MCPServerConfig &config);

    QMap<QString, QProcess *> m_processes;
    QMap<QString, QByteArray> m_buffers;
    QList<MCPDiscoveredTool> m_tools;
    int m_nextId = 1;

    QString m_workspaceRoot;
    QSet<QString> m_trustedWorkspaces;
    void loadTrustedWorkspaces();
    void saveTrustedWorkspaces() const;
};
