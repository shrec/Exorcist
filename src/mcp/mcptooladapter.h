#pragma once

#include "../agent/itool.h"
#include "mcpclient.h"

#include <QEventLoop>

/// Adapter that wraps an MCP remote tool as an ITool so it can be used
/// by the AgentController / ToolRegistry just like built-in tools.
class McpToolAdapter : public ITool
{
public:
    McpToolAdapter(McpClient *client, const McpToolInfo &info)
        : m_client(client), m_info(info) {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("mcp_%1_%2").arg(m_info.serverName, m_info.name);
        s.description = m_info.description;
        s.inputSchema = m_info.inputSchema;
        s.permission  = AgentToolPermission::Dangerous; // external tools get highest permission level
        s.timeoutMs   = 60000;
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        // Synchronous call using event loop (tools run on background threads)
        QEventLoop loop;
        McpToolResult mcpResult;
        const QString reqId = QStringLiteral("mcp-call-%1").arg(quintptr(this));

        auto conn = QObject::connect(m_client, &McpClient::toolCallFinished,
                                     [&](const QString &id, const McpToolResult &res) {
            if (id == reqId) {
                mcpResult = res;
                loop.quit();
            }
        });

        m_client->callTool(m_info.name, args, reqId);
        loop.exec();
        QObject::disconnect(conn);

        ToolExecResult result;
        result.ok = mcpResult.ok;
        result.textContent = mcpResult.text;
        result.error = mcpResult.error;
        if (!mcpResult.content.isEmpty()) {
            QJsonObject data;
            data[QLatin1String("content")] = mcpResult.content;
            result.data = data;
        }
        return result;
    }

private:
    McpClient   *m_client;
    McpToolInfo  m_info;
};
