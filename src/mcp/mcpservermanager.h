#pragma once

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
    QString command;        // for stdio servers
    QStringList args;
    QString url;            // for HTTP servers
    enum Transport { Stdio, Http } transport = Stdio;
    bool enabled = true;
    QJsonObject env;        // environment variables
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

    // Load configuration from .mcp.json or .vscode/mcp.json
    QList<MCPServerConfig> loadConfig(const QString &workspaceRoot) const
    {
        QList<MCPServerConfig> configs;
        const QStringList paths = {
            workspaceRoot + QStringLiteral("/.mcp.json"),
            workspaceRoot + QStringLiteral("/.vscode/mcp.json"),
            workspaceRoot + QStringLiteral("/.github/mcp.json"),
        };

        for (const auto &path : paths) {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) continue;
            const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
            const QJsonObject servers = root.value(QStringLiteral("servers")).toObject();
            for (auto it = servers.begin(); it != servers.end(); ++it) {
                MCPServerConfig cfg;
                cfg.name = it.key();
                const QJsonObject obj = it.value().toObject();
                cfg.command = obj.value(QStringLiteral("command")).toString();
                const QJsonArray argArr = obj.value(QStringLiteral("args")).toArray();
                for (const auto &a : argArr) cfg.args << a.toString();
                cfg.url = obj.value(QStringLiteral("url")).toString();
                cfg.transport = cfg.url.isEmpty() ? MCPServerConfig::Stdio : MCPServerConfig::Http;
                cfg.env = obj.value(QStringLiteral("env")).toObject();
                cfg.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
                configs.append(cfg);
            }
            break; // use first found
        }
        return configs;
    }

    // Start a stdio server
    bool startServer(const MCPServerConfig &config)
    {
        if (config.transport != MCPServerConfig::Stdio) return false;

        auto *proc = new QProcess(this);
        proc->setProgram(config.command);
        proc->setArguments(config.args);

        // Set environment variables
        if (!config.env.isEmpty()) {
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            for (auto it = config.env.begin(); it != config.env.end(); ++it)
                env.insert(it.key(), it.value().toString());
            proc->setProcessEnvironment(env);
        }

        proc->start();
        if (!proc->waitForStarted(5000)) {
            delete proc;
            emit serverError(config.name, tr("Failed to start: %1").arg(proc->errorString()));
            return false;
        }

        m_processes[config.name] = proc;
        emit serverStarted(config.name);

        connect(proc, &QProcess::readyReadStandardOutput, this, [this, name = config.name, proc]() {
            const QByteArray data = proc->readAllStandardOutput();
            processServerOutput(name, data);
        });
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, name = config.name](int code, QProcess::ExitStatus) {
            m_processes.remove(name);
            emit serverStopped(name, code);
        });

        // Send initialize
        sendToServer(config.name, QStringLiteral("initialize"), QJsonObject{
            {QStringLiteral("protocolVersion"), QStringLiteral("2024-11-05")},
            {QStringLiteral("capabilities"), QJsonObject{}},
            {QStringLiteral("clientInfo"), QJsonObject{
                {QStringLiteral("name"), QStringLiteral("exorcist-ide")},
                {QStringLiteral("version"), QStringLiteral("1.0.0")}
            }}
        });

        return true;
    }

    // Stop a running server
    void stopServer(const QString &name)
    {
        if (auto *proc = m_processes.value(name)) {
            proc->terminate();
            if (!proc->waitForFinished(3000))
                proc->kill();
        }
    }

    // Send a JSON-RPC message to a server
    void sendToServer(const QString &name, const QString &method,
                      const QJsonObject &params = {})
    {
        auto *proc = m_processes.value(name);
        if (!proc) return;

        QJsonObject msg;
        msg[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        msg[QStringLiteral("id")] = m_nextId++;
        msg[QStringLiteral("method")] = method;
        if (!params.isEmpty())
            msg[QStringLiteral("params")] = params;

        const QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact);
        const QByteArray frame = QStringLiteral("Content-Length: %1\r\n\r\n")
            .arg(data.size()).toUtf8() + data;
        proc->write(frame);
    }

    // Call a tool on a server
    void callTool(const QString &serverName, const QString &toolName,
                  const QJsonObject &args)
    {
        sendToServer(serverName, QStringLiteral("tools/call"), QJsonObject{
            {QStringLiteral("name"), toolName},
            {QStringLiteral("arguments"), args}
        });
    }

    // Request tool list from server
    void requestToolList(const QString &name)
    {
        sendToServer(name, QStringLiteral("tools/list"));
    }

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
    void processServerOutput(const QString &name, const QByteArray &data)
    {
        m_buffers[name] += data;

        // Simple Content-Length framing parser
        while (true) {
            const QByteArray &buf = m_buffers[name];
            const int headerEnd = buf.indexOf("\r\n\r\n");
            if (headerEnd < 0) break;

            const QByteArray header = buf.left(headerEnd);
            static const QByteArray clPrefix = "Content-Length: ";
            int clIdx = header.indexOf(clPrefix);
            if (clIdx < 0) break;

            bool ok;
            int len = header.mid(clIdx + clPrefix.size()).split('\r').first().toInt(&ok);
            if (!ok) break;

            int bodyStart = headerEnd + 4;
            if (buf.size() < bodyStart + len) break;

            const QByteArray body = buf.mid(bodyStart, len);
            m_buffers[name] = buf.mid(bodyStart + len);

            handleMessage(name, QJsonDocument::fromJson(body).object());
        }
    }

    void handleMessage(const QString &name, const QJsonObject &msg)
    {
        const QString method = msg.value(QStringLiteral("method")).toString();
        const QJsonObject result = msg.value(QStringLiteral("result")).toObject();

        // Tools list response
        if (result.contains(QStringLiteral("tools"))) {
            QList<MCPDiscoveredTool> tools;
            const QJsonArray arr = result.value(QStringLiteral("tools")).toArray();
            for (const auto &t : arr) {
                const QJsonObject obj = t.toObject();
                tools.append({name,
                    obj.value(QStringLiteral("name")).toString(),
                    obj.value(QStringLiteral("description")).toString(),
                    obj.value(QStringLiteral("inputSchema")).toObject()
                });
            }
            m_tools.append(tools);
            emit toolsDiscovered(name, tools);
        }

        // Tool call result
        if (result.contains(QStringLiteral("content"))) {
            emit toolResult(name, {}, result);
        }

        // After initialize, request tools
        if (result.contains(QStringLiteral("protocolVersion"))) {
            // Send initialized notification
            sendToServer(name, QStringLiteral("notifications/initialized"));
            requestToolList(name);
        }
    }

    QMap<QString, QProcess *> m_processes;
    QMap<QString, QByteArray> m_buffers;
    QList<MCPDiscoveredTool> m_tools;
    int m_nextId = 1;
};
