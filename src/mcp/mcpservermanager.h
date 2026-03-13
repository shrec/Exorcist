#pragma once

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

// ─────────────────────────────────────────────────────────────────────────────
// MCPServerManager — manages connections to MCP servers via stdio or HTTP.
//
// Reads .mcp.json configuration, starts stdio servers as child processes,
// discovers tools via initialize → tools/list protocol, and routes
// tool calls to the appropriate server.
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
    explicit MCPServerManager(QObject *parent = nullptr) : QObject(parent) {}

    QList<MCPServerConfig> loadConfig(const QString &workspaceRoot) const;
    bool startServer(const MCPServerConfig &config);
    void stopServer(const QString &name);
    void sendToServer(const QString &name, const QString &method,
                      const QJsonObject &params = {});
    void callTool(const QString &serverName, const QString &toolName,
                  const QJsonObject &args);
    void requestToolList(const QString &name);

    QStringList runningServers() const { return m_processes.keys(); }
    QList<MCPDiscoveredTool> discoveredTools() const { return m_tools; }

signals:
    void serverStarted(const QString &name);
    void serverStopped(const QString &name, int exitCode);
    void serverError(const QString &name, const QString &error);
    void toolsDiscovered(const QString &serverName, const QList<MCPDiscoveredTool> &tools);
    void toolResult(const QString &serverName, const QString &toolName,
                    const QJsonObject &result);

private:
    void processServerOutput(const QString &name, const QByteArray &data);
    void handleMessage(const QString &name, const QJsonObject &msg);

    QMap<QString, QProcess *> m_processes;
    QMap<QString, QByteArray> m_buffers;
    QList<MCPDiscoveredTool> m_tools;
    int m_nextId = 1;
};
